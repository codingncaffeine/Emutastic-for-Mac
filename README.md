![Emutastic](src/Emutastic/Assets/banners%20and%20icons/emutastic-banner-scaled.png)

# Emutastic for Mac

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

A native **macOS** emulator frontend for Apple Silicon that brings features rarely found together on the
Mac: **RetroAchievements with hardcore-mode compliance, GitHub cloud sync of your saves and library,
launch-time ROM-hack patching, in-app game manuals, gameplay recording, and a deep visual theme editor**.
Your collection is organized by console in a clean library; emulation is handled by
[libretro](https://www.libretro.com/) cores loaded at runtime — no cores are bundled.

It's the same app as the [Windows](https://github.com/codingncaffeine/Emutastic) and
[Linux](https://github.com/codingncaffeine/Emutastic-For-Linux) builds, with the platform layer rebuilt
for the Mac: **.NET 10 + Avalonia (Avalonia.Native)**, a native **Metal/OpenGL** presenter, **SDL3 /
CoreAudio** audio, **SDL3 / GameController** input, **VideoToolbox** hardware recording, and `dlopen`
core loading.

> **🚧 Status — early port, in active development.** The native library/UI runs on Apple Silicon today
> (window, theming, every Preferences tab, library navigation). Still being brought up: the game-host
> present path (so games actually render), SDL3 audio + controllers, video recording, snap-video
> previews, libretro core download wiring (`apple/osx/arm64`), and `.app` packaging + signing. The
> highlights and feature list below describe the **target** — parity with the Windows and Linux builds.

> **Legal notice:** This project is a frontend only. It does not include, distribute, or facilitate the
> acquisition of any copyrighted software, ROM images, BIOS files, or other proprietary system files.
> You are solely responsible for ensuring you have the legal right to use any software you load.

## Highlights

- 📺 **EmuTV — living-room mode** — a controller-only, big-screen couch interface: browse and launch
  your whole library from the sofa. On the Mac, games render *inside* the EmuTV window (no second
  window, no Dock/menu-bar flash) and drop you right back to the couch shell on exit. *(See below.)*
- 🏆 **RetroAchievements** — full hardcore-mode compliance, in-game unlock toasts, a trophy case with
  activity heatmap, and friend unlock feeds. Native achievement support is rare on the Mac.
- ☁️ **GitHub cloud sync** — battery saves and your whole library database follow you across Mac,
  Windows, and Linux, with optional AES-256-GCM encryption, all in a private repo you control.
- 🔧 **Launch-time ROM patching** — apply IPS / BPS / UPS hacks and translations at launch; the patched
  game becomes its own library entry, your original ROM left untouched.
- 🎨 **Deep theming** — Dark / Light / OLED / Midnight built-ins plus a visual editor with live color
  tokens, custom backgrounds, and `.emutheme` export/import.
- 📖 **In-app game manuals** · 🎥 **gameplay recording** (native VideoToolbox H.264 / HEVC / ProRes) ·
  📝 **per-game notes**
- 🎛️ **Full SDL3 controller support** — real controller names, analog-stick-as-D-pad, gamepad save
  states, disc swapping, and per-system cheats.
- 🗂️ **Clean, console-organized library** with automatic box art and metadata (OpenVGDB + libretro
  thumbnails, optional ScreenScraper) — no account required to start.
- 🍎 **Native Mac fit** — Apple Silicon build, real macOS window controls, and a fully self-contained
  `.app` (no Homebrew, no .NET install, nothing to set up).

**Compared to the Windows build,** the Mac port now leaves out just one thing: the high-end **PlayStation 2
/ PlayStation 3** cores, which have no Apple Silicon libretro build. Everything else — **EmuTV** included
(new in v0.7.9) — is the same app.

---

## 📺 EmuTV — Living-Room Mode

<img src="src/Emutastic/Assets/banners%20and%20icons/emutv_logo.png" width="280" alt="EmuTV"/>

A full controller-driven, big-screen couch interface — browse and launch your entire library from the
sofa without touching a keyboard or mouse. A 1:1 port of the Windows/Linux living-room mode, rebuilt
natively for macOS.

- **Controller-first fullscreen UI** — a themed system carousel and game lists you drive entirely from
  the gamepad.
- **ES-DE theme engine** — renders EmulationStation Desktop Edition themes, with a built-in default plus
  in-app theme browsing, downloading, and importing.
- **Seamless in-window game launches (a macOS-native solution)** — games render *inside* the EmuTV window
  through a shared-surface compositor, so there's no second window, no focus handoff, and no Dock or
  menu-bar flash: it stays fullscreen from launch to quit and drops you straight back to the couch shell.
  GL and 3D/Vulkan cores (N64, GameCube, PSP, …) both work here.
- **SteamGridDB artwork** and an **animated TV preview** round out the presentation.

**Getting in:** hold **L2 + R2 + L3 + R3** (both triggers + both sticks clicked) for ~2 seconds from the
library, or press **F9**. Quit a game back to the couch with the same chord.

**In-game controller combos:** save state **L3 + R2** · load latest state **R3 + L2** · swap disc
**L3 + Start** — all rebindable per system under **Preferences → Controls**.

---

## Requirements

- An **Apple Silicon** Mac (arm64), **macOS 13 or newer**. That's the whole list.

A packaged release is **fully self-contained** — the `.app` bundles the .NET 10 runtime, the
Avalonia/Skia UI stack, and **SDL3** (audio + controllers). No Homebrew, no .NET install, nothing to
set up.

Everything else is fetched **inside the app** (Preferences → Cores / Extras) on demand — you never
install any of it by hand:

| In-app download | Where | What it's for |
|---|---|---|
| **libretro cores** (`.dylib`) | Preferences → Cores | the per-system emulator cores |
| **DAT files** | Preferences → Cores / Extras | ROM identification |

Gameplay recording needs **no download** on macOS — it encodes natively through Apple's VideoToolbox
(hardware H.264/HEVC/ProRes), unlike the ffmpeg download Windows uses.

Video snap previews use **libVLC**, bundled in the app (arriving in an upcoming build).

> The Homebrew packages listed under [Building](#building) (`dotnet`, `sdl3`, `ffmpeg`, …) are **only
> needed to build from source** — end users running the `.app` never touch Homebrew.

---

## Installing & first launch

Download `Emutastic-<version>-osx-arm64.zip` from the
[Releases](https://github.com/codingncaffeine/Emutastic-for-Mac/releases) page, unzip it, and drag
**Emutastic.app** into your Applications folder.

Emutastic is **self-signed**, not notarized through Apple's paid Developer Program — it's a free,
non-profit app, so paying Apple's yearly fee isn't worth it. Because of that, macOS shows **two
one-time prompts**. Both are expected and safe to approve:

**1 — "Apple cannot check it for malicious software" (first launch).**
Gatekeeper blocks the first *double-click* of any app that isn't notarized. To get past it, just open
it a different way once:

- **Right-click** (or Control-click) **Emutastic.app → Open**, then click **Open** in the dialog.
- *(Terminal alternative: `xattr -dr com.apple.quarantine /Applications/Emutastic.app`)*

After that first time, it opens normally on every launch.

**2 — "Emutastic would like to access files in your Desktop folder" (first time you launch a game).**
macOS asks **every** app once before it may read your Desktop / Documents / Downloads — click
**Allow** (or **OK**). Because Emutastic is signed with a stable certificate, your choice is
**remembered**: you're only asked once, and your **ROMs can live in any folder** with no need to
change file or folder permissions. *(If you ever update and get asked again, allow it once more — that
just means the signing certificate changed.)*

---

## Supported Systems

<details>
<summary><strong>35 of 36 systems playable on Apple Silicon</strong> — PlayStation 2 excluded (click to expand)</summary>

| System | Tag | Core (priority order) | BIOS |
|---|---|---|---|
| NES | NES | nestopia → quicknes → fceumm | No |
| Famicom Disk System | FDS | nestopia | `disksys.rom` |
| SNES | SNES | snes9x → bsnes | No |
| Nintendo 64 | N64 | mupen64plus_next → parallel_n64 | No |
| GameCube | GameCube | dolphin | No |
| Game Boy | GB | mgba → gambatte → sameboy | No |
| Game Boy Color | GBC | mgba → gambatte → sameboy | No |
| Game Boy Advance | GBA | mgba | Optional |
| Nintendo 3DS | 3DS | azahar | No |
| Nintendo DS | NDS | desmume → melonds | No |
| Virtual Boy | VirtualBoy | mednafen_vb | No |
| Genesis / Mega Drive | Genesis | genesis_plus_gx → picodrive | No |
| Sega CD / Mega CD | SegaCD | genesis_plus_gx | Region BIOS |
| Sega 32X | Sega32X | picodrive | No |
| Sega Saturn ² | Saturn | mednafen_saturn → yabause | Region BIOS |
| Master System | SMS | genesis_plus_gx → picodrive | No |
| Game Gear | GameGear | genesis_plus_gx | No |
| SG-1000 | SG1000 | genesis_plus_gx | No |
| Dreamcast | Dreamcast | flycast | No |
| PlayStation | PS1 | mednafen_psx_hw → mednafen_psx | Region BIOS |
| ~~PlayStation 2~~ ¹ | PS2 | *not supported on Apple Silicon* | — |
| PSP | PSP | ppsspp | No |
| TurboGrafx-16 | TG16 | mednafen_pce → mednafen_pce_fast | No |
| TurboGrafx-CD | TGCD | mednafen_pce → mednafen_pce_fast | `syscard3.pce` |
| Neo Geo Pocket | NGP | mednafen_ngp | No |
| Neo Geo Pocket Color | NGPC | mednafen_ngp | No |
| Neo Geo | NeoGeo | geolith | `neogeo.zip` + `aes.zip` |
| Neo Geo CD | NeoCD | geolith | `neogeo.zip` + `aes.zip` + `neocdz.zip` |
| Arcade | Arcade | fbneo + mame2003-plus | No |
| Atari 2600 | Atari2600 | stella | No |
| Atari 7800 | Atari7800 | prosystem | No |
| Atari Jaguar | Jaguar | virtualjaguar | No |
| ColecoVision | ColecoVision | gearcoleco → bluemsx | No |
| Vectrex | Vectrex | vecx | No |
| 3DO | 3DO | opera | `panafz10.bin` |
| Philips CD-i | CDi | same_cdi | No |

</details>

Cores are downloaded from the **Apple Silicon** libretro build servers
(`buildbot.libretro.com/nightly/apple/osx/arm64`) on demand — the same lineup as upstream, as `.dylib`.
Every system above has an Apple Silicon core **except PlayStation 2**.

¹ **PlayStation 2 is not supported on Apple Silicon.** The `pcsx2` (LRPS2) libretro core has no
`apple/osx/arm64` build, and no other libretro core covers PS2; it may return if an upstream arm64 build
appears.

² **Sega Saturn** runs on **Beetle Saturn** (`mednafen_saturn`) or **Yabause** — the Kronos core has no
Apple Silicon build, so it's dropped from the macOS lineup.

---

## BIOS Files

Place BIOS files in `~/Library/Application Support/Emutastic/System/` (or `PortableData/System/` next to
the executable in portable mode). The app also checks each system's ROM folder.

<details>
<summary><strong>BIOS file details by system</strong></summary>

**Sega CD** — `bios_CD_U.bin` (USA), `bios_CD_E.bin` (Europe), `bios_CD_J.bin` (Japan)

**Sega Saturn** — Beetle Saturn: `sega_101.bin` (JP v1.00), `mpr-17933.bin` (JP v1.01),
`mpr-17941.bin` (USA/EU v1.01).

**PlayStation** — USA: `scph5501.bin`, `scph1001.bin`, `scph7001.bin`. Europe: `scph5502.bin`. Japan: `scph5500.bin`

**TurboGrafx-CD** — Any of: `syscard3.pce`, `syscard2.pce`, `syscard1.pce`

**3DO** — Any of: `panafz10.bin`, `panafz1j.bin`, `goldstar.bin`

**Famicom Disk System** — `disksys.rom`

</details>

---

## ROM Import

Drag and drop ROMs onto the library or use **Import ROMs**. The app detects the console from file
extension, cleans the title, and hashes the ROM. For ambiguous formats (`.chd`, `.iso`, `.cue`, `.bin`),
a SHA1 lookup against DAT files is attempted first — if no match, a console picker is shown. `.zip` is
handled by the .NET BCL; `.7z`/`.rar`/`.tar`/`.gz` via SharpCompress (no native dependency).

**Multi-disc games** are auto-bundled into a single library entry via an `.m3u` playlist.

---

## Features

<details>
<summary><strong>Click to expand the full feature list</strong></summary>

<br>

<details>
<summary><strong>Themes</strong></summary>

Four built-in themes — **Dark** (default), **Light**, **OLED Black**, **Midnight Blue** — plus a full
visual editor with live color tokens and preview. Set custom background images with zoom, pan, and tile
controls. Export/import themes as `.emutheme` files to share or to pull in ones the community has made.

</details>

<details>
<summary><strong>Artwork & Metadata</strong></summary>

Box art, titles, developers, genres, and descriptions are filled in automatically — **no account
required**. By default Emutastic matches your games against **OpenVGDB** (a built-in local database) and
pulls box art from the **libretro thumbnail server**. Sign in to **ScreenScraper** (Preferences → Snaps)
to promote it to the primary source — community-edited, region-aware metadata with fuller coverage, plus
3D box art and downloadable game manuals. OpenVGDB stays on as the backup that fills anything
ScreenScraper misses.

</details>

<details>
<summary><strong>Controllers</strong></summary>

Controllers are detected through **SDL3** and identified by product name (CoreAudio / GameController on
macOS) — Xbox, DualSense/DualShock, and hundreds of others — instead of generic "Controller 1, 2, 3."
Button mappings are configurable per controller in **Preferences → Input**.

**Left analog stick works as movement input** on every old console with a digital joystick or D-pad —
push the stick on the NES, SNES, Genesis, Game Boy line, Saturn, Neo Geo, Atari, ColecoVision,
TurboGrafx, arcade games, and more, and your character moves. Diagonals are honored. The D-pad still
works exactly as before — use whichever you prefer.

**Save and load states from the gamepad** — a button chord saves or loads your latest state in any game
with no overlay needed, configurable per console in **Preferences → Controls**.

</details>

<details>
<summary><strong>RetroAchievements</strong></summary>

Earn achievements while playing via [RetroAchievements](https://retroachievements.org/). Sign in with
your RA account in **Preferences → Achievements**; unlocks appear in-game as toast notifications with a
full appearance editor. The **Achievements tab** gives you a trophy case, an activity heatmap, and
friends with unlock feeds and leaderboard toasts. CHD-based titles are supported.

**Hardcore mode** enforces every RetroAchievements hardcore rule (save-state loading blocked, cheats
blocked, no rewind/slow-motion/frame-advance, persistent on-screen indicator). See the
[Hardcore Compliance](https://github.com/codingncaffeine/Emutastic/wiki/Hardcore-Compliance) wiki page
for the line-by-line audit.

</details>

<details>
<summary><strong>Cloud Sync</strong></summary>

Sync battery saves and your library database across machines using your GitHub account — sign in with
one click in **Preferences → Backups** (device flow, no password stored) and a private repo is created
under your account. The same repository serves the Windows, Linux, and macOS apps: save on one machine,
pick up on another (battery saves are keyed by ROM hash, so each install must import the same ROM
files). Optional **AES-256-GCM encryption** with a passphrase you choose. See the
[Cloud Sync](https://github.com/codingncaffeine/Emutastic/wiki/Cloud-Sync) wiki page for details.

</details>

<details>
<summary><strong>Recording & Screenshots</strong></summary>

Capture screenshots and record gameplay clips with a hotkey. On macOS, recording encodes natively
through Apple's **VideoToolbox** (hardware H.264 / HEVC / ProRes) — no ffmpeg download required. Files
land in a per-game folder so screenshots, recordings, and saves stay organized in one place.

</details>

<details>
<summary><strong>Disc Swapping (FDS, PS1, Saturn, Sega CD)</strong></summary>

A button chord flips between discs/sides in-game on systems that need it, rebindable in
**Preferences → Controls → Disk Swap**. Multi-disc games are auto-bundled at import time — see the
[ROM Import](#rom-import) section.

</details>

<details>
<summary><strong>Game Notes</strong></summary>

Keep free-form notes on any game — passwords, where you left off, strategies — in a floating editor with
line numbers, find, and word-wrap/monospace toggles. Notes autosave as you type and ride your Cloud Sync
backup across machines. The window can be pinned on top and rolled up to its title bar — handy beside a
running game on a single display.

</details>

<details>
<summary><strong>Game Manuals</strong></summary>

Download a game's original PDF manual and read it in a built-in viewer — zoom, search, page thumbnails —
that reopens on your last-read page. Manuals are sourced from ScreenScraper (requires a ScreenScraper
login); coverage is best for popular console titles.

</details>

<details>
<summary><strong>Cheats</strong></summary>

Per-game cheats from the in-game menu or the library detail card. Game Genie / GameShark / raw codes
depending on system, with cheat-database import. See
**[Cheats](https://github.com/codingncaffeine/Emutastic/wiki/Cheats)** in the wiki for code formats per
system and the cores where cheats aren't supported.

</details>

<details>
<summary><strong>ROM Hacks</strong></summary>

Apply an IPS, BPS, or UPS patch to a base game right from the library. The patched game becomes its own
library entry — with its own saves — while your original ROM is left untouched, so there's no second
copy on disk. The patch is applied in memory at launch, and BPS/UPS patches are checksum-verified
against your ROM, so a mismatched or wrong-region copy is caught before it loads. Available on cartridge
systems (SNES, GBA, Game Boy / Game Boy Color, NES, Genesis, Nintendo 64, and more). See
**[ROM Hacks](https://github.com/codingncaffeine/Emutastic/wiki/ROM-Hacks)** in the wiki.

</details>

<details>
<summary><strong>Shaders, Bezels & Overlays</strong></summary>

Video shaders (CRT scanlines, LCD grids, and more) render through the slang-shader pipeline, saved per
game. Arcade and Neo Geo **bezels** and **Vectrex overlays** are downloadable extras that frame the
picture. **Turbo buttons** and **native macOS window controls** (Preferences → Theme — real minimize /
maximize / close, or the custom frameless look) round it out.

</details>

<details>
<summary><strong>Also included</strong></summary>

Core options · save states with screenshots · play-time tracking · one-click core downloads · BIOS
guidance. Diagnostic logs live in the `Logs/` folder of the data directory
(`~/Library/Application Support/Emutastic/Logs/`).

</details>

</details>

See the upstream [Emutastic wiki](https://github.com/codingncaffeine/Emutastic/wiki) for per-feature
detail — behavior is intended to match the Windows and Linux builds.

---

## Folder Layout

macOS keeps everything under Application Support (.NET maps both the config and data roots there, unlike
the Linux XDG split):

```
~/Library/Application Support/Emutastic/
    config.json
    library.db
    DATs/                        (No-Intro / Redump DATs — downloadable in-app)
    Cores/                       (libretro core .dylib files — downloadable in-app)
    System/                      (BIOS files)
    Saves/ (SRAM) / Save States/ / Screenshots/ / Recordings/ / Artwork/ / Manuals/ / Cheats/ / Themes/ / Logs/ / ...
```

### Portable mode

Drop an empty `portable.txt` next to the executable **or** launch with `--portable`, and **everything**
lives in `PortableData/` beside the executable — config, library, saves, screenshots, recordings,
artwork, BIOS, libretro cores, and imported ROMs. Paths are stored relative to `PortableData/`, so the
install travels intact.

---

## Building

Requires the **.NET 10 SDK** plus the runtime dependencies. Install everything via Homebrew:

```sh
brew install dotnet cmake pkg-config sdl3 ffmpeg
```

Build and run:

```sh
git clone git@github.com:codingncaffeine/Emutastic-for-Mac.git
cd Emutastic-for-Mac
dotnet build src/Emutastic.slnx -c Release

# Homebrew's dotnet doesn't register a global install location, so the app host
# needs DOTNET_ROOT pointed at it (packaged releases are self-contained and won't):
export DOTNET_ROOT="$(brew --prefix dotnet)/libexec"
src/Emutastic/bin/Release/net10.0/Emutastic
```

Two native libraries build from vendored C sources via clang as `.dylib` — `librcheevos` (RetroAchievements,
pinned v11.6.0 with an ABI-check harness) and `libchdr` (CHD disc images, built with CMake). *(These
MSBuild targets are being ported from the Linux `.so` build; until they land, RetroAchievements and CHD
identification are inert but the app builds and the library UI runs.)* The Linux Wayland presenter
(`libwlpresent`) is **not** used on macOS — its replacement is a native Metal/OpenGL game-host presenter.

---

## Credits

**Emulation** is handled by libretro cores maintained by their upstream authors — Emutastic bundles none
of them; the in-app core manager downloads them from the libretro build servers on demand. The lineup is
unchanged from upstream (Nestopia, snes9x, mGBA, Genesis Plus GX, Mednafen/Beetle, Dolphin, PPSSPP,
Flycast, FBNeo, MAME 2003-Plus, and more) — see the upstream
[Emutastic credits](https://github.com/codingncaffeine/Emutastic#credits) for the full per-core author
list. Please support those projects directly.

**Frameworks & libraries** (the macOS port shares the Linux port's cross-platform stack):

| Library | Purpose | License |
|---|---|---|
| [Avalonia](https://avaloniaui.net/) | Cross-platform UI; **Avalonia.Native** macOS backend (replaces WPF) | MIT |
| [SDL3](https://www.libsdl.org/) | Audio output + controllers; CoreAudio / GameController on macOS (replaces NAudio/WASAPI + XInput) | Zlib |
| [SharpCompress](https://github.com/adamhathcock/sharpcompress) | `.7z`/`.rar`/`.tar`/`.gz` import (replaces SevenZipExtractor) | MIT |
| [Microsoft.Data.Sqlite](https://learn.microsoft.com/dotnet/standard/data/sqlite/) | Library database | MIT |
| [CommunityToolkit.Mvvm](https://github.com/CommunityToolkit/dotnet) | MVVM | MIT |
| [rcheevos](https://github.com/RetroAchievements/rcheevos) | RetroAchievements client | MIT |
| [libchdr](https://github.com/rtissera/libchdr) | CHD format reader | BSD 3-Clause |
| [LibVLCSharp](https://github.com/videolan/libvlcsharp) | In-app video playback | LGPL-2.1 |

Controller illustrations from [OpenEmuControllerArt](https://github.com/kodi-game/OpenEmuControllerArt)
(BSD 3-Clause; not affiliated with OpenEmu). Bezels from [The Bezel Project](https://github.com/thebezelproject).
Full license texts in `NOTICES.txt`.

This is a community macOS port of [Emutastic](https://github.com/codingncaffeine/Emutastic) by the same
author, built from the [Linux port](https://github.com/codingncaffeine/Emutastic-For-Linux).

---

## License

[GNU General Public License v3.0](LICENSE)
