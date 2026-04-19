# Emoji Picker

Replaces the Windows 11 emoji dialog (Win+.) with a Windows 10-inspired picker.

## What it does

Windows 11 ships an emoji dialog that many users find slower and visually busier than the Windows 10 one. This mod intercepts Win+. and opens a lightweight replacement rendered with Direct2D / DirectWrite.

## Features

- **Win+. interception** — blocks the Windows 11 dialog and opens this picker instead (can be disabled to compare)
- **Secondary shortcut** — optional extra hotkey (`Ctrl+.`, `Ctrl+Space`, `Alt+.`)
- **Real-time search** — ~1660 emoji (Unicode 15.0) indexed with German and English keywords
- **Category tabs** — nine categories plus a Recent tab that persists across sessions
- **Theme** — follows system dark/light mode automatically
- **Flags toggle** — hide the Flags category on systems that render flags as two-letter codes

## Settings

| Setting | Default | Description |
|---|---|---|
| Intercept Win+. | `true` | Block the Windows emoji dialog and open this picker instead |
| Custom shortcut | `Ctrl+.` | Additional keyboard shortcut; also supports `Ctrl+Space`, `Alt+.`, or disabled |
| Hide Flags category | `false` | Hide national flags from the picker and tab bar |

## Requirements

- Windows 10/11
- Injected into `explorer.exe` (hotkeys work globally while Explorer runs)

## Implementation notes

- Low-level keyboard hook on the Explorer process catches the configured shortcut and signals a hidden picker window
- Emoji table generated from `unicode.org/emoji-test.txt` (emoji 15.0)
- Search uses locale-invariant lowercase to avoid the Turkish-I bug
- Recent list is stored in the Windhawk mod settings

## License

[WTFPL](../../LICENSE)
