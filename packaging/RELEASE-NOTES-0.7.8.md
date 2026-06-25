### What's New

**Cloud sync — cross-machine saves & databases**

Updated how saves and databases interact across machines to avoid clobbers and make sure the newest
saves always win. Each machine now keeps its **own** library database in the cloud-sync repo, so multiple
devices/OSes can share one repo without overwriting each other's library — while **game saves stay shared
per game** across devices.

**Security**

- Updated the bundled SQLite library to a patched release.

## Install

Apple Silicon only. Download `Emutastic-0.7.8-osx-arm64.zip` from the
[releases page](https://github.com/codingncaffeine/Emutastic-for-Mac/releases), unzip, and move
`Emutastic.app` to Applications. First launch needs **right-click → Open** (or
`xattr -dr com.apple.quarantine /Applications/Emutastic.app`). Existing installs update in-app from
Preferences → About.
