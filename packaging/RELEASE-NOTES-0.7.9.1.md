### What's New

**GameCube BIOS (IPL) support** — Emutastic now recognizes GameCube IPL dumps. Still not required,
but some games' visuals improve with it: titles that use the console's built-in font render their
text correctly (Star Fox Assault is a good example). Drop the file (or an archive of it) onto
Preferences → System Files, or leave it anywhere in your GameCube ROM folder — it's detected
automatically. NTSC dumps cover both USA and Japan; PAL covers Europe.

**BIOS auto-detection in ROM folders** — BIOS files for every system are now recognized anywhere in
that system's ROM folder (subfolders up to eight levels deep), identified by checksum and content
signature regardless of filename, and imported into the System folder automatically. Archives are
covered too: a BIOS sitting inside a `.zip`, `.7z`, `.rar`, `.tar`, or `.gz` in your ROM folder is
found and imported as well. The System folder itself gets the same treatment — a stray dump or
archive parked there under any name (even in Dolphin's own `dolphin-emu/Sys` layout) is placed into
its proper spot.

**System Files improvements** — entries that live in a subfolder (Saturn, GameCube) now display
their full relative path, and found status only reflects locations the emulator actually reads.

**In-app updater fix** — the version check previously ignored the fourth version component, so
four-part hotfix releases were invisible to it. Fixed from this version onward — which also means
v0.7.9 installs won't see an update prompt for this particular release: grab this one from the
Releases page, and in-app updates take it from there.

## Install

Apple Silicon only. Download `Emutastic-0.7.9.1-osx-arm64.zip` below, unzip, and move
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
