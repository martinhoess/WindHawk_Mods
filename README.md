# WindHawk Mods

A collection of [Windhawk](https://windhawk.net) mods for Windows.

---

## Mods

### [Emoji Picker](mods/emoji-picker/)

> Replaces the Windows 11 emoji dialog (Win+.) with a Windows 10-inspired picker: dark/light theme, real-time search, category tabs, and recent emoji.

**Features**
- Keyboard-driven picker opened via Win+. (Windows 11 dialog blocked) or a configurable secondary shortcut
- Real-time search across ~1660 emoji (Unicode 15.0) with German and English keywords
- Nine category tabs plus a Recent tab that persists across sessions
- Direct2D / DirectWrite rendering, automatic dark/light theme following system setting
- Optional hiding of the Flags category for systems that render flags as ISO letters

**Settings**

| Setting | Default | Description |
|---|---|---|
| Intercept Win+. | `true` | Block the Windows 11 emoji dialog and open this picker instead |
| Custom shortcut | `Ctrl+.` | Additional shortcut (`Ctrl+.`, `Ctrl+Space`, `Alt+.`, or disabled) |
| Hide Flags category | `false` | Hide national flags from picker and tab bar |

---

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

### [Right-Click Ring](mods/right-click-ring/)

> Hold the right mouse button to open a Logitech Actions Ring–style radial menu at the cursor. Release over a bubble to run its action; the center hub falls through to the app's own context menu. A quick right-click still shows the normal menu.

**Features**
- Global `WH_MOUSE_LL` hook with a hold-to-open trigger; quick click passes through as a normal right-click
- Logitech-style floating icon bubbles (Segoe Fluent Icons) rendered with Direct2D, per-monitor DPI aware
- Eight actions — clipboard/edit hotkeys, app launch, native menu — plus a center hub that names the hovered slice and falls through to the app's own context menu
- Move-guard hands a right-drag back to the app instead of opening the ring
- Per-app denylist to keep right-click fully native in games, remote desktop, etc.

**Settings**

| Setting | Default | Description |
|---|---|---|
| Enabled | `true` | Master switch without uninstalling |
| Hold threshold (ms) | `250` | How long to hold before the ring opens; also the max delay added to a normal right-click |
| App denylist | *(empty)* | Semicolon-separated process names where the ring is disabled |

See the [mod README](mods/right-click-ring/) for the roadmap (user-configurable slices, sub-bubbles, native-items-in-ring).

---

## License

[WTFPL](LICENSE) — Do What The Fuck You Want To Public License
