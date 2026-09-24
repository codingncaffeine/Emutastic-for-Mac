RetroAchievements hardcore-compliance release: the changes RetroAchievements' own checklist asks for ahead of Emutastic's approval application, plus the wording RetroAchievements adopted in June.

## What's New

- **Version numbers now match the Windows app.** Emutastic for Mac jumps from 0.8.0 to 1.8.10 because RetroAchievements sees the Windows, Linux and Mac builds as one emulator named Emutastic, and its rules want one numeric, always-increasing version line under that name. Nothing else about updating changes — the in-app updater treats 1.8.10 as newer and offers it as usual.
- **"Casual" instead of "Softcore".** RetroAchievements renamed non-hardcore play to Casual on June 30, 2026. The Achievements tab, friend cards and the RetroAchievements preferences now use the same word.

## What's Fixed

- **Reset now resets your achievements too.** Resetting a game from the in-game cog only reset the console; the RetroAchievements runtime kept running, so hit counts and leaderboard attempts from before the reset carried over. RetroAchievements requires the runtime to reset with the game, and it now does.
- **Disc swaps are verified by RetroAchievements.** Swapping discs mid-game (L3 + Start) now hands the newly inserted disc to RetroAchievements so it can confirm the disc belongs to the game you loaded. If it isn't recognised, a hardcore session drops to casual with a message on screen, exactly as RetroArch behaves. Multi-disc games launched from their `.m3u` playlist work as before; in a hardcore session a swap that can't be verified is refused instead.

## Improvements

- **RetroAchievements runtime updated to rcheevos 12.5.0.** Brings upstream fixes for achievement-logic edge cases and disc-image hashing, and the same Neo Geo `.neo` content hashing the Windows app has used since June.

## Install

Download the macOS build (`Emutastic-1.8.10-osx-arm64.zip` / `.dmg`) from the
[releases page](https://github.com/codingncaffeine/Emutastic-for-Mac/releases). Builds are not yet
notarized, so on first launch **right-click the app → Open** (or run
`xattr -dr com.apple.quarantine /Applications/Emutastic.app`). Existing installs update in-app from
Preferences → About.
