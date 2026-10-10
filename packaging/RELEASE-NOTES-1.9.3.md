A last-minute RetroAchievements hardcore audit, done after the compliance application went in,
caught something the earlier audits had missed. This release closes it.

## What's Fixed

- **Cores' own cheat options are locked in hardcore.** Dolphin, PPSSPP, MAME and FBNeo can switch on
  their built-in cheat engines through a core option, a road the in-app cheat gate never saw. A
  hardcore session now serves those options their disabled value, decided before the core loads so it
  holds from the first frame; your saved choice comes back in casual. A one-time "Cheat options are
  disabled in hardcore mode" line says so.
- **Two disc swaps in a row.** A second swap before rcheevos had verified the first disc could lose
  the first check's answer. Every pending check is now kept until it is answered.

## Install

Apple Silicon only. Download `Emutastic-1.9.3-osx-arm64.zip` below, unzip, and move
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
