# Snes9x 3D

## Overview

**Snes9x 3D puts the SNES' hardware planes at different depths on the 3DS' 3D screen.**

The SNES draws a fixed set of background planes and a sprite plane, and games
use them for parallax scrolling: distant scenery on one plane, the playfield on
another, the HUD on a third. Those planes are defined by the console's hardware
rather than by each game's own logic, so an emulator can give every one of them
a real depth instead of flattening them into a single picture. See
[Per-layer 3D depth](#per-layer-3d-depth) for how to set it up.

This project is a fork of [matbo87/snes9x_3ds](https://github.com/matbo87/snes9x_3ds),
itself a fork of the legacy snes9x_3ds codebase by [bubble2k](https://github.com/bubble2k16/snes9x_3ds), and continues that work with a modernized architecture and improved user experience.
It builds with current devkitARM, libctru and citro3d releases (as of June 2026). Optional assets are available in the dedicated asset repository: [snes9x_3ds-assets](https://github.com/matbo87/snes9x_3ds-assets).

It works on all 2DS and 3DS models.
Old 2DS/3DS mainly struggle with Super FX and SA-1 games, but most SNES titles run well.

Feedback and bug reports are welcome.

## Main features

* Per-layer stereoscopic depth, configurable per game
* Improved rendering for HDMA-heavy games and mosaic effects
* SNES refresh rate matching (60.1 Hz for NTSC, 50 Hz for PAL)
* NDSP audio output
* Rich visual customization with thumbnails, themes, per-game backgrounds and overlays
* Crop and overscan
* Improved cheat management
* Extended hotkey options and screen swap support
* Directory caching for faster ROM list loading

## Per-layer 3D depth

Each SNES plane can be placed at its own depth, so that a game's parallax
backgrounds actually sit behind the playfield on the 3D screen.

Open the pause menu and go to the **3D Depth** tab, then turn on **Per-Layer 3D
Depth**.

### One depth per priority, not per plane

The SNES does not composite whole planes at one depth. It interleaves each
background's two tile priorities with the four sprite priorities, so in Mode 1
the order from front to back is:

    BG3 prio 1   (when $2105 bit 3 is set)
    OBJ prio 3
    BG1 prio 1
    BG2 prio 1
    OBJ prio 2
    BG1 prio 0
    BG2 prio 0
    OBJ prio 1
    BG3 prio 1   (when that bit is clear)
    OBJ prio 0
    BG3 prio 0
    Backdrop

A plane's high-priority tiles sit in front of sprites that are themselves in
front of that same plane's low-priority tiles. Each of those slots therefore
gets its own slider: BG1 to BG4 at both priorities, and sprites at all four.

BG3's high priority appears twice in that list, and so it has two sliders --
**BG3 prio 1** and **BG3 prio 1 front**. Bit 3 of `$2105` decides which of the
two places it occupies, and they are at opposite ends of the stack: behind the
sprites, or in front of all of them. The content differs to match, since a game
sets that bit precisely when it wants something over everything else, so one
depth cannot serve both. Only one of the two is ever in the frame at a time,
and the other greys out. A game that flips the bit part-way down a frame gets
both, each band at its own depth.

That distinction is not academic. Super Metroid draws its interface *and* the
Crateria rain on BG3, and the only thing separating them is the priority bit --
the interface on priority 1, the rain on priority 0. One depth per plane forces
them together; one depth per slot lets the interface float in front while the
rain sits back in the scene.

### Splitting a background's two priorities

A background stores one tile per cell, so its two priorities are not two
pictures: they are one grid whose cells each belong to one or the other. Giving
them different depths slides those cells apart, and nothing exists behind the
gap.

Rather than leave a hole there, the renderer extends the nearest tile of the
priority that sits further back across each boundary, clipped to the width the
two pull apart. Where that priority is a continuous structure the other one
overlaps -- Super Metroid's distant Crateria pillars behind its near terrain,
both on BG1 -- the strip shows the pillar continuing behind the terrain, which
is what would actually be there.

It is an approximation rather than recovery: the strip is invented from the
nearest tile, so where that tile is unrelated content the band will smear. At a
few pixels wide it tends to read as a shadowed edge.

Two arrangements are left alone. Nothing is drawn when the two priorities share
a depth, and nothing is drawn when the *high* priority is the one further back,
since filling that would mean painting over the low priority -- more damage than
the gap. Keep the low priority as the further-back one, which is also the
natural reading of a background.

The fill covers the ordinary background path. Offset-per-tile backgrounds
(Modes 2 and 4), mosaic and the hi-res Mode 5/6 path do not have it yet, so
splitting priorities there can still tear.

### Two arrangements, two sets of sliders

Appendix A-19 gives the SNES two composite orders, not one. Modes 0 and 1 stack
up to four backgrounds with the sprites between them; Modes 2 to 7 stack two and
interleave the sprites differently, so that BG2's high priority sits *behind*
sprite priority 2 there where in Mode 1 it sits in front. A slot's place in the
frame, and so the depth that suits it, belongs to one arrangement or the other.

Each therefore keeps its own set of depths, and **Sliders for** chooses which
set the list is showing. The arrangement the game is in is marked with the mode
it is in -- *Modes 0-1 (current: 1)* -- and the menu opens on it. What the
greying and the blank previews mean is on that setting's description. The other one stays reachable, so a game
that plays in Mode 1 and draws its map in Mode 3 can have both set up without
having to be in the right screen at the time.

Within an arrangement the order is the hardware's and never changes. The slots
the current mode does not draw grey out in place -- they do not move and they do
not disappear, and they stay adjustable, because a game is free to change mode
while the menu is closed. Only Mode 0 has four backgrounds; Mode 1 has three,
Modes 2 to 5 have BG1 and BG2, Mode 6 has BG1 alone, and Mode 7 leaves every
background to the sprites. The mode itself is named on the **Per-Layer 3D
Depth** row.

The list ends with the **Backdrop**, which takes no slider: it is one colour
filling the screen and always furthest back, so a depth for it would mean
nothing. It is there because the stack reads better closed, and because its
preview shows what the rest of the frame is built on.

Beside each slider is a preview of what that slot alone holds in the frame behind
the menu, rendered by drawing the frame again with the other slots held back. An
outlined but empty preview means the game is drawing nothing on that slot in this
frame, which is as useful to know as the picture itself. Each preview pixel takes
the brightest of the pixels it stands for rather than their average, so that
something sparse -- rain, a HUD, a few sprites -- still shows at this size
instead of averaging away to black. A layer therefore previews brighter than it
really is.

The rows are twice the usual height to fit those previews, so about six sliders
are on screen at once and the rest are a scroll away.

Previews are of the frame behind the menu, and that frame was drawn in one
arrangement. The other arrangement's sliders keep their rows and their frames but
come out black, whether the list opened on them or was switched to them: those
planes are not what is on screen, and showing the current frame's content next
to them would say something untrue about the depths being set. The two sets are independent -- the sliders shown are always the ones being
changed.

Both the greying and the previews come from the frame the game stopped on. A game
that changes mode between rooms, or part-way down a frame, will look different
the next time the menu is opened.

### Edge cropping

A slot with depth moves sideways between the eyes, so it stops short of one edge
of one eye and something has to give there. **Edge cropping** decides what.

* **Per layer** (the default) cuts each slot's geometry back to the visible
  screen before the shift moves it. The slot then runs out at one edge over a
  strip exactly as wide as its own shift, straight down the frame, and whatever
  sits behind it shows through there. A slot left at `0` keeps the full width
  whatever its neighbours are set to.
* **Whole frame** leaves one strip undrawn per eye instead -- the right eye at
  its left edge, the left eye at its right -- as wide as the largest shift the
  frame used. Nothing shows through it at all, at the cost of a few pixels from
  every slot, including the ones that never moved. The width is taken over the
  slots the frame actually drew, so a large depth sitting in the arrangement the
  game is not currently in does not trim the picture.
* **None** draws the tiles anyway. A game keeps its tilemap valid only where it
  means to draw, so what appears is whatever it last left there: real scenery in
  a game whose map wraps, unrelated tiles in one that reuses the space. How far
  a tile hangs over an edge follows that layer's scroll, which HDMA can change
  from one band of the frame to the next, so the strip can step in and out down
  the screen.

Cutting the tiles back happens while the frame is drawn, so changing to or from
**None** shows on the next frame the game draws rather than on the paused one.
**Whole frame** applies to the paused frame immediately.

### Using the sliders

* `0` places that slot at the screen, where a flat picture sits.
* Higher values push it further behind the screen, up to `11`.
* Negative values pull it in front of the screen, down to `-11`.

Depth is stored per game, because only the game knows which slot it uses for
distant scenery, which one carries the playfield and which one is the HUD. A
good starting point is to leave the playfield at `0`, give the slots that scroll
more slowly a positive depth, and put the interface slightly negative.

Super Metroid, for example, works well at BG1 prio 0 `4` for the distant
pillars, BG1 prio 1 `0` for the terrain, BG2 prio 0 `8`, BG3 prio 0 `10` for the
rain, BG3 prio 1 `-4` for the interface, and sprites `-1`.

Pause the game while adjusting: the top screen keeps showing the paused frame
and redraws it as you move the sliders, so the effect can be judged directly.
While the **3D Depth** tab is open the game screen is left alone -- no dimming
and no "press START" over it -- since it is the thing being judged.

Notes:

* The physical 3D slider still scales the whole effect. The **3D Intensity**
  setting does not: it applies to the border art behind the game screen, and
  a depth set here stays worth the pixels it says whatever that is set to.
* The backdrop fills the screen, so it has no depth of its own and is always
  furthest back.
* The feature needs the 3D screen, so it is unavailable on 2DS models, and it
  needs **Enhanced Resolution** to be off: a shift is added to the frame's
  geometry in render-target pixels, and that path draws at twice the scale, so
  every depth would come out at half the parallax asked for.
* Mode 7 encodes depth as a single bit rather than a slot, so its planes are not
  covered.
* Both eyes are rendered from the same frame's geometry, so the SNES
  compositing rules (priorities, windows, colour math) stay exactly as they are
  in 2D. The SNES itself is emulated once per frame; what is repeated per eye is
  replaying the frame's vertices, and only while the 3D slider is open.
* What happens at the screen edges is a setting of its own -- see below.

## Setup

* A modded 3DS is required; DSP firmware (`3ds/dspfirm.cdc`) is needed for sound output.
* Install via [Universal Updater](https://universal-team.net/projects/universal-updater.html), or install the latest `.cia` from [Releases](https://github.com/matbo87/snes9x_3ds/releases).
* Optional: download asset packs from [snes9x_3ds-assets releases](https://github.com/matbo87/snes9x_3ds-assets/releases).

ROMs can be stored in any folder. ZIP files are not supported.

Supported ROM formats:
* `.smc`
* `.sfc`
* `.fig`
* `.bs`
* `.bsx`

Configs, saves and imported assets are stored in `sd:/3ds/snes9x_3ds`.

### 3DSX version

* Copy `snes9x_3ds.3dsx` to `sd:/3ds/snes9x_3ds`
* Start it from the Homebrew Launcher

## Assets (images and cheats)

Assets are provided in a dedicated asset repository:
* [matbo87/snes9x_3ds-assets](https://github.com/matbo87/snes9x_3ds-assets)

Notes:

* The repository follows a 1G1R-style selection.
* Naming is strict No-Intro style for matching.

## Building from source

* Install devkitPro and 3DS toolchain packages (including devkitARM, libctru, citro3d). If needed, follow the [devkitPro pacman guide](https://devkitpro.org/wiki/devkitPro_pacman).
* The Makefile is based on TricksterGuy's [3ds-template](https://github.com/TricksterGuy/3ds-template).

Required command-line tools in `PATH`:

* For `3dsx` builds: `tex3ds`, `smdhtool`, `3dsxtool` (from the devkitPro 3DS toolchain).
* For `cia` builds: `makerom` in addition to the above.

Common build targets:

* `make 3dsx`
* `make citra`
* `make 3dslink` (sends the `.3dsx` to your Homebrew Launcher)

This repository bundles `makerom` binaries under `makerom/` for convenience.
Bundled binary provenance is documented in `makerom/BINARY_SOURCES.md`.

### Emulator status

* Citra (nightly ≤ 2104): working
* Azahar: Mode7 1024x1024 texture renders as a solid yellow texture

## Development and Contributions

New work usually lands on `develop` first. Merges to `master` create build artifacts via GitHub Actions. Tagged GitHub [releases](https://github.com/matbo87/snes9x_3ds/releases) are the official stable releases.

Community PRs are welcome. For larger changes, a short issue first is appreciated.
Please keep PRs focused and test on hardware where possible.
AI-assisted code is fine, but contributors are responsible for understanding and validating the code they submit.
Broad, risky, hard-to-review PRs may be closed or split into smaller changes. Prototype work may still be credited if it informs a later implementation.

AI note: I use AI assistants as part of my development workflow, including code review, debugging, planning, implementation and documentation. All changes are reviewed and adjusted by me before they are merged.

## Screenshots

<table>
  <tr>
    <td width="50%" align="center"><img src="screenshots/dark-mode-file-menu.png" alt="Start screen" valign="bottom"></td>
    <td width="50%" align="center"><img src="screenshots/retroarch-pause-screen.png" alt="Super Mario World" valign="bottom"></td>
  </tr>
  <tr>
    <td valign="top" width="50%" align="center">Start screen, "Game Thumbnail" option enabled</td>
    <td valign="top" width="50%" align="center">Pause screen, per-game overlay enabled</td>
  </tr>
  <tr><td colspan="2"></td></tr>
  <tr></tr>
  <tr>
    <td width="50%" align="center"><img src="screenshots/aladdin-pp-cheats.png" alt="Aladdin" valign="bottom"></td>
    <td width="50%" align="center"><img src="screenshots/dkc-hotkeys.png" alt="Donkey Kong Country" valign="bottom"></td>
  </tr>
  <tr>
    <td valign="top" width="50%" align="center">Cropped top & bottom, cheats enabled</td>
    <td valign="top" width="50%" align="center">Applied hotkeys</td>
  </tr>
  <tr><td colspan="2"></td></tr>
  <tr></tr>
  <tr>
    <td width="50%" align="center"><img src="screenshots/sf2-cropped-border-cover.png" alt="Super Street Fighter II" valign="bottom"></td>
    <td width="50%" align="center"><img src="screenshots/issd-screen-swap.png" alt="International Superstar Soccer Deluxe" valign="bottom"></td>
  </tr>
  <tr>
    <td valign="top" width="50%" align="center">Crop & overscan, scanlines enabled</td>
    <td valign="top" width="50%" align="center">Swapped screen</td>
 </tr>
 <tr>
    <td width="50%" align="center"><img src="screenshots/tg2-hdma.png" alt="Top Gear II" valign="bottom"></td>
    <td width="50%" align="center"><img src="screenshots/savestate-preview-bsx.png" alt="Excitebike - Bunbun Mario Battle" valign="bottom"></td>
  </tr>
  <tr>
    <td valign="top" width="50%" align="center">In-Frame Palette Changes enabled</td>
    <td valign="top" width="50%" align="center">BS-X game, savestate preview</td>
 </tr>
 </table>
 <br>

## Frequently Asked Questions

### A game runs slow. How can I improve performance?

* Increase `Frameskips` (more than 2 isn't recommended)
* Set `Frame Sync Method` to `Sleep Sync`
* Set `In-Frame Palette Changes` to `Disabled Style 1` or `Disabled Style 2`
* Set `SRAM Auto-Save Delay` to 60 seconds or disable it (SD Card speed is slow on 3DS)
* Disable 3D and/or on-screen display settings

### A game looks or sounds wrong. What can I try?

* Set `In-Frame Palette Changes` to `Enabled`
* Increase `Audio Buffer Size` if audio crackles, skips or stutters
* Enabled cheats can break visuals or gameplay; disable cheats and reload the game
* Check if your ROM is valid (No-Intro is highly recommended; ROM hacks often have issues)
* Check the [known issues](KNOWN_ISSUES.md)

### Cheats are not working properly

* Cheat support is only lightly tested and some codes may not work correctly
* Use cheats with caution: broken codes can affect gameplay or damage save data

### Satellaview (BS-X) games

Satellaview games are supported, but compatibility is hit-or-miss.
See [Known Issues](KNOWN_ISSUES.md#satellaview-bs-x-games) for details and per-game status.

## License

Some files may carry their own license headers, but because this project includes the Snes9x core (`source/Snes9x/`), redistribution of the combined project follows the Snes9x non-commercial license terms.

See:
* [LICENSE.md](LICENSE.md)
* [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## Credits

* The Snes9x team for the SNES emulator core, and the libretro Snes9x core maintainers for ongoing reference work
* bubble2k, original author of [snes9x_3ds](https://github.com/bubble2k16/snes9x_3ds), for creating the excellent base this fork builds on
* Wyatt-James for his [snes9x_3ds fork](https://github.com/Wyatt-James/snes9x_3ds); this fork adapts a few safety, audio and stability fixes from his work
* ramzinouri's [snes9x_3ds fork](https://github.com/ramzinouri/snes9x_3ds) inspired the image border/background and theme support
* willjow's [snes9x_3ds fork](https://github.com/willjow/snes9x_3ds) revived the project after development had gone quiet
* The Citra/Azahar teams for making 3DS emulator testing and debugging practical
* Everyone reporting issues, testing games and suggesting improvements
