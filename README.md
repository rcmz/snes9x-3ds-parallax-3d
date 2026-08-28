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

That distinction is not academic. Super Metroid draws its interface *and* the
Crateria rain on BG3, and the only thing separating them is the priority bit --
the interface on priority 1, the rain on priority 0. One depth per plane forces
them together; one depth per slot lets the interface float in front while the
rain sits back in the scene.

### Using the sliders

* `0` places that slot at the screen, where a flat picture sits.
* Higher values push it further behind the screen.
* Negative values pull it in front of the screen.

Depth is stored per game, because only the game knows which slot it uses for
distant scenery, which one carries the playfield and which one is the HUD. A
good starting point is to leave the playfield at `0`, give the slots that scroll
more slowly a positive depth, and put the interface slightly negative.

Super Metroid, for example, works well at BG1 prio 0 `0`, BG2 prio 0 `8`,
BG3 prio 0 `10` for the rain, BG3 prio 1 `-4` for the interface, and sprites
`-1`.

Pause the game while adjusting: the top screen keeps showing the paused frame
and redraws it as you move the sliders, so the effect can be judged directly.
A slider set beyond the margin the paused frame was built with previews clamped
to that margin and takes full effect once the game runs again.

Notes:

* The physical 3D slider still scales the whole effect, and the existing
  **3D Intensity** setting scales it further.
* The backdrop fills the screen, so it has no depth of its own and is always
  furthest back.
* The feature needs the 3D screen, so it is unavailable on 2DS models, and it
  needs **Enhanced Resolution** to be off, because the 512px render path leaves
  no room for the off-screen margin that shifted planes draw into.
* Mode 7 encodes depth as a single bit rather than a slot, so its planes are not
  covered.
* Both eyes are rendered from the same frame's geometry, so the SNES
  compositing rules (priorities, windows, colour math) stay exactly as they are
  in 2D. The SNES itself is emulated once per frame; what is repeated per eye is
  replaying the frame's vertices, and only while the 3D slider is open.
* A slot can only be shifted as far as the game's own tilemap reaches. Where a
  game leaves no valid tile data just outside the visible screen, a slot given
  depth can uncover a thin strip at one edge showing what is behind it. Giving
  the slots the camera follows a depth of `0` avoids this, since those are the
  ones whose off-screen data is least likely to be there.

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
