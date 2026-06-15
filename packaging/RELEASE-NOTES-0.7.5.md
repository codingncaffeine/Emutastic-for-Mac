Emutastic plays games on the Mac now — your whole library, 2D and 3D, on Apple Silicon, with the
demanding 3D consoles hardware-accelerated through the GPU.

## What's New

- **Games run.** The 0.5 preview could browse, theme, and configure but not play — this release wires
  up the full emulation path. Cores download from Preferences → Cores; video, audio, and controllers
  all work; save states, cheats, screenshots, and recording live in the in-game cog menu.
- **3D consoles, GPU-accelerated.** Nintendo 64, GameCube, 3DS, Dreamcast, and PSP render on Apple's
  GPU (Vulkan via MoltenVK) at full speed — a locked 60fps where the game allows, with internal-
  resolution upscaling far past native (N64 up to 8×, GameCube to 4K-class). PlayStation 1 is
  hardware-accelerated too. Tune **Internal Resolution**, **Texture Filter**, and **Anti-Aliasing**
  live from the in-game cog → Visuals (resolution applies on game restart).
- **Box art and metadata on the fly.** Open a game and its cover art, details, and description fetch
  automatically from ScreenScraper — nothing to bundle or pre-download.
- **Animated game-card previews.** Gameplay video snaps play natively on the game cards.
- **CD-i support.** Drop a CD-i BIOS into the System folder and Philips CD-i games boot — the app
  stages the BIOS where the core expects it.

## What's Fixed

- **The in-game cog menu responds on the first click.** It previously needed a double-click.
- **Cores load reliably.** Fixed an Apple Silicon crash that could kill a game the instant its core loaded.
- **Esc leaves fullscreen instead of quitting.** In 3D games, Escape now drops back to a window
  (including from macOS native fullscreen) and only quits when already windowed.
- **Clean metadata.** Scraped titles and descriptions no longer show raw HTML codes.

## Known limitations

- **PlayStation 2** has no Apple Silicon libretro core and isn't supported on this platform.
- 3D titles compile shaders on first encounter with new content (a brief one-time hitch), and 3DS
  takes a few seconds to compile at boot.

## Install

Apple Silicon only. Download `Emutastic-0.7.5-osx-arm64.zip` from the
[releases page](https://github.com/codingncaffeine/Emutastic-for-Mac/releases), unzip, and move
`Emutastic.app` to Applications. Builds aren't notarized yet, so on first launch **right-click the
app → Open** (or run `xattr -dr com.apple.quarantine /Applications/Emutastic.app`). Existing installs
update in-app from Preferences → About.
