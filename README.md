![Emutastic](src/Emutastic/Assets/banners%20and%20icons/emutastic-banner-scaled.png)

# Emutastic for Mac

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

A native **macOS** port of [Emutastic](https://github.com/codingncaffeine/Emutastic) — a multi-system
emulator frontend inspired by [OpenEmu](https://openemu.org/), built on **.NET 10 + Avalonia** (the
original is Windows/WPF/.NET 8). Games are organized by console in a clean library interface. Emulation
is handled by [libretro](https://www.libretro.com/) cores loaded at runtime — no cores are bundled.

The goal is a **1:1 clone**: aesthetically and functionally identical to the Windows and Linux apps, with
only the platform plumbing swapped underneath — WPF → **Avalonia (Avalonia.Native)**, Direct3D/Vulkan →
**Metal/OpenGL**, WASAPI → **SDL3 / CoreAudio**, XInput → **SDL3 / GameController**, Win32 core loading →
**`dlopen`**.

> **🚧 Status — early port, in active development.** The native library/UI runs on Apple Silicon today
> (window, theming, every Preferences tab, library navigation). Still being brought up: the game-host
> present path (so games actually render), SDL3 audio + controllers, video recording, snap-video
> previews, libretro core download wiring (`apple/osx/arm64`), and `.app` packaging + signing. The
> feature list below describes the **target** — parity with the Windows and Linux builds.

> **Legal notice:** This project is a frontend only. It does not include, distribute, or facilitate the
> acquisition of any copyrighted software, ROM images, BIOS files, or other proprietary system files.
> You are solely responsible for ensuring you have the legal right to use any software you load.

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

Themes (Dark / Light / OLED / Midnight + a visual editor) · automatic artwork & metadata (OpenVGDB +
libretro thumbnails, optional ScreenScraper) · **SDL3** controller support with analog-stick-as-D-pad ·
**RetroAchievements** (unlock toasts with a full appearance editor, hardcore mode, Achievements tab
with trophy case + activity heatmap, friends with unlock feeds + leaderboard toasts, CHD support) ·
screenshots & **gameplay recording** (x264) · **GitHub cloud sync** (below) · disc swapping ·
per-game notes · game manuals (auto-download) · cheats (+ cheat database import) · core options ·
save states with screenshots · play-time tracking · **ROM hacks** (IPS/BPS/UPS soft-patching —
original ROM untouched) · **video shaders** · **bezels & Vectrex overlays** · **turbo buttons** ·
**native window controls** (Preferences → Theme — real macOS minimize / maximize / close, or the custom
frameless look).

(See the upstream [Emutastic wiki](https://github.com/codingncaffeine/Emutastic/wiki) for per-feature
detail — behavior is intended to match. Diagnostic logs live in the `Logs/` folder of the data
directory, which on macOS is `~/Library/Application Support/Emutastic/Logs/`.)

---

## Cloud Sync

Sign in with your GitHub account (**Preferences → Backups** — device flow, no password stored) and
your battery saves + game library sync through a private repository on your account.

The same repository serves the Windows, Linux, and macOS apps: save on one machine, pick up on another
(battery saves are keyed by ROM hash, so each install must import the same ROM files). Optional
AES-256-GCM encryption with a passphrase you choose. Sync activity is logged to `Logs/cloudsync.log`.

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
Inspired by [OpenEmu](https://openemu.org/) for macOS. Full license texts in `NOTICES.txt`.

This is a community macOS port of [Emutastic](https://github.com/codingncaffeine/Emutastic) by the same
author, built from the [Linux port](https://github.com/codingncaffeine/Emutastic-For-Linux).

---

## License

[GNU General Public License v3.0](LICENSE)
