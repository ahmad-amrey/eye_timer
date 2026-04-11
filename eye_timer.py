"""
Cross-platform eye resting reminder.

Run:
    python eye_timer.py

Install dependency:
    pip install PySide6
"""

from __future__ import annotations

import ctypes
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Callable, Iterable

ICON_FILE = "icon.png"
WINDOWS_ICON_FILE = "icon.ico"
WINDOWS_APP_ID = "EyeTimer.App"
REST_START_SOUND_FILE = "universfield-new-notification-017-352293.mp3"
REST_END_SOUND_FILE = "universfield-new-notification-017-352293-pitch-changed.mp3"
SOUND_LEAD_IN_MS = 700


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
    from PySide6.QtCore import QBuffer, QByteArray, QIODevice, QPoint, Qt, QTimer, QUrl
    from PySide6.QtGui import (
        QAction,
        QColor,
        QCloseEvent,
        QIcon,
        QKeyEvent,
        QMouseEvent,
        QPaintEvent,
        QPainter,
        QScreen,
    )
    from PySide6.QtWidgets import (
        QAbstractSpinBox,
        QApplication,
        QCheckBox,
        QFrame,
        QGridLayout,
        QHBoxLayout,
        QLabel,
        QMenu,
        QPushButton,
        QScrollArea,
        QSizePolicy,
        QSpinBox,
        QSystemTrayIcon,
        QTextEdit,
        QVBoxLayout,
        QWidget,
    )
    from PySide6.QtMultimedia import QAudio, QAudioDecoder, QAudioFormat, QAudioSink
except ModuleNotFoundError as exc:
    raise SystemExit("PySide6 is required. Install it with: pip install PySide6") from exc


@dataclass
class ActiveSoundDecode:
    decoder: QAudioDecoder
    audio_format: QAudioFormat | None = None
    chunks: list[bytes] | None = None

    def __post_init__(self) -> None:
        if self.chunks is None:
            self.chunks = []


@dataclass
class ActiveSoundPlayback:
    sink: QAudioSink
    buffer: QBuffer


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

QWidget#countdownWindow {{
    background: {PANEL_BG};
    border: 1px solid #4a4a4a;
}}

QWidget#settingsWindow {{
    background: {BG};
}}

QScrollArea {{
    background: transparent;
    border: 0;
}}

QScrollBar:vertical {{
    background: #2a2a2a;
    border: 0;
    width: 12px;
    margin: 0;
}}

QScrollBar::handle:vertical {{
    background: #a6aaa8;
    border-radius: 6px;
    min-height: 36px;
}}

QScrollBar::handle:vertical:hover {{
    background: #d0d4d2;
}}

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {{
    background: transparent;
    border: 0;
    height: 0;
}}

QLabel#countdownText {{
    background: transparent;
    color: {ACCENT};
    font-size: 16pt;
    font-weight: 800;
}}

QLabel#settingsTitle {{
    background: transparent;
    color: {TEXT};
    font-size: 20pt;
    font-weight: 800;
}}

QLabel#settingsIntro,
QLabel#settingsHint,
QLabel#settingsStatus {{
    background: transparent;
    color: {MUTED_TEXT};
    font-size: 10pt;
}}

QLabel#settingsLabel {{
    background: transparent;
    color: {TEXT};
    font-size: 11pt;
    font-weight: 700;
}}

QSpinBox,
QTextEdit,
QCheckBox {{
    background: #202020;
    color: {TEXT};
    border: 1px solid #303030;
    border-radius: 6px;
    padding: 7px;
    selection-background-color: {ACCENT_DARK};
}}

QSpinBox {{
    min-width: 130px;
    max-width: 130px;
}}

QSpinBox::up-button,
QSpinBox::down-button {{
    width: 0;
    height: 0;
    border: 0;
}}

QTextEdit {{
    min-height: 110px;
}}

QCheckBox {{
    padding: 8px;
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

QPushButton#primaryButton:disabled {{
    background: #3a3a3a;
    color: #8f9692;
}}

QPushButton#secondaryButton {{
    background: {BUTTON_SECONDARY_BG};
    color: {BUTTON_SECONDARY_FG};
}}

QPushButton#secondaryButton:hover {{
    background: #363636;
}}

QPushButton#secondaryButton:disabled {{
    background: #202020;
    color: #686f6b;
}}

QPushButton#countdownHideButton {{
    background: #252525;
    color: {MUTED_TEXT};
    border-radius: 0;
    padding: 0;
    min-width: 26px;
    max-width: 26px;
    min-height: 26px;
    max-height: 26px;
    font-size: 10pt;
    font-weight: 800;
}}

QPushButton#countdownHideButton:hover {{
    background: #343434;
    color: {TEXT};
}}

QMenu {{
    background: {BG};
    color: {TEXT};
    border: 1px solid #303030;
    padding: 6px;
}}

QMenu::item {{
    background: transparent;
    color: {TEXT};
    padding: 9px 34px 9px 16px;
    min-width: 220px;
}}

QMenu::item:selected {{
    background: {BUTTON_SECONDARY_BG};
}}

QMenu::item:disabled {{
    color: {MUTED_TEXT};
}}

QMenu::separator {{
    height: 1px;
    background: #303030;
    margin: 6px 8px;
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
    start_minimized: bool = False
    sound_enabled: bool = True
    healthy_suggestions: tuple[str, ...] = (
        "Look at something at least 6 meters away.",
        "Close your eyes and relax your shoulders.",
        "Let your eyes wander around the room.",
        "Soften your focus and breathe evenly.",
        "Check your posture while your eyes rest.",
    )


SAVED_CONFIG_FIELDS = (
    "rest_interval_seconds",
    "rest_duration_seconds",
    "prepare_notice_seconds",
    "idle_pause_after_seconds",
    "idle_reset_after_seconds",
    "overlay_alpha",
    "prepare_panel_alpha",
    "start_minimized",
    "sound_enabled",
    "healthy_suggestions",
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
    candidates = [Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))]
    app_dir = app_directory()
    if app_dir not in candidates:
        candidates.append(app_dir)
    for base_path in candidates:
        path = base_path / filename
        if path.exists():
            return path
    return candidates[0] / filename


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


class CountdownWindow(QWidget):
    def __init__(self, controller: EyeTimerApp) -> None:
        super().__init__()
        self.controller = controller
        self.drag_offset: QPoint | None = None
        self.setObjectName("countdownWindow")
        self.setWindowTitle("Eye Timer countdown")
        self.setWindowIcon(controller.app_icon)
        window_kind = Qt.WindowType.Tool if controller.tray_available else Qt.WindowType.Window
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | window_kind
        )
        self._build()
        set_native_windows_window_icon(self)

    def _build(self) -> None:
        layout = QHBoxLayout(self)
        layout.setContentsMargins(18, 8, 8, 8)
        layout.setSpacing(10)

        self.countdown_label = QLabel(self.controller.countdown_time_text())
        self.countdown_label.setObjectName("countdownText")
        self.countdown_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.countdown_label.setMinimumWidth(66)
        layout.addWidget(self.countdown_label)

        hide_button = QPushButton("x")
        hide_button.setObjectName("countdownHideButton")
        hide_button.setCursor(Qt.CursorShape.PointingHandCursor)
        hide_button.setToolTip("Hide countdown")
        hide_button.clicked.connect(self.controller.hide_countdown_window)
        layout.addWidget(hide_button)

        self.adjustSize()
        hint = self.sizeHint()
        self.setFixedSize(hint.width(), hint.height())
        self._position_on_primary_screen()

    def set_countdown(self, text: str) -> None:
        self.countdown_label.setText(text)
        self.adjustSize()
        hint = self.sizeHint()
        self.setFixedSize(hint.width(), hint.height())
        self.update()

    def _position_on_primary_screen(self) -> None:
        screen = self.controller.qt_app.primaryScreen()
        if screen is None:
            return
        geometry = screen.availableGeometry()
        margin = 24
        self.move(geometry.right() - self.width() - margin + 1, geometry.top() + margin)

    def closeEvent(self, event: QCloseEvent) -> None:
        if self.controller.stopping:
            event.accept()
            return
        if self.controller.tray_icon is not None:
            self.controller.hide_countdown_window()
        else:
            self.controller.stop()
        event.accept()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.LeftButton:
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

    def contextMenuEvent(self, event) -> None:
        menu = QMenu(self)
        settings = QAction("Settings...", menu)
        settings.triggered.connect(self.controller.show_settings_window)
        menu.addAction(settings)
        menu.addSeparator()
        take_break = QAction("Take break now", menu)
        take_break.triggered.connect(self.controller.start_break)
        menu.addAction(take_break)
        reset_timer = QAction("Reset timer", menu)
        reset_timer.triggered.connect(self.controller.reset_work_timer)
        menu.addAction(reset_timer)
        menu.addSeparator()
        hide_action = QAction("Hide countdown", menu)
        hide_action.triggered.connect(self.controller.hide_countdown_window)
        menu.addAction(hide_action)
        quit_action = QAction("Quit", menu)
        quit_action.triggered.connect(self.controller.stop)
        menu.addAction(quit_action)
        menu.exec(event.globalPos())


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


class SettingsWindow(QWidget):
    def __init__(self, controller: EyeTimerApp) -> None:
        super().__init__(None)
        self.controller = controller
        self.loading_settings = False
        self.setObjectName("settingsWindow")
        self.setWindowTitle("Eye Timer settings")
        self.setWindowFilePath("")
        self.setWindowModified(False)
        self.setWindowIcon(controller.app_icon)
        self.setMinimumWidth(620)
        self._build()
        self.load_from_config()
        self.resize(680, min(700, max(380, self.sizeHint().height())))
        set_native_windows_window_icon(self)

    def _build(self) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(26, 24, 26, 24)
        layout.setSpacing(18)

        title = QLabel("Eye Timer settings")
        title.setObjectName("settingsTitle")
        layout.addWidget(title)

        intro = QLabel("Changes are saved for future starts and applied to the running timer immediately.")
        intro.setObjectName("settingsIntro")
        intro.setWordWrap(True)
        layout.addWidget(intro)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        settings_content = QWidget()
        settings_layout = QVBoxLayout(settings_content)
        settings_layout.setContentsMargins(0, 0, 8, 0)
        settings_layout.setSpacing(16)
        scroll.setWidget(settings_content)

        essential_grid = self._make_settings_grid()
        settings_layout.addLayout(essential_grid)

        self.rest_interval_minutes = self._make_spin_box(1, 480)
        self._add_setting(
            essential_grid,
            "Work interval",
            "Active computer-use time before the full-screen rest overlay appears.",
            self.rest_interval_minutes,
            "min",
        )

        self.rest_duration_seconds = self._make_spin_box(5, 600)
        self._add_setting(
            essential_grid,
            "Rest duration",
            "How long the full-screen rest overlay remains visible before it closes by itself.",
            self.rest_duration_seconds,
            "sec",
        )

        self.advanced_toggle = make_button("Show advanced settings", self.toggle_advanced_settings)
        settings_layout.addWidget(self.advanced_toggle)

        self.advanced_settings = QWidget()
        self.advanced_settings.hide()
        advanced_layout = QVBoxLayout(self.advanced_settings)
        advanced_layout.setContentsMargins(0, 0, 0, 0)
        advanced_layout.setSpacing(16)
        advanced_grid = self._make_settings_grid()
        advanced_layout.addLayout(advanced_grid)
        settings_layout.addWidget(self.advanced_settings)

        self.prepare_notice_seconds = self._make_spin_box(0, 600)
        self._add_setting(
            advanced_grid,
            "Prepare notice",
            "How early the small pre-break panel appears. Set to 0 to disable the warning period.",
            self.prepare_notice_seconds,
            "sec",
        )

        self.idle_pause_after_minutes = self._make_spin_box(0, 60)
        self._add_setting(
            advanced_grid,
            "Pause when idle after",
            "If the computer is idle this long, active-use counting pauses instead of moving toward a break.",
            self.idle_pause_after_minutes,
            "min",
        )

        self.idle_reset_after_minutes = self._make_spin_box(1, 240)
        self._add_setting(
            advanced_grid,
            "Count idle as rest after",
            "If the computer stays idle this long, the app treats it as a completed rest and restarts the interval.",
            self.idle_reset_after_minutes,
            "min",
        )

        self.overlay_opacity_percent = self._make_spin_box(20, 100)
        self._add_setting(
            advanced_grid,
            "Rest overlay opacity",
            "Higher values make the full-screen rest overlay block more of the desktop behind it.",
            self.overlay_opacity_percent,
            "%",
        )

        self.prepare_opacity_percent = self._make_spin_box(20, 100)
        self._add_setting(
            advanced_grid,
            "Prepare panel opacity",
            "Higher values make the small pre-break panel less transparent.",
            self.prepare_opacity_percent,
            "%",
        )

        self.start_minimized = QCheckBox("Start hidden")
        self._add_setting(
            advanced_grid,
            "Start hidden in tray",
            "Takes effect next launch. Off by default, so the countdown starts visible.",
            self.start_minimized,
        )

        self.sound_enabled = QCheckBox("Play tones")
        self._add_setting(
            advanced_grid,
            "Notification tones",
            "Play a short audio file when a rest starts and another when a full rest completes.",
            self.sound_enabled,
        )

        suggestions_label = QLabel("Healthy suggestions")
        suggestions_label.setObjectName("settingsLabel")
        advanced_layout.addWidget(suggestions_label)
        suggestions_hint = QLabel("One suggestion per line. The rest overlay cycles through these messages.")
        suggestions_hint.setObjectName("settingsHint")
        suggestions_hint.setWordWrap(True)
        advanced_layout.addWidget(suggestions_hint)
        self.suggestions_text = QTextEdit()
        advanced_layout.addWidget(self.suggestions_text)

        settings_layout.addStretch(1)
        layout.addWidget(scroll)

        self.status_label = QLabel("")
        self.status_label.setObjectName("settingsStatus")
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)

        button_row = QHBoxLayout()
        button_row.addStretch(1)
        button_row.addWidget(make_button("Close", self.close))
        self.save_button = make_button("Save settings", self.save_settings, primary=True)
        self.set_save_button_changed(False)
        button_row.addWidget(self.save_button)
        layout.addLayout(button_row)

        self._connect_change_signals()

    def _make_spin_box(self, minimum: int, maximum: int) -> QSpinBox:
        box = QSpinBox()
        box.setRange(minimum, maximum)
        box.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)
        return box

    def _make_settings_grid(self) -> QGridLayout:
        grid = QGridLayout()
        grid.setContentsMargins(0, 0, 0, 0)
        grid.setHorizontalSpacing(10)
        grid.setVerticalSpacing(16)
        grid.setColumnStretch(0, 1)
        grid.setColumnMinimumWidth(1, 130)
        grid.setColumnMinimumWidth(2, 32)
        grid.setProperty("nextRow", 0)
        return grid

    def _add_setting(self, layout: QGridLayout, label: str, hint: str, widget: QWidget, unit: str = "") -> None:
        text_column = QVBoxLayout()
        text_column.setSpacing(4)
        title = QLabel(label)
        title.setObjectName("settingsLabel")
        text_column.addWidget(title)
        description = QLabel(hint)
        description.setObjectName("settingsHint")
        description.setWordWrap(True)
        text_column.addWidget(description)

        unit_label = None
        if unit:
            unit_label = QLabel(unit)
            unit_label.setObjectName("settingsHint")
            unit_label.setMinimumWidth(32)

        row = int(layout.property("nextRow") or 0)
        layout.addLayout(text_column, row, 0)
        layout.addWidget(widget, row, 1, Qt.AlignmentFlag.AlignTop)
        if unit_label is not None:
            control_height = max(widget.sizeHint().height(), unit_label.sizeHint().height())
            unit_label.setFixedHeight(control_height)
            unit_label.setAlignment(Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft)
            layout.addWidget(unit_label, row, 2, Qt.AlignmentFlag.AlignTop)
        layout.setProperty("nextRow", row + 1)

    def toggle_advanced_settings(self) -> None:
        is_visible = self.advanced_settings.isVisible()
        self.advanced_settings.setVisible(not is_visible)
        self.advanced_toggle.setText("Show advanced settings" if is_visible else "Hide advanced settings")

    def _connect_change_signals(self) -> None:
        spin_boxes = (
            self.rest_interval_minutes,
            self.rest_duration_seconds,
            self.prepare_notice_seconds,
            self.idle_pause_after_minutes,
            self.idle_reset_after_minutes,
            self.overlay_opacity_percent,
            self.prepare_opacity_percent,
        )
        for box in spin_boxes:
            box.valueChanged.connect(self.update_save_button_state)
        self.start_minimized.toggled.connect(self.update_save_button_state)
        self.sound_enabled.toggled.connect(self.update_save_button_state)
        self.suggestions_text.textChanged.connect(self.update_save_button_state)

    def set_save_button_changed(self, has_changes: bool) -> None:
        self.save_button.setEnabled(has_changes)
        self.save_button.setText("Save settings" if has_changes else "Saved")

    def _current_form_config(self) -> AppConfig:
        suggestions = tuple(
            line.strip()
            for line in self.suggestions_text.toPlainText().splitlines()
            if line.strip()
        )
        return replace(
            self.controller.config,
            rest_interval_seconds=self.rest_interval_minutes.value() * 60,
            rest_duration_seconds=self.rest_duration_seconds.value(),
            prepare_notice_seconds=self.prepare_notice_seconds.value(),
            idle_pause_after_seconds=self.idle_pause_after_minutes.value() * 60,
            idle_reset_after_seconds=self.idle_reset_after_minutes.value() * 60,
            overlay_alpha=self.overlay_opacity_percent.value() / 100,
            prepare_panel_alpha=self.prepare_opacity_percent.value() / 100,
            start_minimized=self.start_minimized.isChecked(),
            sound_enabled=self.sound_enabled.isChecked(),
            healthy_suggestions=suggestions,
        )

    def update_save_button_state(self) -> None:
        if self.loading_settings:
            return
        self.set_save_button_changed(self._current_form_config() != self.controller.config)
        self.status_label.setText("")

    def load_from_config(self) -> None:
        self.loading_settings = True
        config = self.controller.config
        self.advanced_settings.hide()
        self.advanced_toggle.setText("Show advanced settings")
        self.rest_interval_minutes.setValue(max(1, round(config.rest_interval_seconds / 60)))
        self.rest_duration_seconds.setValue(config.rest_duration_seconds)
        self.prepare_notice_seconds.setValue(config.prepare_notice_seconds)
        self.idle_pause_after_minutes.setValue(max(0, round(config.idle_pause_after_seconds / 60)))
        self.idle_reset_after_minutes.setValue(max(1, round(config.idle_reset_after_seconds / 60)))
        self.overlay_opacity_percent.setValue(round(config.overlay_alpha * 100))
        self.prepare_opacity_percent.setValue(round(config.prepare_panel_alpha * 100))
        self.start_minimized.setChecked(config.start_minimized)
        self.sound_enabled.setChecked(config.sound_enabled)
        self.suggestions_text.setPlainText("\n".join(config.healthy_suggestions))
        self.status_label.setText("")
        self.set_save_button_changed(False)
        self.loading_settings = False

    def save_settings(self) -> None:
        config = self._current_form_config()

        try:
            saved_path = self.controller.apply_settings(config)
        except Exception as exc:
            self.status_label.setText(f"Settings were not saved: {exc}")
            return

        self.set_save_button_changed(False)
        self.status_label.setText(f"Settings saved to {saved_path}")

    def showEvent(self, event) -> None:
        super().showEvent(event)
        self.setWindowTitle("Eye Timer settings")
        self.setWindowFilePath("")
        self.setWindowModified(False)


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
        self.tray_available = QSystemTrayIcon.isSystemTrayAvailable()
        self.tray_icon: QSystemTrayIcon | None = None
        self.tray_menu: QMenu | None = None
        self.tray_status_action: QAction | None = None
        self.tray_show_action: QAction | None = None
        self.settings_window: SettingsWindow | None = None
        self.sound_files = {
            "start": REST_START_SOUND_FILE,
            "end": REST_END_SOUND_FILE,
        }
        self.active_sound_decodes: list[ActiveSoundDecode] = []
        self.active_sound_playbacks: list[ActiveSoundPlayback] = []

        self.countdown_window = CountdownWindow(self)
        self.setup_tray_icon()
        self.apply_startup_visibility()

        self.timer = QTimer()
        self.timer.timeout.connect(self.tick)
        self.timer.start(self.config.tick_ms)

    def setup_tray_icon(self) -> None:
        if not self.tray_available:
            return

        tray = QSystemTrayIcon(self.app_icon, self.qt_app)

        menu = QMenu()
        menu.setMinimumWidth(260)
        self.tray_status_action = QAction(self.remaining_time_text(), menu)
        self.tray_status_action.setEnabled(False)
        menu.addAction(self.tray_status_action)
        menu.addSeparator()
        self.tray_show_action = QAction("Show countdown", menu)
        self.tray_show_action.triggered.connect(self.toggle_countdown_window)
        menu.addAction(self.tray_show_action)
        settings = QAction("Settings...", menu)
        settings.triggered.connect(self.show_settings_window)
        menu.addAction(settings)
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

        menu.aboutToShow.connect(self.handle_tray_menu_about_to_show)
        menu.aboutToHide.connect(self.handle_tray_menu_about_to_hide)
        tray.setContextMenu(menu)
        tray.activated.connect(self.handle_tray_activation)
        tray.show()
        self.tray_icon = tray
        self.tray_menu = menu
        self.update_tray_show_action()
        self.update_tray_status()

    def apply_startup_visibility(self) -> None:
        if self.config.start_minimized and self.tray_icon is not None:
            self.countdown_window.hide()
            return
        self.show_countdown_window()

    def handle_tray_activation(self, reason: QSystemTrayIcon.ActivationReason) -> None:
        if reason in (
            QSystemTrayIcon.ActivationReason.Trigger,
            QSystemTrayIcon.ActivationReason.DoubleClick,
            QSystemTrayIcon.ActivationReason.MiddleClick,
        ):
            self.toggle_countdown_window()

    def toggle_countdown_window(self) -> None:
        if self.countdown_window.isVisible():
            self.hide_countdown_window()
        else:
            self.show_countdown_window()

    def show_countdown_window(self) -> None:
        self.countdown_window.show()
        self.countdown_window.raise_()
        self.countdown_window.activateWindow()
        self.update_tray_show_action()

    def hide_countdown_window(self) -> None:
        if self.tray_icon is None:
            self.countdown_window.showMinimized()
            return
        self.countdown_window.hide()
        self.update_tray_show_action()

    def show_settings_window(self) -> None:
        if self.settings_window is None:
            self.settings_window = SettingsWindow(self)
        else:
            self.settings_window.load_from_config()
        self.settings_window.show()
        self.settings_window.raise_()
        self.settings_window.activateWindow()

    def apply_settings(self, config: AppConfig) -> Path:
        validate_config(config)
        saved_path = save_config(config)
        self.config = config
        self.timer.setInterval(config.tick_ms)

        if self.prepare_panel is not None:
            self.prepare_panel.setWindowOpacity(config.prepare_panel_alpha)
            self.prepare_panel.adjustSize()

        alpha = int(max(0.0, min(1.0, config.overlay_alpha)) * 255)
        for overlay in self.overlay_windows:
            overlay.overlay_color = QColor(16, 16, 16, alpha)
            overlay.update()

        if self.break_started_at is not None:
            self.update_break_overlay(time.monotonic())

        self.update_tray_status()
        return saved_path

    def handle_tray_menu_about_to_show(self) -> None:
        self.update_tray_status()

    def handle_tray_menu_about_to_hide(self) -> None:
        self.update_tray_status()

    def update_tray_show_action(self) -> None:
        if self.tray_show_action is not None:
            self.tray_show_action.setText("Hide countdown" if self.countdown_window.isVisible() else "Show countdown")

    def update_tray_status(self) -> None:
        remaining = self.remaining_time_text()
        if self.tray_icon is not None:
            self.tray_icon.setToolTip(f"Eye Timer\n{remaining}")
        if self.tray_status_action is not None:
            self.tray_status_action.setText(remaining)
        self.countdown_window.set_countdown(self.countdown_time_text())

    def remaining_time_text(self) -> str:
        if self.break_started_at is not None:
            elapsed = time.monotonic() - self.break_started_at
            remaining = max(0, self.config.rest_duration_seconds - int(elapsed))
            return f"Break remaining: {remaining}s"
        seconds_until_break = self.config.rest_interval_seconds - self.active_seconds
        return format_next_break(max(0, int(seconds_until_break)))

    def countdown_time_text(self) -> str:
        if self.break_started_at is not None:
            elapsed = time.monotonic() - self.break_started_at
            remaining = max(0, self.config.rest_duration_seconds - int(elapsed))
            return format_countdown(max(0, int(remaining)))
        seconds_until_break = self.config.rest_interval_seconds - self.active_seconds
        return format_countdown(max(0, int(seconds_until_break)))

    def tick(self) -> None:
        now = time.monotonic()
        elapsed = max(0.0, now - self.last_tick)
        self.last_tick = now

        idle_seconds = self.idle_detector.seconds()
        if self.break_started_at is None:
            self.update_work_timer(elapsed, idle_seconds)
        else:
            self.update_break_overlay(now)
        self.update_tray_status()

    def update_work_timer(self, elapsed: float, idle_seconds: float) -> None:
        if idle_seconds >= self.config.idle_reset_after_seconds:
            self.active_seconds = 0.0
            self.prepare_panel_dismissed = False
            self.hide_prepare_panel()
            return

        if idle_seconds < self.config.idle_pause_after_seconds:
            self.active_seconds += elapsed
        else:
            self.prepare_panel_dismissed = False
            self.hide_prepare_panel()

        seconds_until_break = self.config.rest_interval_seconds - self.active_seconds

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
        self.play_notification_tone("start")
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
        if not skipped:
            self.play_notification_tone("end")
        self.update_tray_status()

    def play_notification_tone(self, event: str) -> None:
        if not self.config.sound_enabled:
            return

        filename = self.sound_files.get(event)
        if not filename:
            QApplication.beep()
            return

        sound_path = resource_path(filename)
        if not sound_path.exists():
            QApplication.beep()
            return

        decoder = QAudioDecoder(self.qt_app)
        decode = ActiveSoundDecode(decoder=decoder)
        self.active_sound_decodes.append(decode)
        decoder.bufferReady.connect(lambda active_decode=decode: self.handle_sound_buffer_ready(active_decode))
        decoder.finished.connect(lambda active_decode=decode: self.handle_sound_decode_finished(active_decode))
        decoder.setSource(QUrl.fromLocalFile(str(sound_path)))
        decoder.start()

    def handle_sound_buffer_ready(self, decode: ActiveSoundDecode) -> None:
        buffer = decode.decoder.read()
        if not buffer.isValid():
            return

        if decode.audio_format is None:
            decode.audio_format = buffer.format()
        decode.chunks.append(bytes(buffer.data()))

    def handle_sound_decode_finished(self, decode: ActiveSoundDecode) -> None:
        if decode not in self.active_sound_decodes:
            return

        self.active_sound_decodes.remove(decode)

        if decode.audio_format is None or not decode.chunks:
            decode.decoder.deleteLater()
            QApplication.beep()
            return

        pcm_bytes = self.make_silence(decode.audio_format, SOUND_LEAD_IN_MS) + b"".join(decode.chunks)
        decode.decoder.deleteLater()
        self.start_notification_playback(decode.audio_format, pcm_bytes)

    def start_notification_playback(self, audio_format: QAudioFormat, pcm_bytes: bytes) -> None:
        buffer = QBuffer()
        buffer.setData(QByteArray(pcm_bytes))
        buffer.open(QIODevice.OpenModeFlag.ReadOnly)
        sink = QAudioSink(audio_format, self.qt_app)
        sink.setVolume(0.75)
        playback = ActiveSoundPlayback(sink=sink, buffer=buffer)
        self.active_sound_playbacks.append(playback)
        sink.stateChanged.connect(
            lambda state, active_playback=playback: self.cleanup_sound_playback(state, active_playback)
        )
        sink.start(buffer)

    def cleanup_sound_playback(self, state: QAudio.State, playback: ActiveSoundPlayback) -> None:
        if state == QAudio.State.IdleState:
            playback.sink.stop()
            return

        if state != QAudio.State.StoppedState:
            return

        if playback not in self.active_sound_playbacks:
            return

        self.active_sound_playbacks.remove(playback)
        playback.sink.deleteLater()
        playback.buffer.deleteLater()

    @staticmethod
    def make_silence(audio_format: QAudioFormat, milliseconds: int) -> bytes:
        frame_count = max(0, round(audio_format.sampleRate() * milliseconds / 1000))
        byte_count = audio_format.bytesForFrames(frame_count)
        if audio_format.sampleFormat() == QAudioFormat.SampleFormat.UInt8:
            return b"\x80" * byte_count
        return b"\x00" * byte_count

    def reset_work_timer(self) -> None:
        if self.break_started_at is not None:
            return
        self.active_seconds = 0.0
        self.prepare_panel_dismissed = False
        self.hide_prepare_panel()
        self.update_tray_status()

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
        for decode in list(self.active_sound_decodes):
            decode.decoder.stop()
            decode.decoder.deleteLater()
        self.active_sound_decodes.clear()
        for playback in list(self.active_sound_playbacks):
            self.cleanup_sound_playback(QAudio.State.StoppedState, playback)
        if self.tray_icon is not None:
            self.tray_icon.hide()
            self.tray_icon.deleteLater()
            self.tray_icon = None
            self.tray_menu = None
        self.qt_app.quit()


def format_next_break(seconds: int) -> str:
    minutes, remainder = divmod(max(0, seconds), 60)
    if minutes:
        return f"Next break in {minutes}m {remainder}s"
    return f"Next break in {remainder}s"


def format_countdown(seconds: int) -> str:
    minutes, remainder = divmod(max(0, seconds), 60)
    return f"{minutes:02d}:{remainder:02d}"


def validate_config(config: AppConfig) -> None:
    checks: Iterable[tuple[bool, str]] = (
        (config.rest_interval_seconds > 0, "rest_interval_seconds must be positive"),
        (config.rest_duration_seconds > 0, "rest_duration_seconds must be positive"),
        (config.prepare_notice_seconds >= 0, "prepare_notice_seconds cannot be negative"),
        (config.idle_pause_after_seconds >= 0, "idle_pause_after_seconds cannot be negative"),
        (0.0 <= config.overlay_alpha <= 1.0, "overlay_alpha must be between 0.0 and 1.0"),
        (
            0.0 <= config.prepare_panel_alpha <= 1.0,
            "prepare_panel_alpha must be between 0.0 and 1.0",
        ),
        (
            config.idle_reset_after_seconds >= config.idle_pause_after_seconds,
            "idle_reset_after_seconds must be >= idle_pause_after_seconds",
        ),
        (bool(config.healthy_suggestions), "healthy_suggestions cannot be empty"),
    )
    errors = [message for is_valid, message in checks if not is_valid]
    if errors:
        raise ValueError("; ".join(errors))


def settings_file_path() -> Path:
    return app_directory() / "settings.json"


def app_directory() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent


def save_config(config: AppConfig) -> Path:
    path = settings_file_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    data = {field: getattr(config, field) for field in SAVED_CONFIG_FIELDS}
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")
    return path


def load_saved_config(default_config: AppConfig) -> AppConfig:
    path = settings_file_path()
    if not path.exists():
        return default_config

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(data, dict):
            return default_config
        overrides = {key: value for key, value in data.items() if key in SAVED_CONFIG_FIELDS}
        suggestions = overrides.get("healthy_suggestions")
        if isinstance(suggestions, list):
            overrides["healthy_suggestions"] = tuple(str(item) for item in suggestions if str(item).strip())
        config = replace(default_config, **overrides)
        validate_config(config)
        return config
    except Exception:
        return default_config


def main(config_factory: Callable[[], AppConfig] = AppConfig) -> int:
    config = load_saved_config(config_factory())
    validate_config(config)

    set_windows_app_id()
    qt_app = QApplication(sys.argv)
    qt_app.setApplicationName("Eye Timer")
    qt_app.setApplicationDisplayName("Eye Timer")
    qt_app.setDesktopFileName(WINDOWS_APP_ID)
    qt_app.setWindowIcon(load_app_icon())
    qt_app.setQuitOnLastWindowClosed(False)
    qt_app.setStyleSheet(APP_STYLE)
    if config.start_minimized:
        set_macos_accessory_activation_policy()
    controller = EyeTimerApp(qt_app, config)
    return qt_app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
