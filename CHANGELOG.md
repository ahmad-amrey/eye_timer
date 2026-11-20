# Changelog

All notable changes to Eye Timer are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

To release, add a `## [X.Y.Z] - YYYY-MM-DD` section, set the same version in
`project(EyeTimer VERSION X.Y.Z)` in `CMakeLists.txt`, and push a `vX.Y.Z` tag
on `main`. The section becomes the release notes.

## [0.10.1] - 2026-11-21

Fixes for update checks and starting with Windows after installing.

### Fixed
- "Check for updates…" no longer fails with "status code 403" when GitHub
  limits requests from your network: Eye Timer then reads the latest version
  from the release page instead. A failed automatic check is retried within a
  few hours instead of the next day.
- "Start with Windows/login" now follows the app when it is installed or moved:
  on launch, a startup entry that is missing or still points at an old copy is
  written again for the installed app. Switching the setting on in Settings
  also clears a "Disabled" flag left in Task Manager.

### Added
- The Windows installer offers "Start Eye Timer when I sign in" on a first
  install.

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
