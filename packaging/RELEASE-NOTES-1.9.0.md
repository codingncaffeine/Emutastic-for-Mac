Atari 2600 keeps working with current Stella cores, now that cores get the libretro virtual file system.

## What's Fixed

- **Atari 2600 games will keep starting after Stella updates.** Recent Stella cores refuse to load a game
  unless the frontend provides the libretro virtual file system; every Atari 2600 title then fails with
  "Unrecognized ROM file type". The Windows and Linux apps were already hit by this. The Mac core
  download hasn't picked up that Stella yet, so Mac games still start today, but they would have broken
  with the next core update. Emutastic now provides that file system to every core that asks for it, the
  same way RetroArch does, and was tested here against a current Stella build. Cores that also use it for
  their own files (FBNeo, PPSSPP, Genesis Plus GX, Beetle, Nestopia and others) keep working as before.

## Install

Apple Silicon only. Download `Emutastic-1.9.0-osx-arm64.zip` below, unzip, and move
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
