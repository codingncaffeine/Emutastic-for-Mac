## What's New

- **Version numbers now match the Windows app.** Emutastic for Mac jumps from 0.8.0 to 1.8.10 because
  RetroAchievements sees the Windows, Linux and Mac builds as one emulator named Emutastic, and its rules
  want one numeric, always-increasing version line under that name. Nothing else about updating changes —
  the in-app updater treats 1.8.10 as newer and offers it as usual.
- **Cloud sync backs up each Mac to its own repository.** Every computer now keeps its own
  `emutastic-saves-<name>` repository (the Mac's Local Hostname from System Settings → Sharing), so a new
  or reinstalled machine can never overwrite progress made on another. Signing in again on a reinstalled
  Mac with the same name restores its backup. If you already synced to the shared `emutastic-saves`
  repository, it stays on GitHub untouched.
- **Sign-in kept in your Keychain.** The GitHub sign-in and the encryption passphrase now live in your
  login Keychain instead of `config.json`, and move there automatically on first launch.
- **Sync progress.** The status bar shows which step a sync is on and how many files it has moved, and
  Preferences shows the same beside Sync Now. `Logs/cloudsync.log` records each sync in detail.
- **Customise the sidebar.** Preferences → Library lets you choose which consoles the sidebar lists and
  the order they appear in. Hide a console or a whole manufacturer, move consoles and groups, hide
  consoles with no games, and restore the original layout at any time. Hiding a console never removes
  anything — its games stay in your library and under All Games.
- **Choose which controller is which player.** Preferences → Controls → Input Device now binds a
  controller to that player, and the game honours it — no more "player 1 is whichever pad connected
  first". Unbound players still take pads in connection order.
- **Controllers without an SDL mapping now work.** Generic USB SNES/NES adapters, arcade sticks and
  other pads SDL has no mapping for are now listed and playable, read with a standard button layout you
  can rebind in Preferences.
- **Players stay put when a controller disconnects.** Losing one player's pad mid-game no longer shifts
  every later player down a slot.
- **"Casual" instead of "Softcore".** RetroAchievements renamed non-hardcore play to Casual on June 30,
  2026. The Achievements tab, friend cards and the RetroAchievements preferences use the same word.
- **EmuTV theme rendering.** Themes that use per-system include files, game selector panels, tinted
  images or rooted include paths now render the way their authors intended.
- **Drop a BIOS folder.** Dropping a folder onto System Files now searches it all the way down and
  imports every recognized dump; the originals are left where they are.

## What's Fixed

- **Achievements on PlayStation and other systems.** For cores that describe their memory layout to the
  frontend (PlayStation, among others), Emutastic misread the size of every memory region, so
  RetroAchievements switched off nearly every achievement at load — Rayman had 0 of 59 active. They
  load and unlock normally now (57 of 59 for Rayman; the other two are unsupported by RetroAchievements
  itself).
- **EmuTV games showing upside down.** Games launched from EmuTV could appear upside down, on-screen
  messages included. They display the right way up now.
- **Reset now resets your achievements too.** Resetting a game from the in-game cog only reset the
  console; hit counts and leaderboard attempts from before the reset carried over. RetroAchievements
  requires the runtime to reset with the game, and it now does.
- **Disc swaps are verified by RetroAchievements.** Swapping discs mid-game (L3 + Start) now hands the
  new disc to RetroAchievements to confirm it belongs to the game you loaded. If it isn't recognised, a
  hardcore session drops to casual with a message on screen, exactly as RetroArch behaves; in hardcore, a
  swap that can't be verified is refused.
- **"Every 15 minutes during play" did nothing.** The setting was never read, so it behaved like "On
  game close". It now uploads on that interval while a game is running.
- **"Manual only" still synced on its own.** A sync still ran at startup and a save was still pulled
  before a game launched. Nothing moves now until you press Sync Now.
- **Large saves not restored.** Files over 1 MB were skipped without an error when downloading. They
  download correctly now.
- **Texture packs and BIOS files in the backup.** HD texture packs, BIOS files and console system files
  stay out of the backup.
- **Misleading ScreenScraper login error.** A refused developer registration reported itself as
  "Incorrect ScreenScraper username or password". Each failure is now named for what actually went
  wrong.
- **config.json permissions.** The config file is now readable only by you, and a file from an earlier
  version is tightened on launch.

## Improvements

- **RetroAchievements runtime updated to rcheevos 12.5.0.** Upstream fixes for achievement-logic edge
  cases and disc-image hashing, and the same Neo Geo `.neo` content hashing the Windows app uses.
- RetroAchievements now sees this app as running on macOS (it was reported as Linux).
- Built on Avalonia 12.1.2.

## Install

Apple Silicon only. Download `Emutastic-1.8.10-osx-arm64.zip` below, unzip, and move
`Emutastic.app` to Applications. Existing installs update in-app from Preferences → About.

**First launch:** Emutastic is self-signed (free, non-profit — no Apple Developer subscription), so
macOS shows a one-time "Apple could not verify…" prompt. It is expected, and you will never see a
"damaged" warning — the app is properly signed, just not notarized:

- **macOS 15 Sequoia / 26 Tahoe:** double-click the app once and click **Done** on the warning, then
  open **System Settings → Privacy & Security**, scroll down, and click **Open Anyway** next to the
  Emutastic message. Confirm once and it opens normally forever after.
- **macOS 14 Sonoma and earlier:** right-click (Control-click) **Emutastic.app → Open → Open**.
- **Terminal shortcut (all versions, skips every dialog):**
  `xattr -dr com.apple.quarantine /Applications/Emutastic.app`
