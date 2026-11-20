#pragma once

#include <QString>

namespace platform {

inline constexpr auto kAppUserModelId = "EyeTimer.App";

// Finds a bundled data file (icons, tones, fonts): next to the executable,
// in a macOS bundle's Resources folder, in an installed share folder, or in
// the source tree's asset folder for development builds.
QString resourcePath(const QString &fileName);

// Per-user folder that holds settings.json.
QString appDataDirectory();

// Groups taskbar entries and notifications under one id on Windows.
void setWindowsAppUserModelId();

// "Windows" on Windows, "login" elsewhere; used in the startup setting label.
QString startupMomentName();

// Linux window managers often ignore WindowStaysOnTopHint for frameless
// windows. Best effort through wmctrl, xdotool or GNOME Shell.
void reinforceAlwaysOnTop(const QString &windowTitle);

// Linux/X11: remember the focused window and give focus back after showing a
// panel that must not steal it. No-ops elsewhere.
QString activeWindowId();
void restoreActiveWindow(const QString &windowId);

} // namespace platform
