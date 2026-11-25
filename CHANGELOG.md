# Changelog

All notable changes to Eye Timer are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

Add entries under `## [Unreleased]` as changes land. To release, include in
the pull request that ships the last change: that section renamed to
`## [X.Y.Z] - YYYY-MM-DD` and the same version in `project(EyeTimer VERSION
X.Y.Z)` in `CMakeLists.txt`. Merging it publishes the release: CI tags the
merge commit and releases the pull request's build without building again, and
the section becomes the release notes.

## [Unreleased]

### Fixed
- `Esc` skips the break again when another app had the keyboard as the break
  began. The rest overlay now takes the keyboard on Windows, macOS and Linux,
  and takes it back if another window grabs it, so keys you type during a
  break no longer reach the app underneath.
- Alt+F4 or the window manager's close no longer closes the rest overlay on
  one screen while the break carries on.

### Changed
- During a break, keys other than `Esc` shake the `Skip break` button to show
  that they are held back and how to leave. `Space` no longer skips the break,
  so typing a sentence when the break starts cannot end it by accident.

## [0.11.0] - 2026-11-21

Snooze, and a steady "Break soon" countdown.

### Added
- "Snooze 3 min" on the "Break soon" panel moves the break a few minutes
  later. The length can be changed under Settings → Rhythm → Snooze.

### Fixed
- The "Break soon" panel no longer flickers when you are away from the
  computer during its countdown. Once the countdown has started it runs to the
  break whether or not you are there.

## [0.10.3] - 2026-11-21

Updates download the right installer directly.

### Changed
- "Download" in the update notice now downloads this computer's installer
  straight away (the setup program on Windows, the disk image on macOS, the
  `.deb` package on Debian and Ubuntu) instead of opening the release page
  with every file on it. On other Linux systems it still opens the release
  page.

## [0.10.2] - 2026-11-21

macOS window fixes.

### Fixed
- macOS: the countdown pill, the "Break soon" panel and the rest overlay no
  longer disappear as soon as you click into another app. They also follow
  you to every desktop (Space) and show over full-screen apps.

### Security
- The update notice's Download button only opens this project's release pages
  on GitHub, whatever the update data says.

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
