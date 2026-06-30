### What's New

**EmuTV — Living-Room Mode comes to the Mac**

The full controller-driven, big-screen couch interface lands in the macOS build — a 1:1 port of the
Windows/Linux living-room mode. Drop onto the sofa, pick up a controller, and browse and launch your
whole library on the TV without touching a keyboard or mouse.

- Controller-first fullscreen UI — a themed system carousel and game lists you drive entirely from the
  gamepad.
- ES-DE theme engine — renders EmulationStation Desktop Edition themes, with a built-in default plus
  in-app theme browsing, downloading, and importing.
- Seamless in-window game launches (a macOS-native solution) — games render inside the EmuTV window
  through a shared-surface compositor, so there's no second window, no focus handoff, and no Dock or
  menu-bar flash. It stays fullscreen from launch to quit and drops you straight back to the couch shell.
  GL and 3D/Vulkan cores (N64, GameCube, PSP, …) both work here.
- SteamGridDB artwork, an animated TV preview, and polished presentation throughout.

Getting in: hold L2 + R2 + L3 + R3 (both triggers + both sticks clicked) for ~2 seconds from the library,
or press F9. Quit a game back to the couch with the same chord.

In-game controller combos: save state L3 + R2, load latest state R3 + L2, swap disc L3 + Start — all
rebindable per system under Preferences → Controls.

## Install

Apple Silicon only. Download `Emutastic-0.7.9-osx-arm64.zip` from the
[releases page](https://github.com/codingncaffeine/Emutastic-for-Mac/releases), unzip, and move
`Emutastic.app` to Applications. First launch needs right-click → Open (or
`xattr -dr com.apple.quarantine /Applications/Emutastic.app`). Existing installs update in-app from
Preferences → About.
