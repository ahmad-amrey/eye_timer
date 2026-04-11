# Eye Timer

A small Python/PySide6 eye-rest reminder for Windows, macOS, and Linux.

## Install

```bash
pip install -r requirements.txt
```

## Run

```bash
python eye_timer.py
```

The app starts in the system tray/menu bar/status area by default. Use the tray
icon menu to show the control window, start a break immediately, reset the
timer, or quit.

## Defaults

Edit `AppConfig` in `eye_timer.py` to change behavior:

- `rest_interval_seconds = 20 * 60`: show a rest screen after 20 active minutes.
- `rest_duration_seconds = 20`: keep the full-screen overlay up for 20 seconds.
- `prepare_notice_seconds = 20`: show the small warning panel 20 seconds before the break.
- `idle_pause_after_seconds = 60`: stop counting toward a break after 60 idle seconds.
- `idle_reset_after_seconds = 10 * 60`: treat 10 idle minutes as a completed rest and restart the interval.
- `start_minimized = True`: start hidden in the tray when a tray is available.
- `use_system_tray = True`: use the OS tray/status-area integration.

## Notes

- The overlay is skippable with the `Skip break` button, `Esc`, or `Space`.
- The pre-break notification can be hidden for the current countdown window.
- Hovering over the tray/status icon shows the remaining time. The right-click
  tray menu also shows the remaining time at the top.
- The control window hides itself when it loses focus, so it behaves like a
  temporary notification panel.
- The control window has no taskbar entry while hidden in the tray. If the OS or
  desktop session has no tray/status area, the control window is shown instead.
- On macOS, Qt exposes the tray icon as a menu-bar status item. A packaged app
  may also need `LSUIElement=true` in its `Info.plist` to hide the Dock icon.
- On Windows, `icon.ico` is used for native taskbar/window icons and `icon.png`
  is kept as the source image.
- Multiple monitors are handled with Qt's screen APIs.
- Qt handles the main UI layout and high-DPI behavior, so the app no longer uses
  Tk-specific scaling and size-lock workarounds.
- Idle detection uses OS APIs where possible. On Linux it uses `xprintidle` if
  available, otherwise it tries XScreenSaver through `libXss`; Wayland sessions
  may fall back to counting the user as active because many compositors restrict
  global idle access.
