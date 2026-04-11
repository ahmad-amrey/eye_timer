# Eye Timer

A small Python/Tkinter eye-rest reminder for Windows, macOS, and Linux.

## Run

```bash
python eye_timer.py
```

The control window starts minimized by default. Open it from the taskbar/dock if
you want to start a break immediately, reset the timer, or quit.

## Defaults

Edit `AppConfig` in `eye_timer.py` to change behavior:

- `rest_interval_seconds = 20 * 60`: show a rest screen after 20 active minutes.
- `rest_duration_seconds = 20`: keep the full-screen overlay up for 20 seconds.
- `prepare_notice_seconds = 25`: show the small warning panel 25 seconds before the break.
- `idle_pause_after_seconds = 60`: stop counting toward a break after 60 idle seconds.
- `idle_reset_after_seconds = 5 * 60`: treat 5 idle minutes as a completed rest and restart the interval.
- `start_minimized = True`: minimize the control window after startup.

## Notes

- The overlay is skippable with the `Skip break` button, `Esc`, or `Space`.
- Multiple monitors are handled without required dependencies on Windows and with
  best-effort support on macOS/Linux. If the optional `screeninfo` package is
  installed, the app will use it for monitor enumeration.
- Idle detection uses OS APIs where possible. On Linux it uses `xprintidle` if
  available, otherwise it tries XScreenSaver through `libXss`; Wayland sessions
  may fall back to counting the user as active because many compositors restrict
  global idle access.
