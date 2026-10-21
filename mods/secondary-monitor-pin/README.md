# Secondary Monitor Pin (fork)

> Keeps windows on secondary monitors pinned so they appear on all virtual desktops — with a list of monitors that are left out.

Fork of [Secondary Monitor Pin](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/secondary-monitor-pin.wh.cpp) 1.1 by [tafadias](https://github.com/tafadias). The first commit of this folder is the unchanged upstream file, the changes sit on top of it, so `git diff` against that commit shows exactly what this fork adds.

**License:** MIT, like the original. The repository's WTFPL does not apply to this mod.

## Changes in this fork

- New setting **Excluded monitors**: monitors listed there are treated like the primary monitor, so windows on them are not pinned (and are unpinned again when the mod had pinned them). Use the device name, e.g. `DISPLAY2`; `\\.\DISPLAY2` works too, case doesn't matter.
- Every time the settings are loaded, the mod log lists all connected monitors with name, resolution and position (`Monitor DISPLAY2: 3840x2160 at (x,y)`), so the name can be looked up instead of guessed.

`DISPLAYn` is a slot number, not a hardware ID. It can change after replugging monitors or when a virtual display comes and goes.

## Settings

| Setting | Default | Description |
|---|---|---|
| Excluded monitors | *(empty)* | Monitors that behave like the primary monitor, one per entry, e.g. `DISPLAY2` |

All other settings are unchanged from upstream, see the readme inside the mod file.

## Versioning

`<upstream version>-mh.<n>`, e.g. `1.1-mh.1` = first revision of this fork on top of upstream 1.1.
