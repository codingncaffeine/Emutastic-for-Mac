The first macOS build of Emutastic — an early preview. The native library and interface run on
Apple Silicon; emulation is still being wired up, so you can browse, theme, and configure, but
games don't launch yet.

## What's New

- **Emutastic comes to macOS (Apple Silicon).** A native port of the .NET 10 + Avalonia Linux
  build, running on Avalonia.Native — the full library, Preferences, themes, and artwork/metadata
  interface, pixel-identical to the Windows and Linux apps.
- **Native window controls.** Preferences → Theme → "Use native window controls" swaps the custom
  frameless window for the real macOS title bar with traffic-light minimize / maximize / close.

## Known limitations (early preview)

- **Games don't run yet.** The emulator present path, libretro core downloads, audio, and
  controllers arrive in upcoming releases.
- **PlayStation 2** has no Apple Silicon libretro core and won't be supported on this platform.

## Install

Apple Silicon only. Download `Emutastic-0.5.0-osx-arm64.zip`, unzip, and move `Emutastic.app` to
Applications. It isn't notarized yet, so on first launch **right-click the app → Open** (or run
`xattr -dr com.apple.quarantine /path/to/Emutastic.app`).
