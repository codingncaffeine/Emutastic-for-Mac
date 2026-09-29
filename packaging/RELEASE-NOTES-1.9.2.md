Keyboard binds now work in games, and the Game Boy Color gets its own controller picture.

## What's Fixed

- **Keyboard binds reach the game.** Keys bound in Preferences → Controls did nothing in a game: the game
  window read the keyboard through a fixed key table and never looked at your binds. It now uses them,
  laid over the built-in keys (arrows, Z X A S, Enter, Right Shift, Q W): a bound button's built-in key
  retires, and a key you take for another button leaves its old button. Keys match the label on your
  keyboard layout and still fire with Shift held, and with no controller attached, keys bound to the
  stick rows drive the analog sticks. Each Controls row now shows the key that actually plays instead of
  a dash.
- **Dreamcast stick rows can be bound.** Binding any of the Dreamcast "Left Analog" rows was silently
  dropped, on keyboard and controller alike. They now drive the left stick.

## Improvements

- **The Game Boy Color has its own controller picture** in Preferences → Controls, instead of reusing
  the Game Boy's.

## Install

Apple Silicon only. Download `Emutastic-1.9.2-osx-arm64.zip` below, unzip, and move
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
