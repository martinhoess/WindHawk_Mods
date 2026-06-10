# Right-Click Ring

> Hold the right mouse button to open a Logitech Actions Ring–style radial menu at the cursor. Release over a bubble to run its action; the center hub falls through to the app's own context menu. A quick right-click still shows the normal menu.

Inspired by the Logitech Logi Options+ "Actions Ring". Monochrome black/white floating bubbles with slight transparency, rendered with Direct2D / DirectWrite.

## How it works

- A global low-level mouse hook (`WH_MOUSE_LL`, installed from `explorer.exe`) watches the right button.
- **Hold** the right button (≥ *Hold threshold*) → the ring opens at the cursor. Release over a bubble to run its action; release over the center hub (or **More**) to fall through to the app's **native** context menu; release outside to cancel.
- **Quick click** (shorter than the threshold) passes through as a normal right-click.
- **Right-drag** (move while holding) is handed back to the app as a real drag — the ring does not open.
- The synthesized native-menu click is tagged with a sentinel so the hook ignores it (no re-entry).

Actions are dispatched on a worker thread (never inside the hook callback), so launching apps or sending keystrokes can't stall the input hook.

## Slices (current placeholder set)

| Bubble | Glyph | Action |
|---|---|---|
| Copy | ⧉ | `Ctrl+C` |
| Paste | 📋 | `Ctrl+V` |
| Cut | ✂ | `Ctrl+X` |
| Undo | ↶ | `Ctrl+Z` |
| Redo | ↷ | `Ctrl+Y` |
| Find | 🔍 | `Ctrl+F` |
| Explorer | 📁 | launch a new Explorer window |
| More | ⋯ | native context menu |
| **Hub** | ☰ | native context menu (always); also names the hovered slice |

Icons are Segoe Fluent Icons glyphs. The set is hard-coded for now and becomes user-configurable in a later version.

## Settings

| Setting | Default | Description |
|---|---|---|
| Enabled | `true` | Master switch without uninstalling |
| Hold threshold (ms) | `250` | How long to hold before the ring opens; also the maximum delay added to a normal right-click |
| App denylist | *(empty)* | Semicolon-separated process names (e.g. `game.exe;mstsc.exe`) where the ring is disabled and right-click stays completely native |

## Notes

- While enabled this affects right-click **system-wide**: every right-click waits up to *Hold threshold* (hold = ring, tap = native menu). Toggle the mod off to stop.
- Built on Windows 11 (Segoe Fluent Icons); per-monitor DPI aware.
- Native-menu fall-through works in **any** app because it replays a real right-click. Pulling an app's *own* menu items into the ring is a separate, classic-Win32-only future phase.
- Not yet covered: elevated (admin) windows require an elevated/uiAccess host to hook and to inject into.

## Roadmap

- [x] Ring overlay + hold-to-open hook (v0.1–0.2)
- [x] Center hub → native context-menu fall-through (v0.2)
- [x] Slice actions: hotkey / launch / native (v0.3)
- [x] Icon bubbles + Logitech-style optics; hub names the hovered action (v0.4)
- [x] Move-guard (right-drag passthrough) + per-app denylist (v0.5)
- [ ] **#4 — user-configurable slices** (JSON config under `%LOCALAPPDATA%`, later a C# WPF drag-&-drop editor)
- [ ] Optional optic: Logitech-style pill tooltip beside the hovered bubble (needs a larger overlay window)
- [ ] Sub-bubbles / folders and per-app profiles (like Logitech)
- [ ] Future phase: Windhawk `TrackPopupMenu` hook to render real native menu items inside the ring (classic Win32 apps only; pair with forcing the classic shell menu on Win11)

## License

[WTFPL](../../LICENSE)
