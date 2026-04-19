# Explorer Auto Refresh

Automatically refreshes Explorer folder views when files change, restoring classic Windows behavior.

## What it does

Windows Explorer used to refresh its file listing automatically when files changed in the displayed folder. At some point this behavior was removed or became unreliable. This mod restores it.

## How it works

- **Window tracking** — `SetWinEventHook` detects new and closed Explorer windows. One `DWebBrowserEvents2` COM event sink per window handles navigation events. Tab switches are detected via window title change events (~100 ms latency) and trigger an immediate path re-read.
- **File watching** — `FindFirstChangeNotification` on each open directory detects file system changes without polling.
- **Refresh** — After a configurable debounce delay, `SHChangeNotify(SHCNE_UPDATEDIR)` signals Explorer to refresh the view. Debouncing prevents excessive refreshes during bursts (e.g. a download dropping many files).

## Settings

| Setting | Default | Description |
|---|---|---|
| Enable auto refresh | `true` | Toggle the mod on/off at runtime without uninstalling |
| Debounce delay (ms) | `500` | Wait time after a file change before refreshing. Range 100–5000 ms. Increase for large downloads/extractions |
| Watch network drives | `false` | Also monitor mapped drives and UNC paths. Leave off on slow/unreliable connections |

## Notes

- Virtual folders (This PC, Recycle Bin, etc.) have no file system path and are not monitored
- Up to 60 directories can be watched simultaneously
- Only the active tab per window is monitored; background tabs are picked up when activated
- Requires Windows 10/11 with File Explorer (`explorer.exe`)

## License

[WTFPL](../../LICENSE)
