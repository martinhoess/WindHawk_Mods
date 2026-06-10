// ==WindhawkMod==
// @id              right-click-ring
// @name            Right-Click Ring
// @description     Holding the right mouse button opens a Logitech-style radial overlay at the cursor. The center hub falls through to the app's own context menu; a quick right-click still shows it directly. The 8 outer slices are placeholders (logged only).
// @version         0.2
// @author          martinhoess
// @github          https://github.com/martinhoess
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -ld2d1 -ldwrite -lgdi32 -luser32 -lole32 -lshcore
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*
- enabled: true
  $name: Enabled
  $description: Master switch. Turn off to stop intercepting right-clicks without uninstalling.
- holdMs: 250
  $name: Hold threshold (ms)
  $description: "How long the right button must be held before the ring opens. A shorter press passes through as a normal right-click — but normal right-clicks are delayed by up to this many ms while the mod decides. 200-300 feels natural."
*/
// ==/WindhawkModSettings==

// MVP NOTES
// ---------
// Goal of this version: prove the plumbing works end-to-end — global mouse
// hook fires, a translucent radial overlay renders at the cursor with true
// per-pixel alpha (black/white, Logitech-ish), hover tracking follows the
// mouse, and a release reports which slice was chosen (via Wh_Log).
//
// The 8 outer slice labels are placeholders (selection is logged only). The
// center "Win" hub now falls through: releasing on it re-issues a real
// right-click at the original spot so the app shows its own context menu.
// The ONE behaviour change you'll feel: every right-click system-wide now
// waits up to `holdMs` (hold = ring, tap = native menu).
//
// Architecture mirrors the emoji-picker mod: a single worker thread owns the
// D2D resources, the overlay window, the WH_MOUSE_LL hook, and a message loop.
// Because LL-hook callbacks AND the hold timer are both delivered to that one
// thread's message queue, the state machine needs no locking — only the
// settings (read in the hook, written in Wh_ModSettingsChanged) are atomic.

#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <shellscalingapi.h>
#include <atomic>
#include <cmath>

// ============================================================
// Constants
// ============================================================

static const wchar_t* RING_WNDCLASS = L"WhRightClickRing";

constexpr float  RING_DIAM_DIP = 320.0f;  // outer diameter, device-independent px
constexpr float  HUB_RADIUS_DIP = 46.0f;  // central "Win" hub radius
constexpr int    SEG_COUNT = 8;
constexpr float  PI = 3.14159265358979323846f;

constexpr UINT_PTR TIMER_HOLD = 1;

// Marks our own synthesized right-clicks so the hook ignores them (no re-entry).
constexpr ULONG_PTR RING_SENTINEL = 0x52494E47;  // 'RING'

// Hover sentinels
constexpr int HOVER_NONE = -1;
constexpr int HOVER_HUB  = -2;

// Placeholder slice labels — purely cosmetic for this milestone.
static const wchar_t* g_labels[SEG_COUNT] = {
    L"Copy", L"Paste", L"Cut", L"Undo", L"Redo", L"Search", L"New", L"More",
};

// ============================================================
// State
// ============================================================

enum RingState { ST_IDLE, ST_PENDING, ST_OPEN };

static DWORD  g_threadId = 0;
static HANDLE g_thread   = nullptr;
static HANDLE g_hookReady = nullptr;
static HHOOK  g_mouseHook = nullptr;
static HWND   g_hwnd      = nullptr;

// Worker-thread-only (no locking needed — see header note)
static RingState g_state   = ST_IDLE;
static POINT     g_downPt  = {0, 0};   // where the right button went down (screen px)
static POINT     g_centerPt = {0, 0};  // ring center on screen (== g_downPt while open)
static int       g_hover   = HOVER_NONE;
static float     g_scale     = 1.0f;
static float     g_outerR    = 0.0f;   // physical px
static float     g_hubR      = 0.0f;   // physical px

// Settings (read from hook thread, written from Windhawk thread)
static std::atomic<bool> g_enabled{true};
static std::atomic<int>  g_holdMs{250};

// D2D / DWrite
static ID2D1Factory*       g_d2dFact = nullptr;
static IDWriteFactory*     g_dwFact  = nullptr;
static ID2D1DCRenderTarget* g_dcrt   = nullptr;
static ID2D1SolidColorBrush* g_brush = nullptr;
static IDWriteTextFormat*  g_textFmt = nullptr;
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

// (Re)create the DIB + DCRenderTarget + brush + text format when the required
// size (DPI-dependent) changes. Returns false on failure.
static bool EnsureSurface(int w, int h, float scale) {
    if (g_dcrt && g_surfW == w && g_surfH == h) {
        if (g_textScale != scale && g_dwFact) {
            SafeRelease(&g_textFmt);
            if (SUCCEEDED(g_dwFact->CreateTextFormat(
                    L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    13.0f * scale, L"", &g_textFmt))) {
                g_textFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                g_textFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
            g_textScale = scale;
        }
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

    SafeRelease(&g_textFmt);
    if (SUCCEEDED(g_dwFact->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            13.0f * scale, L"", &g_textFmt))) {
        g_textFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_textFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    g_textScale = scale;

    return g_brush != nullptr;
}

static void FillWedge(float cx, float cy, float innerR, float outerR,
                      float a0, float a1) {
    ID2D1PathGeometry* geo = nullptr;
    if (FAILED(g_d2dFact->CreatePathGeometry(&geo)) || !geo) return;
    ID2D1GeometrySink* sink = nullptr;
    if (FAILED(geo->Open(&sink)) || !sink) { geo->Release(); return; }

    sink->BeginFigure(PointOnCircle(cx, cy, innerR, a0), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(PointOnCircle(cx, cy, outerR, a0));
    D2D1_ARC_SEGMENT outer = {};
    outer.point = PointOnCircle(cx, cy, outerR, a1);
    outer.size = D2D1::SizeF(outerR, outerR);
    outer.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
    outer.arcSize = D2D1_ARC_SIZE_SMALL;
    sink->AddArc(outer);
    sink->AddLine(PointOnCircle(cx, cy, innerR, a1));
    D2D1_ARC_SEGMENT inner = {};
    inner.point = PointOnCircle(cx, cy, innerR, a0);
    inner.size = D2D1::SizeF(innerR, innerR);
    inner.sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
    inner.arcSize = D2D1_ARC_SIZE_SMALL;
    sink->AddArc(inner);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    sink->Release();

    g_dcrt->FillGeometry(geo, g_brush);
    geo->Release();
}

// Render the ring into g_memDC and blit it to the layered window.
static void RenderRing() {
    if (!g_dcrt || !g_brush) return;

    RECT rc = {0, 0, g_surfW, g_surfH};
    if (FAILED(g_dcrt->BindDC(g_memDC, &rc))) return;

    g_dcrt->BeginDraw();
    g_dcrt->Clear(D2D1::ColorF(0, 0.0f));  // fully transparent

    float cx = g_surfW / 2.0f;
    float cy = g_surfH / 2.0f;
    float outerR = g_outerR;
    float hubR = g_hubR;

    // Background disc
    g_brush->SetColor(D2D1::ColorF(0, 0, 0, 0.80f));
    g_dcrt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), outerR, outerR), g_brush);

    // Hover highlight wedge
    if (g_hover >= 0) {
        g_brush->SetColor(D2D1::ColorF(1, 1, 1, 0.16f));
        FillWedge(cx, cy, hubR, outerR, g_hover * 45.0f - 22.5f, g_hover * 45.0f + 22.5f);
    }

    // Separators
    g_brush->SetColor(D2D1::ColorF(1, 1, 1, 0.12f));
    for (int i = 0; i < SEG_COUNT; ++i) {
        float a = i * 45.0f - 22.5f;
        g_dcrt->DrawLine(PointOnCircle(cx, cy, hubR, a),
                         PointOnCircle(cx, cy, outerR, a), g_brush, 1.0f * g_scale);
    }

    // Outer rim
    g_brush->SetColor(D2D1::ColorF(1, 1, 1, 0.25f));
    g_dcrt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), outerR, outerR),
                        g_brush, 1.2f * g_scale);

    // Slice labels
    if (g_textFmt) {
        float labelR = (hubR + outerR) / 2.0f;
        for (int i = 0; i < SEG_COUNT; ++i) {
            D2D1_POINT_2F p = PointOnCircle(cx, cy, labelR, i * 45.0f);
            D2D1_RECT_F r = D2D1::RectF(p.x - 44 * g_scale, p.y - 14 * g_scale,
                                        p.x + 44 * g_scale, p.y + 14 * g_scale);
            g_brush->SetColor(D2D1::ColorF(1, 1, 1, 0.92f));
            g_dcrt->DrawTextW(g_labels[i], (UINT32)wcslen(g_labels[i]),
                              g_textFmt, r, g_brush);
        }
    }

    // Center hub
    g_brush->SetColor(g_hover == HOVER_HUB ? D2D1::ColorF(1, 1, 1, 0.20f)
                                           : D2D1::ColorF(0, 0, 0, 0.55f));
    g_dcrt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), hubR, hubR), g_brush);
    g_brush->SetColor(D2D1::ColorF(1, 1, 1, 0.25f));
    g_dcrt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), hubR, hubR), g_brush, 1.0f * g_scale);
    if (g_textFmt) {
        D2D1_RECT_F r = D2D1::RectF(cx - hubR, cy - hubR, cx + hubR, cy + hubR);
        g_brush->SetColor(D2D1::ColorF(1, 1, 1, 0.95f));
        g_dcrt->DrawTextW(L"Win", 3, g_textFmt, r, g_brush);
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

// Emit a normal right-click at a screen point so the focused app shows its own
// native context menu there. Marked with the sentinel so our own hook lets the
// synthetic events through untouched (no re-entry, no ring re-trigger). The
// down event also moves the cursor to `pt` via absolute positioning, so the
// menu appears exactly where the user originally pressed — not wherever the
// cursor drifted while the ring was open.
static void SynthRightClick(POINT pt) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw < 2) vw = 2;
    if (vh < 2) vh = 2;
    LONG nx = (LONG)(((double)(pt.x - vx) * 65535.0) / (vw - 1));
    LONG ny = (LONG)(((double)(pt.y - vy) * 65535.0) / (vh - 1));

    INPUT in[2] = {};
    in[0].type = INPUT_MOUSE;
    in[0].mi.dx = nx;
    in[0].mi.dy = ny;
    in[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE |
                       MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_RIGHTDOWN;
    in[0].mi.dwExtraInfo = RING_SENTINEL;
    in[1].type = INPUT_MOUSE;
    in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    in[1].mi.dwExtraInfo = RING_SENTINEL;
    SendInput(2, in, sizeof(INPUT));
}

static void OnSelect(int hover) {
    if (hover == HOVER_HUB) {
        Wh_Log(L"Ring: HUB -> native menu at (%d,%d)", g_downPt.x, g_downPt.y);
        SynthRightClick(g_downPt);  // show the app's own context menu
    } else if (hover >= 0) {
        Wh_Log(L"Ring: selected slice %d (%ls)", hover, g_labels[hover]);
    } else {
        Wh_Log(L"Ring: cancelled (released outside)");
    }
}

// ============================================================
// Mouse hook + window proc (both on the worker thread)
// ============================================================

static LRESULT CALLBACK MouseHookProc(int code, WPARAM wp, LPARAM lp) {
    if (code != HC_ACTION || !g_enabled.load())
        return CallNextHookEx(nullptr, code, wp, lp);

    MSLLHOOKSTRUCT* ms = (MSLLHOOKSTRUCT*)lp;
    if (ms->dwExtraInfo == RING_SENTINEL)
        return CallNextHookEx(nullptr, code, wp, lp);

    switch (wp) {
        case WM_RBUTTONDOWN:
            g_downPt = ms->pt;
            g_state = ST_PENDING;
            SetTimer(g_hwnd, TIMER_HOLD, (UINT)g_holdMs.load(), nullptr);
            return 1;  // swallow; decide on UP or timer

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
            return CallNextHookEx(nullptr, code, wp, lp);

        case WM_MOUSEMOVE:
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
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================
// Worker thread
// ============================================================

static DWORD WINAPI RingThread(LPVOID) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

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

static void ReadSettings() {
    g_enabled.store(Wh_GetIntSetting(L"enabled") != 0);
    int h = Wh_GetIntSetting(L"holdMs");
    if (h < 50)   h = 50;
    if (h > 1000) h = 1000;
    g_holdMs.store(h);
    Wh_Log(L"Settings: enabled=%d holdMs=%d", (int)g_enabled.load(), g_holdMs.load());
}

BOOL Wh_ModInit() {
    Wh_Log(L"Right-Click Ring: init");
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
}
