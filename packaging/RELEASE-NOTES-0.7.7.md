**A reliability tweak for Neo Geo + RetroAchievements.** RetroAchievements already works for Neo Geo in
Emutastic — nothing was broken, and your games and saves are untouched. This just updates how Neo Geo
cartridge files are fingerprinted so they keep matching RetroAchievements' database no matter how the
file was produced.

## What's Changed

- **More robust RetroAchievements matching for Neo Geo carts.** RetroAchievements identifies a game by
  fingerprinting its file. Neo Geo `.neo` cartridges start with a 4 KB header that can differ between
  conversion tools — same game, different header — which could keep a cart from lining up with
  RetroAchievements' database. Emutastic now skips that header and fingerprints only the cartridge's ROM
  data, the same way RetroAchievements does, so a Neo Geo game identifies and unlocks its achievements
  regardless of how its `.neo` file was made.

## Install

Apple Silicon only. Download `Emutastic-0.7.7-osx-arm64.zip` from the
[releases page](https://github.com/codingncaffeine/Emutastic-for-Mac/releases), unzip, and move
`Emutastic.app` to Applications. First launch needs **right-click → Open** (or
`xattr -dr com.apple.quarantine /Applications/Emutastic.app`). Existing installs update in-app from
Preferences → About.
