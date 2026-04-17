// ==WindhawkMod==
// @id              explorer-auto-refresh
// @name            Explorer Auto Refresh
// @description     Automatically refreshes Explorer folder views when files change, restoring classic Windows behavior
// @version         1.3
// @author          martinhoess
// @github          https://github.com/martinhoess
// @license         WTFPL
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lshell32 -loleaut32 -lshlwapi -lole32 -luuid
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Explorer Auto Refresh

Windows Explorer used to automatically refresh its file listing when files changed
in the displayed folder. At some point this behavior was removed or became unreliable.
This mod restores it.

## How it works

**Window tracking** — Uses `SetWinEventHook` to detect new and closed Explorer
windows. One `DWebBrowserEvents2` COM event sink per Explorer window handles
navigation events. Tab switches are detected via window title change events and
trigger an immediate path re-read.

**File watching** — `FindFirstChangeNotification` on each open directory detects file
system changes without polling.

**Refresh** — After a configurable debounce delay, `SHChangeNotify(SHCNE_UPDATEDIR)`
signals Explorer to refresh the view. The debounce prevents excessive refreshes during
rapid changes (e.g. a download dropping many files).

## Notes
- Virtual folders (This PC, Recycle Bin, etc.) have no file system path and are not monitored.
- Network drives are skipped by default — enable them in settings if needed.
- Up to 60 directories can be watched simultaneously.
- Background tabs are not individually tracked — only the active tab per window is monitored.
- Tab switches are detected within ~100 ms via window title change events.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- EnableAutoRefresh: true
  $name: Enable auto refresh
  $description: Automatically refresh Explorer when files change in the current folder.

- DebounceMs: 500
  $name: Debounce delay (ms)
  $description: How long to wait after a file change before refreshing. Increase if you see excessive refreshing during large operations (downloads, extractions). Valid range is 100-5000 ms.

- WatchNetworkDrives: false
  $name: Watch network drives
  $description: Also monitor mapped network drives and UNC paths. Disable if Explorer becomes slow or unresponsive on slow or unreliable connections.

*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <atomic>
#include <exdisp.h>
#include <ocidl.h>
#include <oleauto.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ============================================================================
// SETTINGS & LOGGING
// ============================================================================

std::atomic<bool>  g_enabled{true};
std::atomic<DWORD> g_debounceMs{500};
std::atomic<bool>  g_watchNetworkDrives{false};

static void LoadSettings() {
    g_enabled            = Wh_GetIntSetting(L"EnableAutoRefresh",  1) != 0;
    g_watchNetworkDrives = Wh_GetIntSetting(L"WatchNetworkDrives", 0) != 0;

    DWORD raw = (DWORD)Wh_GetIntSetting(L"DebounceMs", 500);
    g_debounceMs = std::max(100ul, std::min(5000ul, raw));
}

// ============================================================================
// TYPES & CONSTANTS
// ============================================================================

constexpr int    MAX_WATCHED_DIRS = 60;
constexpr DISPID kNavigateComplete2 = 252;  // DWebBrowserEvents2::NavigateComplete2

// SetTimer(NULL, ...) ignores nIDEvent and returns a system-generated ID. We
// must store the returned IDs so KillTimer and the WM_TIMER wParam comparisons
// use the real values. Atomics because WinEventProc (see below) and the
// EventMonitorThread message pump both read/write g_refreshTimer. Both run on
// the same STA thread today, but WINEVENT_OUTOFCONTEXT semantics don't
// guarantee future marshalling, and atomics are cheap.
static std::atomic<UINT_PTR> g_safetyTimer{0};
static std::atomic<UINT_PTR> g_refreshTimer{0};

// {34A715A0-6587-11D0-924A-0020AFC7AC4D}
static const GUID s_DIID_DWebBrowserEvents2 =
    {0x34A715A0, 0x6587, 0x11D0, {0x92, 0x4A, 0x00, 0x20, 0xAF, 0xC7, 0xAC, 0x4D}};

// Per-Explorer-window identity: the HWND returned by IWebBrowser2::get_HWND().
// This is STABLE across IShellWindows enumerations — unlike IWebBrowser2 proxy
// addresses, which are new COM proxy objects on each CoCreateInstance call.
using WindowKey = uintptr_t;  // cast from HWND

struct DirectoryWatcher {
    std::wstring path;               // original case — used for API calls
    HANDLE changeHandle = nullptr;
    DWORD lastRefreshTime = 0;
    DWORD watchStartTime  = 0;       // when the handle was opened (grace period clock)
    bool  watchFailed     = false;   // true after FindFirstChangeNotification failed — don't retry
    std::vector<WindowKey> windows;  // Explorer windows currently showing this directory
};

struct WindowSink {
    IConnectionPoint* cp;
    DWORD cookie;
};

// Shared between EventMonitorThread and FileWatcherThread (protected by g_stateMutex).
std::mutex g_stateMutex;
std::unordered_map<std::wstring, DirectoryWatcher> g_watchedDirs;  // key = normalizedPath

// EventMonitorThread-only state (single STA thread — no mutex needed).
std::unordered_map<WindowKey, std::pair<std::wstring, std::wstring>> g_windowPaths;
std::unordered_map<WindowKey, WindowSink> g_windowSinks;

std::unique_ptr<std::thread> g_eventMonitorThread;
std::unique_ptr<std::thread> g_fileWatcherThread;
HANDLE g_monitorStopEvent = nullptr;
HANDLE g_watcherStopEvent = nullptr;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static void OnWindowNavigated(WindowKey wk, const std::wstring& newPath);
static void RefreshWindowSinks();

// ============================================================================
// UTILITY: PATH HELPERS
// ============================================================================

static std::wstring StripTrailingSlash(std::wstring path) {
    while (!path.empty() && path.back() == L'\\')
        path.pop_back();
    return path;
}

static std::wstring NormalizePath(const std::wstring& path) {
    if (path.empty()) return L"";
    std::wstring n = StripTrailingSlash(path);
    for (auto& c : n) c = towlower(c);
    return n;
}

static std::wstring FromBSTR(BSTR b) {
    if (!b) return L"";
    return std::wstring(b, SysStringLen(b));
}

static std::wstring UrlToPath(const std::wstring& url) {
    if (url.empty()) return L"";
    if (url.rfind(L"about:", 0) == 0) return L"";
    wchar_t path[MAX_PATH * 4] = {0};
    DWORD cch = ARRAYSIZE(path);
    if (SUCCEEDED(PathCreateFromUrlW(url.c_str(), path, &cch, 0)))
        return StripTrailingSlash(path);
    const std::wstring pfx = L"file:///";
    if (url.rfind(pfx, 0) == 0)
        return StripTrailingSlash(url.substr(pfx.size()));
    return StripTrailingSlash(url);
}

static bool IsNetworkPath(const std::wstring& path) {
    if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\')
        return true;
    if (path.size() >= 2 && path[1] == L':') {
        wchar_t root[4] = {path[0], L':', L'\\', L'\0'};
        return GetDriveTypeW(root) == DRIVE_REMOTE;
    }
    return false;
}

// ============================================================================
// WATCH LIST SYNC (call with g_stateMutex held)
// ============================================================================

static void SyncWatchListLocked() {
    for (auto& [normPath, watcher] : g_watchedDirs)
        watcher.windows.clear();

    for (const auto& [wk, paths] : g_windowPaths) {
        auto& entry = g_watchedDirs[paths.second];
        if (entry.path.empty())
            entry.path = paths.first;
        entry.windows.push_back(wk);
    }
}

static void SyncWatchList() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    SyncWatchListLocked();
}

// ============================================================================
// COM EVENT SINKS
// ============================================================================

class EventSinkBase : public IDispatch {
    LONG m_refCount = 1;
public:
    virtual ~EventSinkBase() = default;
    STDMETHOD_(ULONG, AddRef)() override  { return InterlockedIncrement(&m_refCount); }
    STDMETHOD_(ULONG, Release)() override {
        LONG n = InterlockedDecrement(&m_refCount);
        if (n == 0) delete this;
        return n;
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDispatch) {
            *ppv = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD(GetTypeInfoCount)(UINT*) override               { return E_NOTIMPL; }
    STDMETHOD(GetTypeInfo)(UINT, LCID, ITypeInfo**) override  { return E_NOTIMPL; }
    STDMETHOD(GetIDsOfNames)(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return E_NOTIMPL; }
};

// Per-window sink: fires when the active tab navigates to a new folder.
class BrowserEventSink : public EventSinkBase {
    WindowKey m_wk;
public:
    explicit BrowserEventSink(WindowKey wk) : m_wk(wk) {}

    STDMETHOD(Invoke)(DISPID dispid, REFIID, LCID, WORD, DISPPARAMS* p,
                      VARIANT*, EXCEPINFO*, UINT*) override {
        if (dispid == kNavigateComplete2 && p && p->cArgs >= 1) {
            const VARIANT& v0 = p->rgvarg[0];
            const VARIANT* pUrl = (V_VT(&v0) == (VT_BYREF | VT_VARIANT))
                                  ? V_VARIANTREF(&v0) : nullptr;
            if (pUrl && V_VT(pUrl) == VT_BSTR) {
                std::wstring newPath = UrlToPath(FromBSTR(V_BSTR(pUrl)));
                if (!newPath.empty())
                    OnWindowNavigated(m_wk, newPath);
            }
        }
        return S_OK;
    }
};

// ============================================================================
// WINDOW TRACKING (EventMonitorThread only)
// ============================================================================

static void OnWindowNavigated(WindowKey wk, const std::wstring& newPath) {
    // Ignore events from windows we no longer track (e.g. a queued NavigateComplete2
    // that arrived after the window was closed and removed from g_windowPaths).
    auto it = g_windowPaths.find(wk);
    if (it == g_windowPaths.end()) return;
    Wh_Log(L"Navigate: %p  %s -> %s",
           (HWND)wk, it->second.first.c_str(), newPath.c_str());
    it->second = {newPath, NormalizePath(newPath)};
    SyncWatchList();
}

// Registers a DWebBrowserEvents2 sink for wk. Only done once per HWND — the sink
// survives tab switches and is only removed when the window closes.
static void RegisterWindowSink(WindowKey wk, IWebBrowser2* wb) {
    if (g_windowSinks.count(wk)) return;  // already registered

    IConnectionPointContainer* cpc = nullptr;
    if (FAILED(wb->QueryInterface(IID_PPV_ARGS(&cpc)))) return;

    IConnectionPoint* cp = nullptr;
    if (SUCCEEDED(cpc->FindConnectionPoint(s_DIID_DWebBrowserEvents2, &cp))) {
        auto* sink = new BrowserEventSink(wk);
        DWORD cookie = 0;
        if (SUCCEEDED(cp->Advise(sink, &cookie))) {
            g_windowSinks[wk] = {cp, cookie};
        } else {
            cp->Release();
            Wh_Log(L"Advise failed for window %p — navigation events won't be tracked", (HWND)wk);
        }
        sink->Release();
    }
    cpc->Release();
}

static void UnregisterWindowSink(WindowKey wk) {
    auto it = g_windowSinks.find(wk);
    if (it == g_windowSinks.end()) return;
    it->second.cp->Unadvise(it->second.cookie);
    it->second.cp->Release();
    g_windowSinks.erase(it);
}

// Enumerates IShellWindows (one entry per Explorer window = active tab).
// Registers sinks for new windows, removes sinks for closed windows.
// Safe to call frequently — HWND keys mean no spurious Unadvise/Advise churn.
static void RefreshWindowSinks() {
    IShellWindows* spWindows = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&spWindows))))
        return;

    long count = 0;
    spWindows->get_Count(&count);

    std::unordered_set<WindowKey> activeWindows;

    for (long i = 0; i < count; ++i) {
        VARIANT vI;
        VariantInit(&vI);
        V_VT(&vI) = VT_I4;
        V_I4(&vI) = i;

        IDispatch* disp = nullptr;
        if (FAILED(spWindows->Item(vI, &disp)) || !disp) {
            VariantClear(&vI);
            continue;
        }

        IWebBrowser2* wb = nullptr;
        if (SUCCEEDED(disp->QueryInterface(IID_PPV_ARGS(&wb))) && wb) {
            // get_HWND returns a stable HWND — unlike wb's proxy address, which
            // is a new COM proxy object on every CoCreateInstance call.
            SHANDLE_PTR hwndLong = 0;
            wb->get_HWND(&hwndLong);
            HWND hwnd = (HWND)(LONG_PTR)hwndLong;

            if (hwnd) {
                WindowKey wk = (WindowKey)hwnd;
                activeWindows.insert(wk);

                BSTR bUrl = nullptr;
                std::wstring path;
                if (SUCCEEDED(wb->get_LocationURL(&bUrl))) {
                    path = UrlToPath(FromBSTR(bUrl));
                    SysFreeString(bUrl);
                }

                if (!path.empty()) {
                    auto it = g_windowPaths.find(wk);
                    if (it == g_windowPaths.end()) {
                        g_windowPaths[wk] = {path, NormalizePath(path)};
                        Wh_Log(L"New window: %p -> %s", hwnd, path.c_str());
                    } else if (it->second.first != path) {
                        Wh_Log(L"Tab switched: %p  %s -> %s",
                               hwnd, it->second.first.c_str(), path.c_str());
                        it->second = {path, NormalizePath(path)};
                    }
                }

                // Register sink once — reuse across tab switches.
                RegisterWindowSink(wk, wb);
            }
            wb->Release();
        }
        disp->Release();
        VariantClear(&vI);
    }

    spWindows->Release();

    // Remove entries for windows no longer in IShellWindows.
    std::vector<WindowKey> toRemove;
    for (const auto& [wk, paths] : g_windowPaths)
        if (!activeWindows.count(wk)) toRemove.push_back(wk);
    for (const auto& [wk, ws] : g_windowSinks)
        if (!activeWindows.count(wk) && !g_windowPaths.count(wk)) toRemove.push_back(wk);

    for (WindowKey wk : toRemove) {
        auto pathIt = g_windowPaths.find(wk);
        Wh_Log(L"Window closed: %p (%s)",
               (HWND)wk,
               pathIt != g_windowPaths.end() ? pathIt->second.first.c_str() : L"<no path>");
        if (pathIt != g_windowPaths.end()) g_windowPaths.erase(pathIt);
        UnregisterWindowSink(wk);
    }

    SyncWatchList();
}

// ============================================================================
// EVENT MONITOR THREAD
// ============================================================================

static void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd,
                                   LONG idObject, LONG idChild, DWORD, DWORD) {
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;

    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
    if (wcscmp(cls, L"CabinetWClass") != 0 &&
        wcscmp(cls, L"ExploreWClass")  != 0) return;

    // No per-event logging — too noisy. Windhawk log is always active.

    // Debounce: reset a 100-ms countdown. Rapid events (e.g. multiple NAMECHANGE
    // callbacks per tab switch) coalesce into a single RefreshWindowSinks() call.
    // SetTimer(NULL, ...) ignores nIDEvent — must store the returned ID so we can
    // kill the previous pending timer and match it in the message pump.
    UINT_PTR prev = g_refreshTimer.exchange(0);
    if (prev) KillTimer(nullptr, prev);
    g_refreshTimer = SetTimer(nullptr, 0, 100, nullptr);
}

void EventMonitorThread() {
  try {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    RefreshWindowSinks();

    HWINEVENTHOOK hookShow = SetWinEventHook(
        EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW,
        nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    HWINEVENTHOOK hookDestroy = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY,
        nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    HWINEVENTHOOK hookNameChange = SetWinEventHook(
        EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE,
        nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    if (!hookShow || !hookDestroy || !hookNameChange)
        Wh_Log(L"SetWinEventHook failed (show=%p destroy=%p namechange=%p)",
               hookShow, hookDestroy, hookNameChange);
    else
        Wh_Log(L"WinEvent hooks registered");

    g_safetyTimer = SetTimer(nullptr, 0, 1000, nullptr);

    MSG msg;
    while (true) {
        DWORD r = MsgWaitForMultipleObjects(1, &g_monitorStopEvent, FALSE,
                                            INFINITE, QS_ALLEVENTS);
        if (r == WAIT_OBJECT_0) break;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_TIMER) {
                if (msg.wParam == g_safetyTimer.load()) {
                    RefreshWindowSinks();
                } else if (msg.wParam == g_refreshTimer.load()) {
                    UINT_PTR id = g_refreshTimer.exchange(0);
                    if (id) KillTimer(nullptr, id);
                    RefreshWindowSinks();
                }
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (UINT_PTR t = g_safetyTimer.exchange(0))  KillTimer(nullptr, t);
    if (UINT_PTR t = g_refreshTimer.exchange(0)) KillTimer(nullptr, t);
    if (hookShow)       UnhookWinEvent(hookShow);
    if (hookDestroy)    UnhookWinEvent(hookDestroy);
    if (hookNameChange) UnhookWinEvent(hookNameChange);

    for (auto& [wk, ws] : g_windowSinks) {
        ws.cp->Unadvise(ws.cookie);
        ws.cp->Release();
    }
    g_windowSinks.clear();
    g_windowPaths.clear();

    CoUninitialize();
  } catch (...) {
    Wh_Log(L"EventMonitorThread: unhandled exception");
  }
}

// ============================================================================
// FILE WATCHER THREAD
// ============================================================================

void FileWatcherThread() {
  try {
    HANDLE watcherHandles[MAX_WATCHED_DIRS + 1];
    std::wstring dirKeys[MAX_WATCHED_DIRS];
    int handleCount = 0;

    while (WaitForSingleObject(g_watcherStopEvent, 100) == WAIT_TIMEOUT) {
        if (!g_enabled) {
            Sleep(100);
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(g_stateMutex);

            // Remove entries with no open windows.
            std::vector<std::wstring> toRemove;
            for (const auto& [normPath, watcher] : g_watchedDirs) {
                if (watcher.windows.empty())
                    toRemove.push_back(normPath);
            }
            for (const auto& normPath : toRemove) {
                auto it = g_watchedDirs.find(normPath);
                if (it != g_watchedDirs.end()) {
                    if (it->second.changeHandle)
                        FindCloseChangeNotification(it->second.changeHandle);
                    Wh_Log(L"Stopped watching: %s", it->second.path.c_str());
                    g_watchedDirs.erase(it);
                }
            }

            // Open notification handles for newly tracked directories.
            for (auto& [normPath, watcher] : g_watchedDirs) {
                if (watcher.changeHandle || watcher.watchFailed || watcher.windows.empty())
                    continue;

                if (!g_watchNetworkDrives && IsNetworkPath(watcher.path)) {
                    continue;
                }

                // Only watch actual directories — Explorer shows zip files as
                // folders but FindFirstChangeNotificationW can't watch them.
                DWORD attrs = GetFileAttributesW(watcher.path.c_str());
                if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                    Wh_Log(L"Skipping non-directory: %s", watcher.path.c_str());
                    watcher.watchFailed = true;
                    continue;
                }

                HANDLE h = FindFirstChangeNotificationW(
                    watcher.path.c_str(),
                    FALSE,
                    FILE_NOTIFY_CHANGE_FILE_NAME  |
                    FILE_NOTIFY_CHANGE_DIR_NAME   |
                    FILE_NOTIFY_CHANGE_LAST_WRITE |
                    FILE_NOTIFY_CHANGE_SIZE       |
                    FILE_NOTIFY_CHANGE_ATTRIBUTES
                );
                if (h != INVALID_HANDLE_VALUE) {
                    watcher.changeHandle    = h;
                    watcher.lastRefreshTime = 0;
                    watcher.watchStartTime  = GetTickCount();
                    Wh_Log(L"Started watching: %s", watcher.path.c_str());
                } else {
                    Wh_Log(L"Failed to watch: %s (err %u)", watcher.path.c_str(), GetLastError());
                    watcher.watchFailed = true;
                }
            }

            // Build handle array for WaitForMultipleObjects.
            handleCount = 1;
            watcherHandles[0] = g_watcherStopEvent;
            for (auto& [normPath, watcher] : g_watchedDirs) {
                if (watcher.changeHandle && handleCount < MAX_WATCHED_DIRS) {
                    watcherHandles[handleCount] = watcher.changeHandle;
                    dirKeys[handleCount] = normPath;
                    handleCount++;
                }
            }
        }

        if (handleCount <= 1) {
            Sleep(100);
            continue;
        }

        DWORD dwWait = WaitForMultipleObjects(handleCount, watcherHandles, FALSE, 100);

        if (dwWait == WAIT_TIMEOUT || dwWait == WAIT_OBJECT_0) {
            // timeout or stop event
        } else if (dwWait >= WAIT_OBJECT_0 + 1 &&
                   dwWait <  WAIT_OBJECT_0 + (DWORD)handleCount) {
            int idx = (int)(dwWait - WAIT_OBJECT_0);
            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                auto it = g_watchedDirs.find(dirKeys[idx]);
                if (it != g_watchedDirs.end()) {
                    // file change detected — debounce timer will trigger refresh
                    it->second.lastRefreshTime = GetTickCount();
                }
            }
            FindNextChangeNotification(watcherHandles[idx]);
        } else if (dwWait == WAIT_FAILED) {
            Wh_Log(L"WaitForMultipleObjects failed: %u", GetLastError());
            Sleep(100);
        }

        // Send debounced refresh notifications.
        DWORD now      = GetTickCount();
        DWORD debounce = g_debounceMs;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            for (auto& [normPath, watcher] : g_watchedDirs) {
                if (!watcher.lastRefreshTime) continue;
                // Grace period: skip refreshes for the first 1.5 s after a watcher
                // is opened. This prevents a SHChangeNotify from firing while a new
                // tab is still initializing its view, which would cause a white flash.
                if (now - watcher.watchStartTime < 1500) continue;
                if (now - watcher.lastRefreshTime >= debounce) {
                    SHChangeNotify(SHCNE_UPDATEDIR,
                                   SHCNF_PATHW | SHCNF_FLUSHNOWAIT,
                                   watcher.path.c_str(), nullptr);
                    Wh_Log(L"Refreshed: %s", watcher.path.c_str());
                    watcher.lastRefreshTime = 0;
                }
            }
        }
    }

    // Cleanup on exit.
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        for (auto& [normPath, watcher] : g_watchedDirs) {
            if (watcher.changeHandle) {
                FindCloseChangeNotification(watcher.changeHandle);
                watcher.changeHandle = nullptr;
            }
        }
        g_watchedDirs.clear();
    }
  } catch (...) {
    Wh_Log(L"FileWatcherThread: unhandled exception");
  }
}

// ============================================================================
// MOD LIFECYCLE
// ============================================================================

BOOL Wh_ModInit() {
    Wh_Log(L"Explorer Auto Refresh: Initializing...");
    LoadSettings();
    Wh_Log(L"Settings: enabled=%d debounce=%ums network=%d",
           g_enabled.load(), g_debounceMs.load(), g_watchNetworkDrives.load());

    g_monitorStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_watcherStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_monitorStopEvent || !g_watcherStopEvent) {
        // If only one of the two CreateEventW calls succeeded, the other leaks
        // for the lifetime of explorer.exe — Windhawk does not call Uninit when
        // Init returns FALSE. Close whichever handle we did get.
        if (g_monitorStopEvent) { CloseHandle(g_monitorStopEvent); g_monitorStopEvent = nullptr; }
        if (g_watcherStopEvent) { CloseHandle(g_watcherStopEvent); g_watcherStopEvent = nullptr; }
        Wh_Log(L"Failed to create stop events");
        return FALSE;
    }

    // Create threads one at a time so we can cleanly roll back if the second
    // make_unique throws. If we returned FALSE while a thread was still
    // running, Windhawk would unload the DLL under it — the thread's code
    // pages would become unmapped and the next callback would crash explorer.
    try {
        g_eventMonitorThread = std::make_unique<std::thread>(EventMonitorThread);
    } catch (...) {
        Wh_Log(L"Failed to create event monitor thread");
        CloseHandle(g_monitorStopEvent); g_monitorStopEvent = nullptr;
        CloseHandle(g_watcherStopEvent); g_watcherStopEvent = nullptr;
        return FALSE;
    }
    try {
        g_fileWatcherThread  = std::make_unique<std::thread>(FileWatcherThread);
    } catch (...) {
        Wh_Log(L"Failed to create file watcher thread");
        SetEvent(g_monitorStopEvent);
        if (g_eventMonitorThread->joinable()) g_eventMonitorThread->join();
        g_eventMonitorThread.reset();
        CloseHandle(g_monitorStopEvent); g_monitorStopEvent = nullptr;
        CloseHandle(g_watcherStopEvent); g_watcherStopEvent = nullptr;
        return FALSE;
    }

    Wh_Log(L"Explorer Auto Refresh: Ready");
    return TRUE;
}

void Wh_ModAfterInit() {}

void Wh_ModBeforeUninit() {
    Wh_Log(L"Explorer Auto Refresh: Shutting down...");

    if (g_monitorStopEvent) SetEvent(g_monitorStopEvent);
    if (g_watcherStopEvent) SetEvent(g_watcherStopEvent);

    if (g_eventMonitorThread) {
        if (g_eventMonitorThread->joinable()) g_eventMonitorThread->join();
        g_eventMonitorThread.reset();
    }
    if (g_fileWatcherThread) {
        if (g_fileWatcherThread->joinable()) g_fileWatcherThread->join();
        g_fileWatcherThread.reset();
    }

    if (g_monitorStopEvent) { CloseHandle(g_monitorStopEvent); g_monitorStopEvent = nullptr; }
    if (g_watcherStopEvent) { CloseHandle(g_watcherStopEvent); g_watcherStopEvent = nullptr; }
}

void Wh_ModUninit() {
    Wh_Log(L"Explorer Auto Refresh: Unloaded");
}

BOOL Wh_ModSettingsChanged(BOOL* bReload) {
    LoadSettings();
    Wh_Log(L"Settings updated: enabled=%d debounce=%ums network=%d",
           g_enabled.load(), g_debounceMs.load(), g_watchNetworkDrives.load());
    *bReload = FALSE;
    return TRUE;
}
