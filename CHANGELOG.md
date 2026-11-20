# Changelog

All notable changes to Eye Timer are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

To release, add a `## [X.Y.Z] - YYYY-MM-DD` section, set the same version in
`project(EyeTimer VERSION X.Y.Z)` in `CMakeLists.txt`, and push a `vX.Y.Z` tag
on `main`. The section becomes the release notes.

## [0.10.0] - 2026-11-20

Eye Timer is rewritten in C++ with Qt 6 and gets a new look, proper installers
and update notifications.

### Added
- Installers for every platform: a per-user Windows installer (no admin
  rights), a drag-to-install macOS disk image, and a Debian package for Linux.
  Installing, updating and uninstalling never touch your settings.
- Update notifications: once a day Eye Timer checks GitHub for a newer
  release and offers to open its download page. It can be turned off in
  Settings, and "Check for updates…" in the menu checks on demand.
- "Soft light" design for the rest overlay, countdown, pre-break notice, tray
  menu and settings.
- "Start break now" on the pre-break notice, a "Break N of today" counter, tone
  previews in Settings, and a choice of colour palettes.
- A tray icon that shows progress toward the next break.

### Changed
- Settings are organised into Rhythm, Idle, Appearance, System and Messages
  tabs.
- Settings files from earlier versions are read as they are.

### Fixed
- Notification tones no longer stutter or drop out while they play.
- Sleep and resume no longer count as active time.
