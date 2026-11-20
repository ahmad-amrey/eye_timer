#pragma once

#include <QString>

namespace autostart {

// Registers or removes the app as a login item:
//   Windows: a shortcut in the user's Startup folder
//   macOS:   ~/Library/LaunchAgents/com.eyetimer.app.plist
//   Linux:   $XDG_CONFIG_HOME/autostart/eye-timer.desktop
// `executable` defaults to this program.
bool setEnabled(bool enabled, QString *error, const QString &executable = {});

// True when the login item exists and launches this executable.
bool isRegistered();

// Makes the login item match the setting after the app was installed or moved:
// when `wanted` and the item is missing or points at another copy, it is
// written again for this executable. Never removes anything.
void sync(bool wanted);

} // namespace autostart
