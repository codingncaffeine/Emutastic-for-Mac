Your saves now follow you to every machine, gameplay recording is built into the Mac with nothing to
download, and controllers behave the way they should.

## What's New

- **Cloud sync backs up everything now.** Sync previously covered only battery saves (`.srm`); this
  release adds the memory cards and save data the cores manage themselves — GameCube & Dreamcast memory
  cards, PSP / 3DS / DS save data, PlayStation & Saturn cards, arcade NVRAM. Saves are organized per
  console, and a fresh machine pulls your saves down automatically — even for games you haven't imported
  there yet. (Your existing saves are tidied into per-console folders once, on first launch; save states
  aren't synced yet.)
- **Recording is native — nothing to download.** Gameplay capture now uses Apple's built-in VideoToolbox
  (hardware-accelerated H.264 / HEVC / ProRes) instead of needing an ffmpeg download. Choose the encoder,
  quality, scale and audio bitrate in Preferences → Media; H.264/HEVC save as `.mp4`, ProRes as `.mov`.
  Record from the in-game cog → Record (or F9).

## What's Fixed

- **Controllers connected after a game starts now work.** A controller switched on mid-launch — or paired
  after the game opened — is picked up immediately. No more "controls are dead until I restart the game."
- **Fixed a launch hang.** The app could beachball on startup while scanning for controllers; resolved.
- **View Recordings, game manuals, and external links open.** These quietly did nothing on macOS before.
- **The in-app updater works.** Preferences → About now checks the Mac releases and updates in place.
- **macOS remembers folder permissions.** The app is signed with a stable certificate now, so you grant
  access to your ROM folders **once** instead of being re-asked on every launch — keep your ROMs anywhere.

## Install

Apple Silicon only. Download `Emutastic-0.7.6-osx-arm64.zip` from the
[releases page](https://github.com/codingncaffeine/Emutastic-for-Mac/releases), unzip, and move
`Emutastic.app` to Applications. Builds aren't notarized yet, so on first launch **right-click the
app → Open** (or run `xattr -dr com.apple.quarantine /Applications/Emutastic.app`). Existing installs
update in-app from Preferences → About.
