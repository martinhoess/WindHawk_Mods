# WindHawk Mods

A collection of [Windhawk](https://windhawk.net) mods for Windows.

---

## Mods

### [Explorer Auto Refresh](mods/explorer-auto-refresh/)

> Automatically refreshes Explorer folder views when files change, restoring classic Windows behavior.

Windows Explorer used to automatically refresh its file listing when files changed in the displayed folder. At some point this behavior was removed or became unreliable. This mod restores it.

**Features**
- Event-driven file watching via `FindFirstChangeNotification` — no polling
- Instant detection of new/closed Explorer windows via `SetWinEventHook`
- Tab switch detection via window title change events (~100 ms latency)
- Configurable debounce delay to prevent excessive refreshes during large operations
- Optional monitoring of network drives
- Up to 60 directories watched simultaneously

**Settings**

| Setting | Default | Description |
|---|---|---|
| Enable auto refresh | `true` | Toggle the mod on/off without uninstalling |
| Debounce delay (ms) | `500` | Wait time after a file change before refreshing. Increase for large downloads/extractions |
| Watch network drives | `false` | Also monitor mapped drives and UNC paths |

**Notes**
- Virtual folders (This PC, Recycle Bin, etc.) are not monitored — no file system path
- Only the active tab per window is monitored; background tabs are picked up when activated
- Requires Windows 10/11 with File Explorer (explorer.exe)

---

## License

[WTFPL](LICENSE) — Do What The Fuck You Want To Public License
