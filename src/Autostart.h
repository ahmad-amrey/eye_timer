#pragma once

#include <QString>

namespace autostart {

// Registers or removes the app as a login item:
//   Windows: a shortcut in the user's Startup folder
//   macOS:   ~/Library/LaunchAgents/com.eyetimer.app.plist
//   Linux:   $XDG_CONFIG_HOME/autostart/eye-timer.desktop
bool setEnabled(bool enabled, QString *error);

} // namespace autostart
