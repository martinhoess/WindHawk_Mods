// ==WindhawkMod==
// @id              right-click-ring
// @name            Right-Click Ring
// @description     Holding the right mouse button opens a Logitech-style radial overlay at the cursor. The 8 slices show icons and run actions (clipboard/edit hotkeys, app launch, native menu); the center hub names the hovered slice and falls through to the app's own context menu. A quick right-click still shows it directly. A right-drag, and any app on the denylist, pass through untouched.
// @version         0.5
// @author          martinhoess
// @github          https://github.com/martinhoess
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -ld2d1 -ldwrite -lgdi32 -luser32 -lole32 -lshcore -lshell32
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*
- enabled: true
  $name: Enabled
  $description: Master switch. Turn off to stop intercepting right-clicks without uninstalling.
- holdMs: 250
  $name: Hold threshold (ms)
  $description: "How long the right button must be held before the ring opens. A shorter press passes through as a normal right-click — but normal right-clicks are delayed by up to this many ms while the mod decides. 200-300 feels natural."
- denyList: ""
  $name: App denylist
  $description: "Semicolon-separated process names (e.g. game.exe;mstsc.exe). While one of these is the foreground app the ring is disabled and right-click behaves completely normally. Useful for games, remote desktop, or any app whose own right-click you want untouched."
*/
// ==/WindhawkModSettings==

// MVP NOTES
// ---------
// Goal of this version: prove the plumbing works end-to-end — global mouse
// hook fires, a translucent radial overlay renders at the cursor with true
// per-pixel alpha (black/white, Logitech-ish), hover tracking follows the
// mouse, and a release reports which slice was chosen (via Wh_Log).
//
// The 8 outer slices now dispatch real actions (see g_slices): clipboard/edit
// hotkeys, an app launch, and a native-menu fall-through. The center "Win" hub
// re-issues a real right-click at the original spot so the app shows its own
// context menu. The slice set is a hard-coded placeholder for now — it becomes
// user-configurable in a later milestone. The ONE behaviour change you'll
// feel: every right-click system-wide now waits up to `holdMs` (hold = ring,
// tap = native menu).
//
// Architecture mirrors the emoji-picker mod: a single worker thread owns the
// D2D resources, the overlay window, the WH_MOUSE_LL hook, and a message loop.
// Because LL-hook callbacks AND the hold timer are both delivered to that one
// thread's message queue, the state machine needs no locking — only the
// settings (read in the hook, written in Wh_ModSettingsChanged) are atomic.

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <shellscalingapi.h>
#include <atomic>
#include <vector>
#include <string>
#include <cmath>

// ============================================================
// Constants
// ============================================================

static const wchar_t* RING_WNDCLASS = L"WhRightClickRing";

constexpr float  RING_DIAM_DIP   = 320.0f; // overlay diameter, device-independent px
constexpr float  HUB_RADIUS_DIP  = 36.0f;  // central hub radius
constexpr float  BUBBLE_R_DIP    = 30.0f;  // each action bubble radius
constexpr float  BUBBLE_RING_DIP = 104.0f; // distance center -> bubble center
constexpr int    SEG_COUNT = 8;
constexpr float  PI = 3.14159265358979323846f;

constexpr UINT_PTR TIMER_HOLD = 1;

// Posted from the hook to the worker thread to run a selected action off the
// hook callback. wParam: slice index 0..SEG_COUNT-1, or SEG_COUNT for the hub.
constexpr UINT WM_RING_ACTION = WM_APP + 1;

// Marks our own synthesized right-clicks so the hook ignores them (no re-entry).
constexpr ULONG_PTR RING_SENTINEL = 0x52494E47;  // 'RING'

// Hover sentinels
constexpr int HOVER_NONE = -1;
constexpr int HOVER_HUB  = -2;

// Action a slice (or the hub) performs when selected.
enum ActType { AT_HOTKEY, AT_LAUNCH, AT_NATIVE };

// Modifier bitmask for AT_HOTKEY. Custom RMOD_* names avoid clashing with the
// MOD_* macros from WinUser.h (RegisterHotKey).
constexpr WORD RMOD_CTRL  = 0x01;
constexpr WORD RMOD_ALT   = 0x02;
constexpr WORD RMOD_SHIFT = 0x04;
constexpr WORD RMOD_WIN   = 0x08;

struct SliceAction {
    const wchar_t* label;
    const wchar_t* icon;    // Segoe Fluent Icons glyph (PUA codepoint)
    ActType        type;
    WORD           mods;    // AT_HOTKEY: modifier bitmask
    WORD           vk;      // AT_HOTKEY: virtual key
    const wchar_t* launch;  // AT_LAUNCH: ShellExecute target
};

// The 8 outer slices, clockwise from 12 o'clock. Hard-coded placeholder set —
// becomes user-configurable later. Exercises all three action types. Icons are
// Segoe Fluent Icons glyphs (shipped on Windows 11).
static const SliceAction g_slices[SEG_COUNT] = {
    {L"Copy",     L"", AT_HOTKEY, RMOD_CTRL, 'C', nullptr},
    {L"Paste",    L"", AT_HOTKEY, RMOD_CTRL, 'V', nullptr},
    {L"Cut",      L"", AT_HOTKEY, RMOD_CTRL, 'X', nullptr},
    {L"Undo",     L"", AT_HOTKEY, RMOD_CTRL, 'Z', nullptr},
    {L"Redo",     L"", AT_HOTKEY, RMOD_CTRL, 'Y', nullptr},
    {L"Find",     L"", AT_HOTKEY, RMOD_CTRL, 'F', nullptr},
    {L"Explorer", L"", AT_LAUNCH, 0,         0,   L"explorer.exe"},
    {L"More",     L"", AT_NATIVE, 0,         0,   nullptr},
};

// ============================================================
// State
// ============================================================

enum RingState { ST_IDLE, ST_PENDING, ST_OPEN, ST_PASSTHRU };

static DWORD  g_threadId = 0;
static HANDLE g_thread   = nullptr;
static HANDLE g_hookReady = nullptr;
static HHOOK  g_mouseHook = nullptr;
static HWND   g_hwnd      = nullptr;

// Worker-thread-only (no locking needed — see header note)
static RingState g_state   = ST_IDLE;
static POINT     g_downPt  = {0, 0};   // where the right button went down (screen px)
static POINT     g_centerPt = {0, 0};  // ring center on screen (== g_downPt while open)
static POINT     g_actionPt = {0, 0};  // target captured at selection for a deferred action
static int       g_hover   = HOVER_NONE;
static float     g_scale     = 1.0f;
static float     g_outerR    = 0.0f;   // physical px
static float     g_hubR      = 0.0f;   // physical px

// Settings (read from hook thread, written from Windhawk thread)
static std::atomic<bool> g_enabled{true};
static std::atomic<int>  g_holdMs{250};
static int g_dragSlop = 8;  // px; movement during PENDING beyond this = drag, not hold

// Denylist of foreground process names. Checked on the hook thread, written by
// ReadSettings on the Windhawk thread (guarded by g_denyCs).
static std::vector<std::wstring> g_denyList;
static CRITICAL_SECTION          g_denyCs;
static bool                      g_denyCsInit = false;

// D2D / DWrite
static ID2D1Factory*       g_d2dFact = nullptr;
static IDWriteFactory*     g_dwFact  = nullptr;
static ID2D1DCRenderTarget* g_dcrt   = nullptr;
static ID2D1SolidColorBrush* g_brush = nullptr;
static IDWriteTextFormat*  g_textFmt = nullptr;
static IDWriteTextFormat*  g_iconFmt = nullptr;
static float               g_textScale = 0.0f;

// GDI layered-window backing surface
static HDC     g_memDC  = nullptr;
static HBITMAP g_dib    = nullptr;
static HBITMAP g_oldBmp = nullptr;
static int     g_surfW  = 0;
static int     g_surfH  = 0;

template <class T>
static void SafeRelease(T** pp) {
    if (*pp) { (*pp)->Release(); *pp = nullptr; }
}

// ============================================================
// Geometry helpers
// ============================================================

// Angle measured in degrees from 12 o'clock, increasing clockwise.
static D2D1_POINT_2F PointOnCircle(float cx, float cy, float r, float degFromTop) {
    float rad = degFromTop * (PI / 180.0f);
    return D2D1::Point2F(cx + r * sinf(rad), cy - r * cosf(rad));
}

// Which slice (or hub / none) is under a screen point, relative to ring center.
static int HitTest(POINT pt) {
    float dx = (float)(pt.x - g_centerPt.x);
    float dy = (float)(pt.y - g_centerPt.y);
    float d = sqrtf(dx * dx + dy * dy);
    if (d < g_hubR)   return HOVER_HUB;
    if (d > g_outerR) return HOVER_NONE;
    // atan2(dx, -dy): top -> 0 deg, right -> 90 deg, clockwise.
    float deg = atan2f(dx, -dy) * (180.0f / PI);
    float norm = fmodf(deg + 360.0f, 360.0f);
    int seg = (int)((norm + 22.5f) / 45.0f) % SEG_COUNT;
    return seg;
}

// ============================================================
// Rendering
// ============================================================

// (Re)create the device-independent text + icon fonts for a given DPI scale.
static void CreateFonts(float scale) {
    if (!g_dwFact) return;
    SafeRelease(&g_textFmt);
    if (SUCCEEDED(g_dwFact->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            13.0f * scale, L"", &g_textFmt))) {
        g_textFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_textFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    SafeRelease(&g_iconFmt);
    if (SUCCEEDED(g_dwFact->CreateTextFormat(
            L"Segoe Fluent Icons", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            22.0f * scale, L"", &g_iconFmt))) {
        g_iconFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_iconFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    g_textScale = scale;
}

// (Re)create the DIB + DCRenderTarget + brush when the required size changes,
// and refresh fonts when the DPI scale changes. Returns false on failure.
static bool EnsureSurface(int w, int h, float scale) {
    if (g_dcrt && g_surfW == w && g_surfH == h) {
        if (g_textScale != scale) CreateFonts(scale);
        return g_dcrt != nullptr;
    }

    // Teardown old surface
    SafeRelease(&g_brush);
    SafeRelease(&g_dcrt);
    if (g_memDC) {
        if (g_oldBmp) SelectObject(g_memDC, g_oldBmp);
        DeleteDC(g_memDC);
        g_memDC = nullptr;
        g_oldBmp = nullptr;
    }
    if (g_dib) { DeleteObject(g_dib); g_dib = nullptr; }

    // 32-bit top-down DIB for the layered window
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    g_dib = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!g_dib) return false;
    g_memDC = CreateCompatibleDC(nullptr);
    if (!g_memDC) { DeleteObject(g_dib); g_dib = nullptr; return false; }
    g_oldBmp = (HBITMAP)SelectObject(g_memDC, g_dib);

    // DC render target bound per-frame to g_memDC
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);
    if (FAILED(g_d2dFact->CreateDCRenderTarget(&props, &g_dcrt)) || !g_dcrt)
        return false;
    g_dcrt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &g_brush);

    g_surfW = w;
    g_surfH = h;
    CreateFonts(scale);

    return g_brush != nullptr;
}

// Render the ring into g_memDC and blit it to the layered window. Logitech-
// style: discrete floating bubbles on a transparent backdrop, the hovered one
// solid + slightly enlarged, a central hub that names the hovered action.
static void RenderRing() {
    if (!g_dcrt || !g_brush) return;

    RECT rc = {0, 0, g_surfW, g_surfH};
    if (FAILED(g_dcrt->BindDC(g_memDC, &rc))) return;

    g_dcrt->BeginDraw();
    g_dcrt->Clear(D2D1::ColorF(0, 0.0f));  // fully transparent

    float cx = g_surfW / 2.0f;
    float cy = g_surfH / 2.0f;
    float hubR    = g_hubR;
    float bubbleR = BUBBLE_R_DIP * g_scale;
    float ringR   = BUBBLE_RING_DIP * g_scale;

    // Action bubbles
    for (int i = 0; i < SEG_COUNT; ++i) {
        D2D1_POINT_2F bc = PointOnCircle(cx, cy, ringR, i * 45.0f);
        bool hot = (i == g_hover);
        float r = hot ? bubbleR * 1.08f : bubbleR;
        D2D1_ELLIPSE e = D2D1::Ellipse(bc, r, r);

        g_brush->SetColor(hot ? D2D1::ColorF(0, 0, 0, 0.92f)
                              : D2D1::ColorF(0, 0, 0, 0.55f));
        g_dcrt->FillEllipse(e, g_brush);
        g_brush->SetColor(D2D1::ColorF(1, 1, 1, hot ? 0.55f : 0.16f));
        g_dcrt->DrawEllipse(e, g_brush, (hot ? 1.6f : 1.0f) * g_scale);

        if (g_iconFmt) {
            float ih = 22.0f * g_scale;
            D2D1_RECT_F ir = D2D1::RectF(bc.x - ih, bc.y - ih, bc.x + ih, bc.y + ih);
            g_brush->SetColor(D2D1::ColorF(1, 1, 1, hot ? 1.0f : 0.85f));
            g_dcrt->DrawTextW(g_slices[i].icon, (UINT32)wcslen(g_slices[i].icon),
                              g_iconFmt, ir, g_brush);
        }
    }

    // Center hub
    bool hubHot = (g_hover == HOVER_HUB);
    D2D1_ELLIPSE hub = D2D1::Ellipse(D2D1::Point2F(cx, cy), hubR, hubR);
    g_brush->SetColor(hubHot ? D2D1::ColorF(0, 0, 0, 0.92f)
                             : D2D1::ColorF(0, 0, 0, 0.70f));
    g_dcrt->FillEllipse(hub, g_brush);
    g_brush->SetColor(D2D1::ColorF(1, 1, 1, hubHot ? 0.55f : 0.22f));
    g_dcrt->DrawEllipse(hub, g_brush, 1.2f * g_scale);

    if (g_hover >= 0 && g_hover < SEG_COUNT && g_textFmt) {
        // Name the hovered action so the icons stay discoverable.
        D2D1_RECT_F r = D2D1::RectF(cx - hubR + 2, cy - hubR, cx + hubR - 2, cy + hubR);
        g_brush->SetColor(D2D1::ColorF(1, 1, 1, 0.95f));
        g_dcrt->DrawTextW(g_slices[g_hover].label, (UINT32)wcslen(g_slices[g_hover].label),
                          g_textFmt, r, g_brush);
    } else if (g_iconFmt) {
        // Idle / hub hovered: a menu glyph hints "native context menu".
        D2D1_RECT_F r = D2D1::RectF(cx - hubR, cy - hubR, cx + hubR, cy + hubR);
        g_brush->SetColor(D2D1::ColorF(1, 1, 1, 0.95f));
        g_dcrt->DrawTextW(L"", 1, g_iconFmt, r, g_brush);
    }

    if (g_dcrt->EndDraw() == (HRESULT)D2DERR_RECREATE_TARGET) {
        // Device lost — drop the surface; next ShowRing rebuilds it.
        SafeRelease(&g_brush);
        SafeRelease(&g_dcrt);
        g_surfW = g_surfH = 0;
        return;
    }

    // Blit to the layered window (positions + composites with per-pixel alpha)
    SIZE size = {g_surfW, g_surfH};
    POINT src = {0, 0};
    POINT dst = {g_centerPt.x - g_surfW / 2, g_centerPt.y - g_surfH / 2};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(g_hwnd, screen, &dst, &size, g_memDC, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

static void ShowRing(POINT pt) {
    UINT dx = 96, dy = 96;
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (mon) GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    float scale = dx / 96.0f;
    int diam = (int)(RING_DIAM_DIP * scale + 0.5f);

    if (!EnsureSurface(diam, diam, scale)) {
        Wh_Log(L"ShowRing: EnsureSurface failed");
        g_state = ST_IDLE;
        return;
    }

    g_centerPt = pt;
    g_scale = scale;
    g_outerR = diam / 2.0f - 2.0f * scale;
    g_hubR = HUB_RADIUS_DIP * scale;
    g_hover = HitTest(pt);  // cursor is at center, so usually HUB

    ShowWindow(g_hwnd, SW_SHOWNA);
    RenderRing();
}

static void HideRing() {
    ShowWindow(g_hwnd, SW_HIDE);
    g_hover = HOVER_NONE;
}

// Build an absolute-position mouse INPUT for screen point `pt`, tagged with the
// sentinel so our own hook ignores it. Coords normalize over the virtual desktop.
static INPUT MakeAbsMouse(POINT pt, DWORD flags) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw < 2) vw = 2;
    if (vh < 2) vh = 2;
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dx = (LONG)(((double)(pt.x - vx) * 65535.0) / (vw - 1));
    in.mi.dy = (LONG)(((double)(pt.y - vy) * 65535.0) / (vh - 1));
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | flags;
    in.mi.dwExtraInfo = RING_SENTINEL;
    return in;
}

// Emit a full right-click at a screen point so the focused app shows its own
// native context menu there, exactly where the user pressed (not where the
// cursor drifted while the ring was open).
static void SynthRightClick(POINT pt) {
    INPUT in[2] = {};
    in[0] = MakeAbsMouse(pt, MOUSEEVENTF_RIGHTDOWN);
    in[1].type = INPUT_MOUSE;
    in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    in[1].mi.dwExtraInfo = RING_SENTINEL;
    SendInput(2, in, sizeof(INPUT));
}

// Emit only the right-button press (at the original point) when a right-drag is
// detected, so the app receives the down it never saw; the live moves + the
// real release then flow through to complete the drag naturally.
static void SynthRightDownAt(POINT pt) {
    INPUT in = MakeAbsMouse(pt, MOUSEEVENTF_RIGHTDOWN);
    SendInput(1, &in, sizeof(INPUT));
}

// Inject a modifier+key chord into the focused app. Our overlay never takes
// focus (WS_EX_NOACTIVATE), so the app the user right-clicked still owns it and
// receives the keystrokes. The right button is already released by selection
// time, so no stray button state interferes.
static void SendHotkey(WORD mods, WORD vk) {
    INPUT seq[12] = {};
    int n = 0;
    auto key = [&](WORD k, bool up) {
        seq[n].type = INPUT_KEYBOARD;
        seq[n].ki.wVk = k;
        if (up) seq[n].ki.dwFlags = KEYEVENTF_KEYUP;
        ++n;
    };
    if (mods & RMOD_CTRL)  key(VK_CONTROL, false);
    if (mods & RMOD_ALT)   key(VK_MENU,    false);
    if (mods & RMOD_SHIFT) key(VK_SHIFT,   false);
    if (mods & RMOD_WIN)   key(VK_LWIN,    false);
    key(vk, false);
    key(vk, true);
    if (mods & RMOD_WIN)   key(VK_LWIN,    true);
    if (mods & RMOD_SHIFT) key(VK_SHIFT,   true);
    if (mods & RMOD_ALT)   key(VK_MENU,    true);
    if (mods & RMOD_CTRL)  key(VK_CONTROL, true);
    SendInput(n, seq, sizeof(INPUT));
}

// Perform a selection. idx 0..SEG_COUNT-1 = outer slice, SEG_COUNT = hub. Runs
// on the worker thread (posted from the hook) so ShellExecute / SendInput never
// block the low-level mouse hook callback.
static void DispatchAction(int idx) {
    if (idx == SEG_COUNT) {            // center hub: app's own context menu
        SynthRightClick(g_actionPt);
        return;
    }
    if (idx < 0 || idx >= SEG_COUNT) return;
    const SliceAction& a = g_slices[idx];
    switch (a.type) {
        case AT_HOTKEY:
            SendHotkey(a.mods, a.vk);
            break;
        case AT_LAUNCH:
            if (a.launch)
                ShellExecuteW(nullptr, L"open", a.launch, nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case AT_NATIVE:
            SynthRightClick(g_actionPt);
            break;
    }
}

// Called from the hook on release. Captures the target point and posts the
// action to the worker thread; the hook returns immediately.
static void OnSelect(int hover) {
    if (hover == HOVER_HUB) {
        Wh_Log(L"Ring: HUB -> native menu at (%d,%d)", g_downPt.x, g_downPt.y);
        g_actionPt = g_downPt;
        PostMessage(g_hwnd, WM_RING_ACTION, (WPARAM)SEG_COUNT, 0);
    } else if (hover >= 0 && hover < SEG_COUNT) {
        Wh_Log(L"Ring: slice %d (%ls)", hover, g_slices[hover].label);
        g_actionPt = g_downPt;
        PostMessage(g_hwnd, WM_RING_ACTION, (WPARAM)hover, 0);
    } else {
        Wh_Log(L"Ring: cancelled (released outside)");
    }
}

// ============================================================
// Mouse hook + window proc (both on the worker thread)
// ============================================================

// Foreground window's executable basename, lowercased. Empty on failure.
static std::wstring GetForegroundExeName() {
    HWND fg = GetForegroundWindow();
    if (!fg) return L"";
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return L"";
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"";
    wchar_t path[MAX_PATH] = {};
    DWORD sz = MAX_PATH;
    BOOL ok = QueryFullProcessImageNameW(h, 0, path, &sz);
    CloseHandle(h);
    if (!ok || sz == 0) return L"";
    const wchar_t* base = path;
    for (DWORD i = 0; i < sz; i++)
        if (path[i] == L'\\' || path[i] == L'/') base = &path[i + 1];
    std::wstring name(base);
    int n = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE,
        name.c_str(), (int)name.size(), name.data(), (int)name.size(),
        nullptr, nullptr, 0);
    if (n > 0) name.resize(n);
    return name;
}

static bool IsForegroundDenied() {
    if (!g_denyCsInit) return false;
    std::wstring exe = GetForegroundExeName();
    if (exe.empty()) return false;
    bool denied = false;
    EnterCriticalSection(&g_denyCs);
    for (const auto& d : g_denyList)
        if (d == exe) { denied = true; break; }
    LeaveCriticalSection(&g_denyCs);
    return denied;
}

static LRESULT CALLBACK MouseHookProc(int code, WPARAM wp, LPARAM lp) {
    if (code != HC_ACTION || !g_enabled.load())
        return CallNextHookEx(nullptr, code, wp, lp);

    MSLLHOOKSTRUCT* ms = (MSLLHOOKSTRUCT*)lp;
    if (ms->dwExtraInfo == RING_SENTINEL)
        return CallNextHookEx(nullptr, code, wp, lp);

    switch (wp) {
        case WM_RBUTTONDOWN:
            // Denylisted foreground app -> stay completely out of the way.
            if (IsForegroundDenied())
                return CallNextHookEx(nullptr, code, wp, lp);
            g_downPt = ms->pt;
            g_state = ST_PENDING;
            SetTimer(g_hwnd, TIMER_HOLD, (UINT)g_holdMs.load(), nullptr);
            return 1;  // swallow; decide on move / up / timer

        case WM_RBUTTONUP:
            if (g_state == ST_PENDING) {
                KillTimer(g_hwnd, TIMER_HOLD);
                g_state = ST_IDLE;
                SynthRightClick(g_downPt);  // short press -> native menu
                return 1;
            }
            if (g_state == ST_OPEN) {
                int hover = HitTest(ms->pt);
                HideRing();
                g_state = ST_IDLE;
                OnSelect(hover);
                return 1;
            }
            if (g_state == ST_PASSTHRU) {
                // A right-drag we handed back to the app: let the real
                // release through to complete it.
                g_state = ST_IDLE;
                return CallNextHookEx(nullptr, code, wp, lp);
            }
            return CallNextHookEx(nullptr, code, wp, lp);

        case WM_MOUSEMOVE:
            if (g_state == ST_PENDING) {
                // Move-guard: a right-drag must NOT open the ring. Once the
                // cursor leaves the slop box before the hold fires, hand the
                // gesture back as a real drag — synth the press the app never
                // saw (at the original point), then stop swallowing so the
                // live moves + release flow through.
                int dx = ms->pt.x - g_downPt.x;
                int dy = ms->pt.y - g_downPt.y;
                if (dx * dx + dy * dy > g_dragSlop * g_dragSlop) {
                    KillTimer(g_hwnd, TIMER_HOLD);
                    g_state = ST_PASSTHRU;
                    SynthRightDownAt(g_downPt);
                    return 1;  // swallow this move so it lands after the synth down
                }
                return 1;  // still deciding — keep swallowing
            }
            if (g_state == ST_OPEN) {
                int hover = HitTest(ms->pt);
                if (hover != g_hover) {
                    g_hover = hover;
                    RenderRing();
                }
            }
            return CallNextHookEx(nullptr, code, wp, lp);
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TIMER && wp == TIMER_HOLD) {
        KillTimer(hwnd, TIMER_HOLD);
        if (g_state == ST_PENDING) {
            g_state = ST_OPEN;
            ShowRing(g_downPt);
        }
        return 0;
    }
    if (msg == WM_RING_ACTION) {
        DispatchAction((int)wp);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================
// Worker thread
// ============================================================

static DWORD WINAPI RingThread(LPVOID) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Drag slop = a bit more than the system drag size, so a tiny twitch while
    // pressing doesn't cancel the ring. Constant after this point.
    int slop = GetSystemMetrics(SM_CXDRAG) * 2;
    g_dragSlop = slop < 8 ? 8 : slop;

    D2D1_FACTORY_OPTIONS fo = {};
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory), &fo, (void**)&g_d2dFact)) || !g_d2dFact) {
        Wh_Log(L"D2D1CreateFactory failed");
        if (g_hookReady) SetEvent(g_hookReady);
        goto cleanup;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory), (IUnknown**)&g_dwFact)) || !g_dwFact) {
        Wh_Log(L"DWriteCreateFactory failed");
        if (g_hookReady) SetEvent(g_hookReady);
        goto cleanup;
    }

    {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = RING_WNDCLASS;
        RegisterClassExW(&wc);
    }

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        RING_WNDCLASS, L"Right-Click Ring", WS_POPUP,
        0, 0, 10, 10, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!g_hwnd) {
        Wh_Log(L"CreateWindowEx failed");
        if (g_hookReady) SetEvent(g_hookReady);
        goto cleanup;
    }

    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, nullptr, 0);
    if (!g_mouseHook) Wh_Log(L"SetWindowsHookEx(WH_MOUSE_LL) failed");
    if (g_hookReady) SetEvent(g_hookReady);

    Wh_Log(L"Right-Click Ring ready (hook=%p)", (void*)g_mouseHook);

    {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

cleanup:
    if (g_mouseHook) { UnhookWindowsHookEx(g_mouseHook); g_mouseHook = nullptr; }
    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = nullptr; }
    SafeRelease(&g_brush);
    SafeRelease(&g_dcrt);
    SafeRelease(&g_textFmt);
    SafeRelease(&g_iconFmt);
    if (g_memDC) {
        if (g_oldBmp) SelectObject(g_memDC, g_oldBmp);
        DeleteDC(g_memDC);
        g_memDC = nullptr;
    }
    if (g_dib) { DeleteObject(g_dib); g_dib = nullptr; }
    SafeRelease(&g_dwFact);
    SafeRelease(&g_d2dFact);
    UnregisterClassW(RING_WNDCLASS, GetModuleHandle(nullptr));
    CoUninitialize();
    return 0;
}

// ============================================================
// Windhawk entry points
// ============================================================

// Split a semicolon-separated list into trimmed, invariant-lowercased names.
static std::vector<std::wstring> ParseDenyList(LPCWSTR s) {
    std::vector<std::wstring> out;
    if (!s) return out;
    std::wstring cur;
    for (const wchar_t* p = s; ; ++p) {
        if (*p == L';' || *p == L'\0') {
            size_t a = 0, b = cur.size();
            while (a < b && (cur[a] == L' ' || cur[a] == L'\t')) ++a;
            while (b > a && (cur[b-1] == L' ' || cur[b-1] == L'\t')) --b;
            if (b > a) {
                std::wstring item(cur, a, b - a);
                int n = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE,
                    item.c_str(), (int)item.size(), item.data(), (int)item.size(),
                    nullptr, nullptr, 0);
                if (n > 0) item.resize(n);
                out.push_back(std::move(item));
            }
            cur.clear();
            if (*p == L'\0') break;
        } else {
            cur.push_back(*p);
        }
    }
    return out;
}

static void ReadSettings() {
    g_enabled.store(Wh_GetIntSetting(L"enabled") != 0);
    int h = Wh_GetIntSetting(L"holdMs");
    if (h < 50)   h = 50;
    if (h > 1000) h = 1000;
    g_holdMs.store(h);

    LPCWSTR deny = Wh_GetStringSetting(L"denyList");
    auto parsed = ParseDenyList(deny);
    Wh_FreeStringSetting(deny);
    if (g_denyCsInit) {
        EnterCriticalSection(&g_denyCs);
        g_denyList = std::move(parsed);
        LeaveCriticalSection(&g_denyCs);
    } else {
        g_denyList = std::move(parsed);
    }

    Wh_Log(L"Settings: enabled=%d holdMs=%d denyN=%zu",
        (int)g_enabled.load(), g_holdMs.load(), g_denyList.size());
}

BOOL Wh_ModInit() {
    Wh_Log(L"Right-Click Ring: init");
    InitializeCriticalSection(&g_denyCs);
    g_denyCsInit = true;
    ReadSettings();
    g_hookReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, RingThread, nullptr, 0, &g_threadId);
    if (!g_thread) {
        if (g_hookReady) { CloseHandle(g_hookReady); g_hookReady = nullptr; }
        return FALSE;
    }
    if (g_hookReady) {
        WaitForSingleObject(g_hookReady, 2000);
        CloseHandle(g_hookReady);
        g_hookReady = nullptr;
    }
    if (!g_mouseHook) {
        Wh_Log(L"Wh_ModInit: mouse hook not active — reporting failure");
        if (g_hwnd) PostMessage(g_hwnd, WM_CLOSE, 0, 0);
        if (g_threadId) PostThreadMessage(g_threadId, WM_QUIT, 0, 0);
        if (g_thread) {
            WaitForSingleObject(g_thread, 2000);
            CloseHandle(g_thread);
            g_thread = nullptr;
        }
        return FALSE;
    }
    return TRUE;
}

void Wh_ModSettingsChanged() {
    ReadSettings();
}

void Wh_ModUninit() {
    Wh_Log(L"Right-Click Ring: uninit");
    // Unhook first: once this returns the OS guarantees no further hook calls,
    // so the DLL can unload safely even if the worker thread is wedged.
    if (g_mouseHook) { UnhookWindowsHookEx(g_mouseHook); g_mouseHook = nullptr; }
    if (g_hwnd) PostMessage(g_hwnd, WM_CLOSE, 0, 0);
    if (g_threadId) PostThreadMessage(g_threadId, WM_QUIT, 0, 0);
    if (g_thread) {
        if (WaitForSingleObject(g_thread, 5000) == WAIT_TIMEOUT)
            Wh_Log(L"RingThread did not exit in 5s — hook already removed, leaking handle");
        else
            CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_denyCsInit) {
        DeleteCriticalSection(&g_denyCs);
        g_denyCsInit = false;
    }
}
