<!--
  Release-notes template for Emutastic for Mac.
  Copy this to packaging/RELEASE-NOTES-<version>.md for each release (e.g. RELEASE-NOTES-0.1.0.md)
  and fill it in. Keep the voice user-facing and concrete — what changed, not how it was built.
  Every section is optional; delete any that don't apply to this release.
-->

One- or two-sentence summary of the headline change in this release.

## What's New

- **Headline feature.** What it does and how to reach it (e.g. "Grab the core from Preferences → Cores").

## What's Fixed

- **Plain-language symptom.** What was going wrong, and that it now works.

## Improvements

- **Smaller polish.** A refinement to existing behavior.

## Install

Download the macOS build (`Emutastic-<version>-osx-arm64.zip` / `.dmg`) from the
[releases page](https://github.com/codingncaffeine/Emutastic-for-Mac/releases). Builds are not yet
notarized, so on first launch **right-click the app → Open** (or run
`xattr -dr com.apple.quarantine /Applications/Emutastic.app`). Existing installs update in-app from
Preferences → About.
