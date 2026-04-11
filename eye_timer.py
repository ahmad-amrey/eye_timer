"""
Cross-platform eye resting reminder.

Run:
    python eye_timer.py

Install dependency:
    pip install PySide6
"""

from __future__ import annotations

import ctypes
import platform
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

ICON_FILE = "icon.png"
WINDOWS_ICON_FILE = "icon.ico"
WINDOWS_APP_ID = "EyeTimer.App"


def set_windows_app_id() -> None:
    if platform.system().lower() != "windows":
        return
    try:
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID.argtypes = [ctypes.c_wchar_p]
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID.restype = ctypes.c_long
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(WINDOWS_APP_ID)
    except Exception:
        pass


def set_macos_accessory_activation_policy() -> None:
    if platform.system().lower() != "darwin":
        return
    try:
        from AppKit import NSApp, NSApplicationActivationPolicyAccessory  # type: ignore

        app = NSApp()
        if app is not None:
            app.setActivationPolicy_(NSApplicationActivationPolicyAccessory)
    except Exception:
        pass


set_windows_app_id()

try:
    from PySide6.QtCore import QPoint, Qt, QTimer
    from PySide6.QtGui import QAction, QColor, QCloseEvent, QIcon, QKeyEvent, QMouseEvent, QPaintEvent, QPainter, QScreen
    from PySide6.QtWidgets import (
        QApplication,
        QFrame,
        QHBoxLayout,
        QLabel,
        QMenu,
        QPushButton,
        QSizePolicy,
        QSystemTrayIcon,
        QVBoxLayout,
        QWidget,
    )
except ModuleNotFoundError as exc:
    raise SystemExit("PySide6 is required. Install it with: pip install PySide6") from exc


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


APP_STYLE = f"""
QWidget {{
    font-family: "Segoe UI", "Inter", "Arial";
    background: {BG};
    color: {TEXT};
}}

QWidget#controlWindow {{
    background: {BG};
}}

QLabel#appTitle {{
    font-size: 22pt;
    font-weight: 700;
}}

QLabel#statusLabel {{
    color: {MUTED_TEXT};
    font-size: 11pt;
}}

QLabel#nextBreakLabel {{
    color: {ACCENT};
    font-size: 17pt;
    font-weight: 700;
}}

QWidget#overlayWindow {{
    background-color: rgba(16, 16, 16, 220);
}}

QLabel#overlayTitle {{
    color: {TEXT};
    background: transparent;
    font-size: 34pt;
    font-weight: 700;
}}

QLabel#overlayCountdown {{
    color: {TEXT};
    background: transparent;
    font-size: 88pt;
    font-weight: 800;
}}

QLabel#overlaySuggestion {{
    color: {ACCENT};
    background: transparent;
    font-size: 20pt;
}}

QWidget#preparePanel {{
    background: {PANEL_BG};
}}

QFrame#prepareAccent {{
    background: {ACCENT_DARK};
}}

QLabel#prepareTitle {{
    color: {TEXT};
    background: transparent;
    font-size: 15pt;
    font-weight: 700;
}}

QLabel#prepareCountdown {{
    color: {ACCENT};
    background: transparent;
    font-size: 12pt;
    font-weight: 700;
}}

QLabel#prepareMessage {{
    color: {MUTED_TEXT};
    background: transparent;
    font-size: 10pt;
}}

QPushButton {{
    border: 0;
    border-radius: 8px;
    padding: 9px 18px;
    font-size: 10pt;
    font-weight: 700;
}}

QPushButton#primaryButton {{
    background: {BUTTON_BG};
    color: {BUTTON_FG};
}}

QPushButton#primaryButton:hover {{
    background: #ffffff;
}}

QPushButton#secondaryButton {{
    background: {BUTTON_SECONDARY_BG};
    color: {BUTTON_SECONDARY_FG};
}}

QPushButton#secondaryButton:hover {{
    background: #363636;
}}

QPushButton#windowButton {{
    background: {BUTTON_SECONDARY_BG};
    color: {BUTTON_SECONDARY_FG};
    border-radius: 8px;
    padding: 0;
    min-width: 34px;
    max-width: 34px;
    min-height: 28px;
    max-height: 28px;
    font-size: 9pt;
}}

QPushButton#windowButton:hover {{
    background: #363636;
}}
"""


@dataclass(frozen=True)
class AppConfig:
    rest_interval_seconds: int = 20 * 60
    rest_duration_seconds: int = 20
    prepare_notice_seconds: int = 20
    idle_pause_after_seconds: int = 60
    idle_reset_after_seconds: int = 10 * 60
    tick_ms: int = 1000
    overlay_alpha: float = 0.86
    prepare_panel_alpha: float = 0.96
    start_minimized: bool = True
    use_system_tray: bool = True
    healthy_suggestions: tuple[str, ...] = (
        "Look at something at least 6 meters away.",
        "Close your eyes and relax your shoulders.",
        "Let your eyes wander around the room.",
        "Soften your focus and breathe evenly.",
        "Check your posture while your eyes rest.",
    )


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


def make_button(text: str, callback: Callable[[], None], *, primary: bool = False) -> QPushButton:
    button = QPushButton(text)
    button.setObjectName("primaryButton" if primary else "secondaryButton")
    button.setCursor(Qt.CursorShape.PointingHandCursor)
    button.clicked.connect(callback)
    return button


def resource_path(filename: str) -> Path:
    base_path = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return base_path / filename


def load_app_icon() -> QIcon:
    for filename in icon_filenames():
        icon_path = resource_path(filename)
        if icon_path.exists():
            return QIcon(str(icon_path))
    return QIcon()


def icon_filenames() -> tuple[str, ...]:
    if platform.system().lower() == "windows":
        return (WINDOWS_ICON_FILE, ICON_FILE)
    return (ICON_FILE, WINDOWS_ICON_FILE)


def set_native_windows_window_icon(widget: QWidget) -> None:
    if platform.system().lower() != "windows":
        return

    icon_path = resource_path(WINDOWS_ICON_FILE)
    if not icon_path.exists():
        return

    image_icon = 1
    lr_load_from_file = 0x00000010
    lr_default_size = 0x00000040
    wm_set_icon = 0x0080
    icon_small = 0
    icon_big = 1

    try:
        user32 = ctypes.windll.user32
        user32.LoadImageW.argtypes = [
            ctypes.c_void_p,
            ctypes.c_wchar_p,
            ctypes.c_uint,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_uint,
        ]
        user32.LoadImageW.restype = ctypes.c_void_p
        user32.SendMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_size_t, ctypes.c_void_p]
        user32.SendMessageW.restype = ctypes.c_void_p
        hicon = user32.LoadImageW(None, str(icon_path), image_icon, 0, 0, lr_load_from_file | lr_default_size)
        if not hicon:
            return
        hwnd = ctypes.c_void_p(int(widget.winId()))
        user32.SendMessageW(hwnd, wm_set_icon, icon_small, hicon)
        user32.SendMessageW(hwnd, wm_set_icon, icon_big, hicon)
        widget._native_windows_icons = getattr(widget, "_native_windows_icons", []) + [hicon]
    except Exception:
        pass


class ControlWindow(QWidget):
    def __init__(self, controller: EyeTimerApp) -> None:
        super().__init__()
        self.controller = controller
        self.drag_offset: QPoint | None = None
        self.setObjectName("controlWindow")
        self.setWindowTitle("Eye Timer")
        self.setWindowIcon(controller.app_icon)
        window_kind = Qt.WindowType.Tool if controller.tray_available else Qt.WindowType.Window
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint | window_kind)
        self.setMinimumWidth(520)
        self._build()
        set_native_windows_window_icon(self)

    def _build(self) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(22, 18, 22, 24)
        layout.setSpacing(14)

        title_bar = QHBoxLayout()
        title_bar.setContentsMargins(0, 0, 0, 8)
        title_bar.setSpacing(8)
        title_bar.addStretch(1)

        minimize = QPushButton("_")
        minimize.setObjectName("windowButton")
        minimize.setCursor(Qt.CursorShape.PointingHandCursor)
        minimize.clicked.connect(self.controller.hide_control_window)
        title_bar.addWidget(minimize)
        layout.addLayout(title_bar)

        title = QLabel("Eye Timer")
        title.setObjectName("appTitle")
        layout.addWidget(title)

        self.status_label = QLabel("Starting.")
        self.status_label.setObjectName("statusLabel")
        layout.addWidget(self.status_label)

        self.next_break_label = QLabel(format_next_break(self.controller.config.rest_interval_seconds))
        self.next_break_label.setObjectName("nextBreakLabel")
        layout.addWidget(self.next_break_label)

        button_row = QHBoxLayout()
        button_row.setContentsMargins(0, 10, 0, 0)
        button_row.setSpacing(10)
        button_row.addWidget(make_button("Take break now", self.controller.start_break, primary=True))
        button_row.addWidget(make_button("Reset timer", self.controller.reset_work_timer))
        button_row.addStretch(1)
        button_row.addWidget(make_button("Quit", self.controller.stop))
        layout.addLayout(button_row)

        self.adjustSize()
        hint = self.sizeHint()
        self.setFixedSize(max(520, hint.width()), hint.height())

    def set_status(self, text: str) -> None:
        self.status_label.setText(text)

    def set_next_break(self, text: str) -> None:
        self.next_break_label.setText(text)

    def closeEvent(self, event: QCloseEvent) -> None:
        if self.controller.stopping:
            event.accept()
            return
        if self.controller.tray_icon is not None:
            self.controller.hide_control_window()
        else:
            self.controller.stop()
        event.accept()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.LeftButton and not isinstance(self.childAt(event.position().toPoint()), QPushButton):
            self.drag_offset = event.globalPosition().toPoint() - self.frameGeometry().topLeft()
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        if self.drag_offset is not None and event.buttons() & Qt.MouseButton.LeftButton:
            self.move(event.globalPosition().toPoint() - self.drag_offset)
            event.accept()
            return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        self.drag_offset = None
        super().mouseReleaseEvent(event)


class BreakOverlay(QWidget):
    def __init__(self, controller: EyeTimerApp, screen: QScreen, suggestion: str) -> None:
        super().__init__(None)
        self.controller = controller
        self.setObjectName("overlayWindow")
        alpha = int(max(0.0, min(1.0, controller.config.overlay_alpha)) * 255)
        self.overlay_color = QColor(16, 16, 16, alpha)
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setGeometry(screen.geometry())
        self._build(suggestion)

    def _build(self, suggestion: str) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(80, 80, 80, 80)
        layout.setSpacing(20)
        layout.addStretch(1)

        title = QLabel("Rest your eyes")
        title.setObjectName("overlayTitle")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)

        self.countdown_label = QLabel(str(self.controller.config.rest_duration_seconds))
        self.countdown_label.setObjectName("overlayCountdown")
        self.countdown_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.countdown_label)

        suggestion_label = QLabel(suggestion)
        suggestion_label.setObjectName("overlaySuggestion")
        suggestion_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        suggestion_label.setWordWrap(True)
        suggestion_label.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        layout.addWidget(suggestion_label)

        skip_row = QHBoxLayout()
        skip_row.addStretch(1)
        skip_row.addWidget(make_button("Skip break", lambda: self.controller.finish_break(skipped=True)))
        skip_row.addStretch(1)
        layout.addLayout(skip_row)

        layout.addStretch(1)

    def set_remaining(self, seconds: int) -> None:
        self.countdown_label.setText(str(seconds))

    def paintEvent(self, event: QPaintEvent) -> None:
        painter = QPainter(self)
        painter.fillRect(self.rect(), self.overlay_color)

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() in (Qt.Key.Key_Escape, Qt.Key.Key_Space):
            self.controller.finish_break(skipped=True)
            return
        super().keyPressEvent(event)


class PreparePanel(QWidget):
    def __init__(self, controller: EyeTimerApp, screen: QScreen, seconds_until_break: int) -> None:
        super().__init__(None)
        self.controller = controller
        self.screen = screen
        self.setObjectName("preparePanel")
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setWindowOpacity(controller.config.prepare_panel_alpha)
        self._build(seconds_until_break)
        self._position()

    def _build(self, seconds_until_break: int) -> None:
        outer = QHBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        accent = QFrame()
        accent.setObjectName("prepareAccent")
        accent.setFixedWidth(5)
        outer.addWidget(accent)

        content = QVBoxLayout()
        content.setContentsMargins(22, 18, 22, 18)
        content.setSpacing(8)
        outer.addLayout(content)

        header = QHBoxLayout()
        header.setSpacing(12)
        title = QLabel("Break soon")
        title.setObjectName("prepareTitle")
        header.addWidget(title)
        header.addStretch(1)
        header.addWidget(make_button("Hide", self.controller.dismiss_prepare_panel))
        content.addLayout(header)

        self.countdown_label = QLabel()
        self.countdown_label.setObjectName("prepareCountdown")
        content.addWidget(self.countdown_label)

        message = QLabel("Finish what you are doing and look away.")
        message.setObjectName("prepareMessage")
        message.setWordWrap(True)
        content.addWidget(message)

        self.setFixedWidth(min(460, max(280, self.screen.availableGeometry().width() - 48)))
        self.set_countdown(seconds_until_break)
        self.adjustSize()

    def set_countdown(self, seconds_until_break: int) -> None:
        self.countdown_label.setText(f"Eye rest starts in {seconds_until_break}s")
        self.adjustSize()
        self._position()

    def _position(self) -> None:
        geometry = self.screen.availableGeometry()
        margin = 24
        x = geometry.right() - self.width() - margin + 1
        y = geometry.bottom() - self.height() - margin + 1
        self.move(x, y)


class EyeTimerApp:
    def __init__(self, qt_app: QApplication, config: AppConfig) -> None:
        self.qt_app = qt_app
        self.config = config
        self.idle_detector = IdleDetector()
        self.active_seconds = 0.0
        self.last_tick = time.monotonic()
        self.break_started_at: float | None = None
        self.suggestion_index = 0
        self.overlay_windows: list[BreakOverlay] = []
        self.prepare_panel: PreparePanel | None = None
        self.prepare_panel_dismissed = False
        self.stopping = False
        self.app_icon = qt_app.windowIcon()
        self.tray_available = self.config.use_system_tray and QSystemTrayIcon.isSystemTrayAvailable()
        self.tray_icon: QSystemTrayIcon | None = None
        self.tray_show_action: QAction | None = None

        self.control_window = ControlWindow(self)
        self.setup_tray_icon()
        self.apply_startup_visibility()

        self.timer = QTimer()
        self.timer.timeout.connect(self.tick)
        self.timer.start(self.config.tick_ms)

    def setup_tray_icon(self) -> None:
        if not self.tray_available:
            return

        tray = QSystemTrayIcon(self.app_icon, self.qt_app)
        tray.setToolTip("Eye Timer")

        menu = QMenu()
        self.tray_show_action = QAction("Show Eye Timer", menu)
        self.tray_show_action.triggered.connect(self.toggle_control_window)
        menu.addAction(self.tray_show_action)
        menu.addSeparator()
        take_break = QAction("Take break now", menu)
        take_break.triggered.connect(self.start_break)
        menu.addAction(take_break)
        reset_timer = QAction("Reset timer", menu)
        reset_timer.triggered.connect(self.reset_work_timer)
        menu.addAction(reset_timer)
        menu.addSeparator()
        quit_action = QAction("Quit", menu)
        quit_action.triggered.connect(self.stop)
        menu.addAction(quit_action)

        tray.setContextMenu(menu)
        tray.activated.connect(self.handle_tray_activation)
        tray.show()
        self.tray_icon = tray
        self.update_tray_show_action()

    def apply_startup_visibility(self) -> None:
        if self.config.start_minimized and self.tray_icon is not None:
            self.control_window.hide()
            return
        self.show_control_window()

    def handle_tray_activation(self, reason: QSystemTrayIcon.ActivationReason) -> None:
        if reason in (
            QSystemTrayIcon.ActivationReason.Trigger,
            QSystemTrayIcon.ActivationReason.DoubleClick,
            QSystemTrayIcon.ActivationReason.MiddleClick,
        ):
            self.toggle_control_window()

    def toggle_control_window(self) -> None:
        if self.control_window.isVisible():
            self.hide_control_window()
        else:
            self.show_control_window()

    def show_control_window(self) -> None:
        self.control_window.show()
        self.control_window.raise_()
        self.control_window.activateWindow()
        self.update_tray_show_action()

    def hide_control_window(self) -> None:
        if self.tray_icon is None:
            self.control_window.showMinimized()
            return
        self.control_window.hide()
        self.update_tray_show_action()

    def update_tray_show_action(self) -> None:
        if self.tray_show_action is not None:
            self.tray_show_action.setText("Hide Eye Timer" if self.control_window.isVisible() else "Show Eye Timer")

    def tick(self) -> None:
        now = time.monotonic()
        elapsed = max(0.0, now - self.last_tick)
        self.last_tick = now

        idle_seconds = self.idle_detector.seconds()
        if self.break_started_at is None:
            self.update_work_timer(elapsed, idle_seconds)
        else:
            self.update_break_overlay(now)

    def update_work_timer(self, elapsed: float, idle_seconds: float) -> None:
        if idle_seconds >= self.config.idle_reset_after_seconds:
            self.active_seconds = 0.0
            self.prepare_panel_dismissed = False
            self.hide_prepare_panel()
            self.control_window.set_status("Away long enough to count as a rest.")
            self.control_window.set_next_break(format_next_break(self.config.rest_interval_seconds))
            return

        if idle_seconds < self.config.idle_pause_after_seconds:
            self.active_seconds += elapsed
            self.control_window.set_status("Counting active computer use.")
        else:
            self.prepare_panel_dismissed = False
            self.hide_prepare_panel()
            self.control_window.set_status("Paused while the computer is idle.")

        seconds_until_break = self.config.rest_interval_seconds - self.active_seconds
        self.control_window.set_next_break(format_next_break(max(0, int(seconds_until_break))))

        if seconds_until_break <= 0:
            self.start_break()
        elif seconds_until_break <= self.config.prepare_notice_seconds:
            if not self.prepare_panel_dismissed:
                self.show_or_update_prepare_panel(max(1, int(seconds_until_break)))
        else:
            self.prepare_panel_dismissed = False
            self.hide_prepare_panel()

    def start_break(self) -> None:
        if self.break_started_at is not None:
            return

        self.prepare_panel_dismissed = False
        self.hide_prepare_panel()
        self.break_started_at = time.monotonic()
        suggestion = self.config.healthy_suggestions[self.suggestion_index % len(self.config.healthy_suggestions)]
        self.suggestion_index += 1

        self.overlay_windows = []
        for screen in self.qt_app.screens():
            overlay = BreakOverlay(self, screen, suggestion)
            overlay.show()
            overlay.raise_()
            self.overlay_windows.append(overlay)

        if self.overlay_windows:
            self.overlay_windows[0].activateWindow()
        self.update_break_overlay(time.monotonic())

    def update_break_overlay(self, now: float) -> None:
        if self.break_started_at is None:
            return

        elapsed = now - self.break_started_at
        remaining = max(0, self.config.rest_duration_seconds - int(elapsed))
        self.control_window.set_status("Resting your eyes.")
        self.control_window.set_next_break(f"Break remaining: {remaining}s")
        for overlay in self.overlay_windows:
            overlay.set_remaining(remaining)

        if elapsed >= self.config.rest_duration_seconds:
            self.finish_break(skipped=False)

    def finish_break(self, skipped: bool) -> None:
        for overlay in self.overlay_windows:
            overlay.close()
            overlay.deleteLater()
        self.overlay_windows = []
        self.break_started_at = None
        self.active_seconds = 0.0
        self.prepare_panel_dismissed = False
        self.last_tick = time.monotonic()
        self.control_window.set_status("Break skipped." if skipped else "Break completed.")
        self.control_window.set_next_break(format_next_break(self.config.rest_interval_seconds))

    def reset_work_timer(self) -> None:
        if self.break_started_at is not None:
            return
        self.active_seconds = 0.0
        self.prepare_panel_dismissed = False
        self.hide_prepare_panel()
        self.control_window.set_status("Timer reset.")
        self.control_window.set_next_break(format_next_break(self.config.rest_interval_seconds))

    def show_or_update_prepare_panel(self, seconds_until_break: int) -> None:
        if self.prepare_panel_dismissed:
            return

        if self.prepare_panel is not None:
            self.prepare_panel.set_countdown(seconds_until_break)
            self.prepare_panel.show()
            self.prepare_panel.raise_()
            return

        screen = self.qt_app.primaryScreen()
        self.prepare_panel = PreparePanel(self, screen, seconds_until_break)
        self.prepare_panel.show()
        self.prepare_panel.raise_()

    def hide_prepare_panel(self) -> None:
        if self.prepare_panel is not None:
            self.prepare_panel.close()
            self.prepare_panel.deleteLater()
            self.prepare_panel = None

    def dismiss_prepare_panel(self) -> None:
        self.prepare_panel_dismissed = True
        self.hide_prepare_panel()

    def stop(self) -> None:
        if self.stopping:
            return
        self.stopping = True
        self.timer.stop()
        self.hide_prepare_panel()
        for overlay in self.overlay_windows:
            overlay.close()
            overlay.deleteLater()
        self.overlay_windows = []
        if self.tray_icon is not None:
            self.tray_icon.hide()
            self.tray_icon.deleteLater()
            self.tray_icon = None
        self.qt_app.quit()


def format_next_break(seconds: int) -> str:
    minutes, remainder = divmod(max(0, seconds), 60)
    if minutes:
        return f"Next break in {minutes}m {remainder}s"
    return f"Next break in {remainder}s"


def validate_config(config: AppConfig) -> None:
    checks: Iterable[tuple[bool, str]] = (
        (config.rest_interval_seconds > 0, "rest_interval_seconds must be positive"),
        (config.rest_duration_seconds > 0, "rest_duration_seconds must be positive"),
        (config.prepare_notice_seconds >= 0, "prepare_notice_seconds cannot be negative"),
        (config.idle_pause_after_seconds >= 0, "idle_pause_after_seconds cannot be negative"),
        (
            config.idle_reset_after_seconds >= config.idle_pause_after_seconds,
            "idle_reset_after_seconds must be >= idle_pause_after_seconds",
        ),
        (bool(config.healthy_suggestions), "healthy_suggestions cannot be empty"),
    )
    errors = [message for is_valid, message in checks if not is_valid]
    if errors:
        raise ValueError("; ".join(errors))


def main(config_factory: Callable[[], AppConfig] = AppConfig) -> int:
    config = config_factory()
    validate_config(config)

    set_windows_app_id()
    qt_app = QApplication(sys.argv)
    qt_app.setApplicationName("Eye Timer")
    qt_app.setApplicationDisplayName("Eye Timer")
    qt_app.setDesktopFileName(WINDOWS_APP_ID)
    qt_app.setWindowIcon(load_app_icon())
    qt_app.setQuitOnLastWindowClosed(False)
    qt_app.setStyleSheet(APP_STYLE)
    if config.use_system_tray and config.start_minimized:
        set_macos_accessory_activation_policy()
    controller = EyeTimerApp(qt_app, config)
    return qt_app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
