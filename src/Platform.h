#pragma once

#include <QString>

class QWidget;

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

// For frameless always-on-top windows. macOS: stays visible when the app
// loses focus, follows the user to every desktop (Space) and shows over
// full-screen apps. Call after the window flags and attributes are set, as it
// creates the native window. No-op elsewhere.
void keepFloatingEverywhere(QWidget *widget);

// Makes a full-screen window receive the keyboard while another
// application is in the foreground, so keystrokes stop reaching that app.
// activateWindow() alone is not enough: Windows lets only the foreground
// process change the foreground window, a background macOS app's window
// cannot become key, and X11 window managers may refuse focus. Windows joins
// the foreground thread's input queue first, macOS activates the app, and
// X11 also grabs the keyboard. Safe to call repeatedly; a call that cannot
// succeed yet (window not mapped) is retried by calling again.
void takeKeyboardFocus(QWidget *widget);

// Ends the X11 keyboard grab taken by takeKeyboardFocus(). No-op elsewhere.
void releaseKeyboardFocus(QWidget *widget);

// Linux window managers often ignore WindowStaysOnTopHint for frameless
// windows. Best effort through wmctrl, xdotool or GNOME Shell.
void reinforceAlwaysOnTop(const QString &windowTitle);

// Linux/X11: remember the focused window and give focus back after showing a
// panel that must not steal it. No-ops elsewhere.
QString activeWindowId();
void restoreActiveWindow(const QString &windowId);

} // namespace platform
