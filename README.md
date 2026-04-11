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

The control window starts minimized by default. Open it from the taskbar/dock if
you want to start a break immediately, reset the timer, or quit.

## Defaults

Edit `AppConfig` in `eye_timer.py` to change behavior:

- `rest_interval_seconds = 1 * 60`: show a rest screen after 1 active minute.
- `rest_duration_seconds = 20`: keep the full-screen overlay up for 20 seconds.
- `prepare_notice_seconds = 20`: show the small warning panel 20 seconds before the break.
- `idle_pause_after_seconds = 60`: stop counting toward a break after 60 idle seconds.
- `idle_reset_after_seconds = 10 * 60`: treat 10 idle minutes as a completed rest and restart the interval.
- `start_minimized = True`: minimize the control window after startup.

## Notes

- The overlay is skippable with the `Skip break` button, `Esc`, or `Space`.
- The pre-break notification can be hidden for the current countdown window.
- Multiple monitors are handled with Qt's screen APIs.
- Qt handles the main UI layout and high-DPI behavior, so the app no longer uses
  Tk-specific scaling and size-lock workarounds.
- Idle detection uses OS APIs where possible. On Linux it uses `xprintidle` if
  available, otherwise it tries XScreenSaver through `libXss`; Wayland sessions
  may fall back to counting the user as active because many compositors restrict
  global idle access.
