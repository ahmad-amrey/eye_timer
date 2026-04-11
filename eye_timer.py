"""
Cross-platform eye resting reminder.

Run:
    python eye_timer.py

Configuration lives in the AppConfig dataclass below. The defaults implement a
20-20 style reminder: every 20 active minutes, show a 20 second full-screen
overlay, preceded by a small warning panel.
"""

from __future__ import annotations

import ctypes
import platform
import re
import shutil
import subprocess
import time
import tkinter as tk
from dataclasses import dataclass
from typing import Callable, Iterable


BG = "#101010"
PANEL_BG = "#181818"
TEXT = "#f5f5f5"
MUTED_TEXT = "#b8c0bb"
ACCENT = "#6ee7a8"
ACCENT_DARK = "#198754"
BUTTON_BG = "#f5f5f5"
BUTTON_FG = "#101010"
BUTTON_SECONDARY_BG = "#2a2a2a"
BUTTON_SECONDARY_FG = "#f5f5f5"


@dataclass(frozen=True)
class AppConfig:
    rest_interval_seconds: int = 1 * 60
    rest_duration_seconds: int = 20
    prepare_notice_seconds: int = 20
    idle_pause_after_seconds: int = 60
    idle_reset_after_seconds: int = 10 * 60
    tick_ms: int = 1000
    overlay_alpha: float = 0.86
    prepare_panel_alpha: float = 0.96
    start_minimized: bool = True
    healthy_suggestions: tuple[str, ...] = (
        "Look at something at least 6 meters away.",
        "Close your eyes and relax your shoulders.",
        "Let your eyes wander around the room.",
        "Soften your focus and breathe evenly.",
        "Check your posture while your eyes rest.",
    )


@dataclass(frozen=True)
class Monitor:
    x: int
    y: int
    width: int
    height: int
    name: str = "display"


def clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def monitor_scale(monitor: Monitor) -> float:
    # Base against a 1080p display, but keep extreme monitor sizes usable.
    return clamp(min(monitor.width / 1920, monitor.height / 1080), 0.85, 1.75)


def scaled(value: int, scale: float) -> int:
    return max(1, int(round(value * scale)))


def enable_dpi_awareness() -> None:
    if platform.system().lower() != "windows":
        return

    try:
        ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
        return
    except Exception:
        pass

    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except Exception:
        try:
            ctypes.windll.user32.SetProcessDPIAware()
        except Exception:
            pass


class IdleDetector:
    def __init__(self) -> None:
        system = platform.system().lower()
        if system == "windows":
            self._reader = self._windows_idle_seconds
        elif system == "darwin":
            self._reader = self._mac_idle_seconds
        elif system == "linux":
            self._reader = self._linux_idle_seconds
        else:
            self._reader = self._unsupported_idle_seconds

    def seconds(self) -> float:
        try:
            idle_seconds = self._reader()
        except Exception:
            return 0.0
        if idle_seconds is None or idle_seconds < 0:
            return 0.0
        return idle_seconds

    @staticmethod
    def _windows_idle_seconds() -> float:
        class LastInputInfo(ctypes.Structure):
            _fields_ = [("cbSize", ctypes.c_uint), ("dwTime", ctypes.c_uint)]

        info = LastInputInfo()
        info.cbSize = ctypes.sizeof(info)
        if not ctypes.windll.user32.GetLastInputInfo(ctypes.byref(info)):
            return 0.0
        ctypes.windll.kernel32.GetTickCount.restype = ctypes.c_uint
        elapsed_ms = ctypes.c_uint(ctypes.windll.kernel32.GetTickCount() - info.dwTime).value
        return max(0.0, elapsed_ms / 1000.0)

    @staticmethod
    def _mac_idle_seconds() -> float:
        command = ["ioreg", "-c", "IOHIDSystem"]
        output = subprocess.check_output(command, text=True, stderr=subprocess.DEVNULL)
        match = re.search(r'"HIDIdleTime"\s*=\s*(\d+)', output)
        if not match:
            return 0.0
        return int(match.group(1)) / 1_000_000_000

    @staticmethod
    def _linux_idle_seconds() -> float:
        if shutil.which("xprintidle"):
            try:
                output = subprocess.check_output(["xprintidle"], text=True, stderr=subprocess.DEVNULL)
                return int(output.strip()) / 1000
            except Exception:
                pass

        return IdleDetector._linux_xss_idle_seconds()

    @staticmethod
    def _linux_xss_idle_seconds() -> float:
        lib_x11 = ctypes.cdll.LoadLibrary("libX11.so.6")
        lib_xss = ctypes.cdll.LoadLibrary("libXss.so.1")

        class XScreenSaverInfo(ctypes.Structure):
            _fields_ = [
                ("window", ctypes.c_ulong),
                ("state", ctypes.c_int),
                ("kind", ctypes.c_int),
                ("since", ctypes.c_ulong),
                ("idle", ctypes.c_ulong),
                ("eventMask", ctypes.c_ulong),
            ]

        lib_x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        lib_x11.XOpenDisplay.restype = ctypes.c_void_p
        lib_x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        lib_x11.XDefaultRootWindow.restype = ctypes.c_ulong
        lib_x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
        lib_x11.XCloseDisplay.restype = ctypes.c_int
        lib_xss.XScreenSaverQueryInfo.argtypes = [
            ctypes.c_void_p,
            ctypes.c_ulong,
            ctypes.POINTER(XScreenSaverInfo),
        ]
        lib_xss.XScreenSaverQueryInfo.restype = ctypes.c_int

        display = lib_x11.XOpenDisplay(None)
        if not display:
            return 0.0
        try:
            root = lib_x11.XDefaultRootWindow(display)
            info = XScreenSaverInfo()
            if not lib_xss.XScreenSaverQueryInfo(display, root, ctypes.byref(info)):
                return 0.0
            return info.idle / 1000
        finally:
            lib_x11.XCloseDisplay(display)

    @staticmethod
    def _unsupported_idle_seconds() -> float:
        return 0.0


def get_monitors(root: tk.Tk) -> list[Monitor]:
    monitors = _optional_screeninfo_monitors()
    if monitors:
        return monitors

    system = platform.system().lower()
    if system == "windows":
        monitors = _windows_monitors()
    elif system == "darwin":
        monitors = _mac_monitors()
    elif system == "linux":
        monitors = _linux_monitors()
    else:
        monitors = []

    if monitors:
        return monitors

    root.update_idletasks()
    return [Monitor(0, 0, root.winfo_screenwidth(), root.winfo_screenheight(), "primary")]


def _optional_screeninfo_monitors() -> list[Monitor]:
    try:
        from screeninfo import get_monitors as screeninfo_get_monitors  # type: ignore
    except Exception:
        return []

    monitors: list[Monitor] = []
    for index, monitor in enumerate(screeninfo_get_monitors(), start=1):
        monitors.append(
            Monitor(
                int(monitor.x),
                int(monitor.y),
                int(monitor.width),
                int(monitor.height),
                getattr(monitor, "name", f"display-{index}") or f"display-{index}",
            )
        )
    return monitors


def _windows_monitors() -> list[Monitor]:
    monitors: list[Monitor] = []

    from ctypes import wintypes

    monitor_enum_proc = ctypes.WINFUNCTYPE(
        wintypes.BOOL,
        wintypes.HMONITOR,
        wintypes.HDC,
        ctypes.POINTER(wintypes.RECT),
        wintypes.LPARAM,
    )

    def callback(_monitor: int, _dc: int, rect: ctypes.POINTER(wintypes.RECT), _data: int) -> int:
        bounds = rect.contents
        monitors.append(
            Monitor(
                bounds.left,
                bounds.top,
                bounds.right - bounds.left,
                bounds.bottom - bounds.top,
                f"display-{len(monitors) + 1}",
            )
        )
        return 1

    ctypes.windll.user32.EnumDisplayMonitors(0, 0, monitor_enum_proc(callback), 0)
    return monitors


def _mac_monitors() -> list[Monitor]:
    try:
        import Quartz  # type: ignore
        result = Quartz.CGGetActiveDisplayList(16, None, None)
        if not isinstance(result, tuple) or len(result) < 2 or result[0] != 0:
            return []
        displays = result[1] or []

        monitors: list[Monitor] = []
        for index, display_id in enumerate(displays):
            bounds = Quartz.CGDisplayBounds(display_id)
            monitors.append(
                Monitor(
                    int(bounds.origin.x),
                    int(bounds.origin.y),
                    int(bounds.size.width),
                    int(bounds.size.height),
                    f"display-{index + 1}",
                )
            )
        return monitors
    except Exception:
        return []


def _linux_monitors() -> list[Monitor]:
    if not shutil.which("xrandr"):
        return []
    try:
        output = subprocess.check_output(["xrandr", "--query"], text=True, stderr=subprocess.DEVNULL)
    except Exception:
        return []

    monitors: list[Monitor] = []
    pattern = re.compile(r"\b(\d+)x(\d+)\+(-?\d+)\+(-?\d+)\b")
    for line in output.splitlines():
        if " connected" not in line:
            continue
        match = pattern.search(line)
        if not match:
            continue
        width, height, x, y = [int(group) for group in match.groups()]
        name = line.split(maxsplit=1)[0]
        monitors.append(Monitor(x, y, width, height, name))
    return monitors


class EyeTimerApp:
    def __init__(self, root: tk.Tk, config: AppConfig) -> None:
        self.root = root
        self.config = config
        self.idle_detector = IdleDetector()
        self.monitors = get_monitors(root)
        self.ui_scale = self._root_scale()
        self.active_seconds = 0.0
        self.last_tick = time.monotonic()
        self.break_started_at: float | None = None
        self.suggestion_index = 0
        self.overlay_windows: list[tk.Toplevel] = []
        self.countdown_labels: list[tk.Label] = []
        self.suggestion_labels: list[tk.Label] = []
        self.prepare_window: tk.Toplevel | None = None
        self.control_window_size: tuple[int, int] | None = None
        self.control_window_resize_job: str | None = None
        self.prepare_panel_dismissed = False
        self.prepare_countdown_var = tk.StringVar(value="")
        self.status_var = tk.StringVar(value="Starting.")
        self.next_break_var = tk.StringVar(value=self._format_next_break(self.config.rest_interval_seconds))
        self._build_control_window()
        self._tick()

    def _build_control_window(self) -> None:
        scale = self.ui_scale
        self.root.title("Eye Timer")
        self.root.configure(background=BG)
        self.root.geometry(f"{scaled(560, scale)}x{scaled(260, scale)}")
        self.root.minsize(scaled(520, scale), scaled(240, scale))
        self.root.resizable(False, False)
        self.root.protocol("WM_DELETE_WINDOW", self.stop)

        container = tk.Frame(self.root, padx=scaled(24, scale), pady=scaled(22, scale), background=BG)
        container.pack(fill="both", expand=True)

        tk.Label(
            container,
            text="Eye Timer",
            font=("Segoe UI", 20, "bold"),
            foreground=TEXT,
            background=BG,
        ).pack(anchor="w")

        tk.Label(
            container,
            textvariable=self.status_var,
            font=("Segoe UI", 11),
            foreground=MUTED_TEXT,
            background=BG,
        ).pack(anchor="w", pady=(scaled(16, scale), 0))

        tk.Label(
            container,
            textvariable=self.next_break_var,
            font=("Segoe UI", 16, "bold"),
            foreground=ACCENT,
            background=BG,
        ).pack(anchor="w", pady=(scaled(4, scale), scaled(20, scale)))

        buttons = tk.Frame(container, background=BG)
        buttons.pack(fill="x", pady=(scaled(2, scale), 0))
        self._make_button(buttons, "Take break now", self._start_break, primary=True, scale=scale).pack(side="left")
        self._make_button(buttons, "Reset timer", self._reset_work_timer, scale=scale).pack(side="left", padx=scaled(10, scale))
        self._make_button(buttons, "Quit", self.stop, scale=scale).pack(side="right")

        self._fit_root_to_content()
        self.root.bind("<Configure>", self._keep_control_window_size)

        if self.config.start_minimized:
            self.root.after(250, self.root.iconify)

    def _tick(self) -> None:
        now = time.monotonic()
        elapsed = max(0.0, now - self.last_tick)
        self.last_tick = now

        idle_seconds = self.idle_detector.seconds()
        if self.break_started_at is None:
            self._update_work_timer(elapsed, idle_seconds)
        else:
            self._update_break_overlay(now)

        self.root.after(self.config.tick_ms, self._tick)

    def _update_work_timer(self, elapsed: float, idle_seconds: float) -> None:
        if idle_seconds >= self.config.idle_reset_after_seconds:
            self.active_seconds = 0.0
            self.prepare_panel_dismissed = False
            self._hide_prepare_panel()
            self.status_var.set("Away long enough to count as a rest.")
            self.next_break_var.set(self._format_next_break(self.config.rest_interval_seconds))
            return

        if idle_seconds < self.config.idle_pause_after_seconds:
            self.active_seconds += elapsed
            self.status_var.set("Counting active computer use.")
        else:
            self.prepare_panel_dismissed = False
            self._hide_prepare_panel()
            self.status_var.set("Paused while the computer is idle.")

        seconds_until_break = self.config.rest_interval_seconds - self.active_seconds
        self.next_break_var.set(self._format_next_break(max(0, int(seconds_until_break))))
        if seconds_until_break <= 0:
            self._start_break()
        elif seconds_until_break <= self.config.prepare_notice_seconds:
            if not self.prepare_panel_dismissed:
                self._show_or_update_prepare_panel(max(1, int(seconds_until_break)))
        else:
            self.prepare_panel_dismissed = False
            self._hide_prepare_panel()

    def _start_break(self) -> None:
        if self.break_started_at is not None:
            return

        self.prepare_panel_dismissed = False
        self._hide_prepare_panel()
        self.break_started_at = time.monotonic()
        suggestion = self.config.healthy_suggestions[self.suggestion_index % len(self.config.healthy_suggestions)]
        self.suggestion_index += 1

        self.overlay_windows = []
        self.countdown_labels = []
        self.suggestion_labels = []

        for monitor in self.monitors:
            scale = monitor_scale(monitor)
            window = tk.Toplevel(self.root)
            window.overrideredirect(True)
            self._set_window_attribute(window, "-topmost", True)
            self._set_window_attribute(window, "-alpha", self.config.overlay_alpha)
            window.configure(background=BG)
            window.geometry(f"{monitor.width}x{monitor.height}+{monitor.x}+{monitor.y}")
            window.bind("<Escape>", lambda _event: self._finish_break(skipped=True))
            window.bind("<space>", lambda _event: self._finish_break(skipped=True))
            window.bind("<Button-1>", lambda _event: window.focus_force())

            content = tk.Frame(window, background=BG, padx=scaled(36, scale), pady=scaled(28, scale))
            content.place(relx=0.5, rely=0.48, anchor="center")

            title = tk.Label(
                content,
                text="Rest your eyes",
                font=("Segoe UI", scaled(34, scale), "bold"),
                foreground=TEXT,
                background=BG,
            )
            title.pack(pady=(0, scaled(18, scale)))

            countdown = tk.Label(
                content,
                text=str(self.config.rest_duration_seconds),
                font=("Segoe UI", scaled(96, scale), "bold"),
                foreground=TEXT,
                background=BG,
            )
            countdown.pack()

            suggestion_label = tk.Label(
                content,
                text=suggestion,
                font=("Segoe UI", scaled(20, scale)),
                foreground=ACCENT,
                background=BG,
                wraplength=max(scaled(340, scale), min(scaled(980, scale), monitor.width - scaled(160, scale))),
                justify="center",
            )
            suggestion_label.pack(pady=(scaled(18, scale), scaled(30, scale)))

            skip = self._make_button(content, "Skip break", lambda: self._finish_break(skipped=True), scale=scale)
            skip.pack()

            self.overlay_windows.append(window)
            self.countdown_labels.append(countdown)
            self.suggestion_labels.append(suggestion_label)
            window.focus_force()

        self._update_break_overlay(time.monotonic())

    def _update_break_overlay(self, now: float) -> None:
        if self.break_started_at is None:
            return

        elapsed = now - self.break_started_at
        remaining = max(0, self.config.rest_duration_seconds - int(elapsed))
        self.status_var.set("Resting your eyes.")
        self.next_break_var.set(f"Break remaining: {remaining}s")
        for label in self.countdown_labels:
            label.configure(text=str(remaining))

        if elapsed >= self.config.rest_duration_seconds:
            self._finish_break(skipped=False)

    def _finish_break(self, skipped: bool) -> None:
        for window in self.overlay_windows:
            if window.winfo_exists():
                window.destroy()
        self.overlay_windows = []
        self.countdown_labels = []
        self.suggestion_labels = []
        self.break_started_at = None
        self.active_seconds = 0.0
        self.prepare_panel_dismissed = False
        self.last_tick = time.monotonic()
        self.status_var.set("Break skipped." if skipped else "Break completed.")
        self.next_break_var.set(self._format_next_break(self.config.rest_interval_seconds))

    def _reset_work_timer(self) -> None:
        if self.break_started_at is not None:
            return
        self.active_seconds = 0.0
        self.prepare_panel_dismissed = False
        self._hide_prepare_panel()
        self.status_var.set("Timer reset.")
        self.next_break_var.set(self._format_next_break(self.config.rest_interval_seconds))

    def _show_or_update_prepare_panel(self, seconds_until_break: int) -> None:
        if self.prepare_panel_dismissed:
            return

        self.prepare_countdown_var.set(f"Eye rest starts in {seconds_until_break}s")
        if self.prepare_window is not None and self.prepare_window.winfo_exists():
            return

        monitor = self._primary_monitor()
        scale = monitor_scale(monitor)
        width = min(scaled(460, scale), max(scaled(340, scale), monitor.width - scaled(48, scale)))
        height = scaled(130, scale)
        margin = scaled(24, scale)
        x = monitor.x + monitor.width - width - margin
        y = monitor.y + monitor.height - height - scaled(56, scale)

        window = tk.Toplevel(self.root)
        window.overrideredirect(True)
        self._set_window_attribute(window, "-topmost", True)
        self._set_window_attribute(window, "-alpha", self.config.prepare_panel_alpha)
        window.configure(background=PANEL_BG)
        window.geometry(f"{width}x{height}+{x}+{y}")
        window.bind("<Button-1>", lambda _event: window.focus_force())

        accent = tk.Frame(window, width=scaled(5, scale), background=ACCENT_DARK)
        accent.pack(side="left", fill="y")

        frame = tk.Frame(window, padx=scaled(22, scale), pady=scaled(18, scale), background=PANEL_BG)
        frame.pack(side="left", fill="both", expand=True)

        title = tk.Label(
            frame,
            text="Break soon",
            font=("Segoe UI", 15, "bold"),
            foreground=TEXT,
            background=PANEL_BG,
            anchor="w",
        )
        title.pack(fill="x")

        countdown = tk.Label(
            frame,
            textvariable=self.prepare_countdown_var,
            font=("Segoe UI", 12, "bold"),
            foreground=ACCENT,
            background=PANEL_BG,
            anchor="w",
        )
        countdown.pack(fill="x", pady=(scaled(8, scale), scaled(3, scale)))

        message = tk.Label(
            frame,
            text="Finish what you are doing and look away.",
            font=("Segoe UI", 10),
            foreground=MUTED_TEXT,
            background=PANEL_BG,
            anchor="w",
            wraplength=width - scaled(60, scale),
            justify="left",
        )
        message.pack(fill="x")

        actions = tk.Frame(frame, background=PANEL_BG)
        actions.pack(fill="x", pady=(scaled(14, scale), 0))
        self._make_button(actions, "Hide", self._dismiss_prepare_panel, scale=scale).pack(side="left")

        window.update_idletasks()
        width = max(width, window.winfo_reqwidth())
        height = max(height, window.winfo_reqheight())
        x = monitor.x + monitor.width - width - margin
        y = monitor.y + monitor.height - height - scaled(56, scale)
        window.geometry(f"{width}x{height}+{x}+{y}")

        self.prepare_window = window

    def _hide_prepare_panel(self) -> None:
        if self.prepare_window is not None and self.prepare_window.winfo_exists():
            self.prepare_window.destroy()
        self.prepare_window = None

    def _dismiss_prepare_panel(self) -> None:
        self.prepare_panel_dismissed = True
        self._hide_prepare_panel()

    def _make_button(
        self,
        parent: tk.Misc,
        text: str,
        command: Callable[[], None],
        *,
        primary: bool = False,
        scale: float = 1.0,
    ) -> tk.Button:
        background = BUTTON_BG if primary else BUTTON_SECONDARY_BG
        foreground = BUTTON_FG if primary else BUTTON_SECONDARY_FG
        active_background = "#ffffff" if primary else "#363636"
        button = tk.Button(
            parent,
            text=text,
            command=command,
            font=("Segoe UI", 10, "bold"),
            foreground=foreground,
            background=background,
            activeforeground=foreground,
            activebackground=active_background,
            relief="flat",
            borderwidth=0,
            highlightthickness=0,
            padx=scaled(18, scale),
            pady=scaled(9, scale),
            cursor="hand2",
        )
        button.bind("<Enter>", lambda _event: button.configure(background=active_background))
        button.bind("<Leave>", lambda _event: button.configure(background=background))
        return button

    def _fit_root_to_content(self) -> None:
        self.root.update_idletasks()
        width = max(self.root.winfo_width(), self.root.winfo_reqwidth())
        height = max(self.root.winfo_height(), self.root.winfo_reqheight())
        self.control_window_size = (width, height)
        self.root.geometry(f"{width}x{height}")
        self.root.minsize(width, height)
        self.root.maxsize(width, height)

    def _keep_control_window_size(self, event: tk.Event) -> None:
        if event.widget is not self.root or self.control_window_size is None:
            return
        if self.root.state() != "normal":
            return

        width, height = self.control_window_size
        if event.width == width and event.height == height:
            return
        if self.control_window_resize_job is not None:
            self.root.after_cancel(self.control_window_resize_job)
        self.control_window_resize_job = self.root.after(50, self._restore_control_window_size)

    def _restore_control_window_size(self) -> None:
        self.control_window_resize_job = None
        if self.control_window_size is None or self.root.state() != "normal":
            return
        width, height = self.control_window_size
        self.root.geometry(f"{width}x{height}")

    def _primary_monitor(self) -> Monitor:
        for monitor in self.monitors:
            if monitor.x == 0 and monitor.y == 0:
                return monitor
        return self.monitors[0]

    def _root_scale(self) -> float:
        try:
            return clamp(float(self.root.winfo_fpixels("1i")) / 96.0, 1.0, 1.75)
        except tk.TclError:
            return 1.0

    @staticmethod
    def _set_window_attribute(window: tk.Toplevel, attribute: str, value: object) -> None:
        try:
            window.attributes(attribute, value)
        except tk.TclError:
            pass

    @staticmethod
    def _format_next_break(seconds: int) -> str:
        minutes, remainder = divmod(max(0, seconds), 60)
        if minutes:
            return f"Next break in {minutes}m {remainder}s"
        return f"Next break in {remainder}s"

    def stop(self) -> None:
        if self.control_window_resize_job is not None:
            try:
                self.root.after_cancel(self.control_window_resize_job)
            except tk.TclError:
                pass
            self.control_window_resize_job = None
        self._hide_prepare_panel()
        for window in self.overlay_windows:
            if window.winfo_exists():
                window.destroy()
        self.root.destroy()


def validate_config(config: AppConfig) -> None:
    checks: Iterable[tuple[bool, str]] = (
        (config.rest_interval_seconds > 0, "rest_interval_seconds must be positive"),
        (config.rest_duration_seconds > 0, "rest_duration_seconds must be positive"),
        (config.prepare_notice_seconds >= 0, "prepare_notice_seconds cannot be negative"),
        (config.idle_pause_after_seconds >= 0, "idle_pause_after_seconds cannot be negative"),
        (config.idle_reset_after_seconds >= config.idle_pause_after_seconds, "idle_reset_after_seconds must be >= idle_pause_after_seconds"),
        (bool(config.healthy_suggestions), "healthy_suggestions cannot be empty"),
    )
    errors = [message for is_valid, message in checks if not is_valid]
    if errors:
        raise ValueError("; ".join(errors))


def main(config_factory: Callable[[], AppConfig] = AppConfig) -> None:
    enable_dpi_awareness()
    config = config_factory()
    validate_config(config)
    root = tk.Tk()
    EyeTimerApp(root, config)
    root.mainloop()


if __name__ == "__main__":
    main()
