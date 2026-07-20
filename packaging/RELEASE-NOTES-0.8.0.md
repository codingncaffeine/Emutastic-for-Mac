## What's New

- **NES HD packs (Mesen)** — Emutastic now supports Mesen HD packs: high-resolution graphics and
  remastered audio for NES games. Install the Mesen core from Preferences → Cores, then right-click
  a game → **Apply ROM Hack / HD Pack…** and pick the pack archive exactly as you downloaded it —
  nested "unzip me first" bundles included. Installed packs are kept in a per-game mod library and
  selected from the game card's **…** menu (HD Mod: None or any installed mod, with renaming).
  Packs made for Mesen 2 (format v107+) can't run on the libretro core and are clearly labeled
  instead of failing silently.
- **SNES in HD and widescreen (bsnes-hd beta)** — new core available in Preferences → Cores.
  HD Mode 7 (up to 10×), supersampling, perspective correction, and widescreen up to 21:9 — all
  adjustable live from the in-game cog's Visuals panel. HD Mode 7 is on at 2× out of the box.
- **ROM hacks, upgraded** — right-click a game → **Apply ROM Hack…** and pick the patch file or its
  downloaded zip directly (no extracting needed). Hacks now run on every core, including ones that
  load games by file path — a patched copy is staged automatically and your original ROM is never
  modified. RetroAchievements identifies hacks by their patched content, so recognized hacks earn
  on their own sets and hardcore is never credited from modified games (mods that embed ROM patches
  drop to softcore for the session).
- **Your names stick** — games you've renamed, and ROM-hack titles, are no longer renamed back by
  metadata refreshes.
- **Early texture-pack support** — right-click a GameCube, Nintendo 64, or PSP game →
  **Install Texture Pack…** to load high-res texture packs for those systems.

## What's Fixed

- **PS1: analog support is back, and digital-only titles just work** — the DualShock is the default
  PS1 controller again, so analog-capable games get working sticks. A number of early PS1 releases
  only accept the original digital pad: with a DualShock connected they either refuse to start
  ("please insert a standard PlayStation controller") or boot with controls unresponsive. Emutastic
  now carries a compatibility table covering every digital-only PS1 release and plugs in the right
  pad type automatically — at launch and after loading a save state. This replaces the previous
  behavior of forcing the digital pad for all PS1 games.
- Resolved a crash when launching games on certain newly installed cores.

## Install

Apple Silicon only. Download `Emutastic-0.8.0-osx-arm64.zip` below, unzip, and move
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
