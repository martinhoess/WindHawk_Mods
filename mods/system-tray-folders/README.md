# System Tray Folders (fork)

> Group icons in the Windows 11 "Show hidden icons" tray flyout into folders — with an extra **expanded groups** style that shows each folder's icons directly under a header, no click needed.

Fork of [System Tray Folders](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/system-tray-folders.wh.cpp) 1.0.0 by [Ris Peng](https://github.com/RisPNG). The first commit of this folder is the unchanged upstream file, the changes sit on top of it, so `git diff` against that commit shows exactly what this fork adds.

**License:** GPL-3.0-only, like the original. The repository's WTFPL does not apply to this mod.

## Changes in this fork

- New setting **Folder style**:
  - *Folder tiles* — the original behavior (a tile per folder, click to open).
  - *Expanded groups* — each folder becomes a header row followed by its icons. The header shows the folder name right-aligned and the folder glyph at the far right, in a dimmed text color with a divider below. Headers can't be clicked.
- In groups mode a divider row separates the groups from the icons that aren't in any folder (after the last group with *Before other icons*, before the first one with *After other icons*).
- Header and divider use theme resources (`TextFillColorTertiaryBrush`, `DividerStrokeColorDefaultBrush`), so they follow light and dark mode.

## Installing

Disable or uninstall the original System Tray Folders first. Both hook the same flyout, so running them side by side groups the icons twice.

## Use together with Tray Overflow List

The groups style is meant for [Tray Overflow List](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/tray-overflow-list.wh.cpp), which turns the flyout into a one-column list with app names. Both mods must be enabled. Tray Overflow List only restyles real tray icon rows and leaves the headers alone, so each header gets a full-width row. In the plain icon grid a header only gets one cell and is cut off.

The list uses one fixed row height for every item, so headers and the divider row are as tall as an icon row.

## Settings

| Setting | Default | Description |
|---|---|---|
| Folders | — | Name, optional glyph (e.g. `U+E7FC`) and icon patterns (`*`/`?` wildcards) per folder |
| Folder position | Before other icons | Where folders/groups appear relative to icons that aren't in a folder |
| Show empty folders | `false` | Also show folders that currently have no icons |
| Folder style | Folder tiles | *Folder tiles* (original) or *Expanded groups* (this fork) |

See the readme inside the mod file for how icon patterns are matched.

## Versioning

`<upstream version>-mh.<n>`, e.g. `1.0.0-mh.1` = first revision of this fork on top of upstream 1.0.0.
