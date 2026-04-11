#!/usr/bin/env python3
"""
Build Eye Timer into a standalone executable using Nuitka.

Usage:
    python build.py

Requires:
    pip install -e ".[build]"

On first run Nuitka will offer to download MinGW-w64 automatically if no
C compiler is found — accept it.

Output:
    dist/EyeTimer.exe  (Windows)
    dist/EyeTimer      (macOS / Linux)
"""

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent
DIST = ROOT / "dist"

DATA_FILES = [
    "icon.ico",
    "icon.png",
    "universfield-new-notification-017-352293.mp3",
    "universfield-new-notification-017-352293-pitch-changed.mp3",
]


def main() -> None:
    if DIST.exists():
        shutil.rmtree(DIST)

    cmd = [
        sys.executable, "-m", "nuitka",
        "--onefile",
        "--enable-plugin=pyside6",
        "--include-qt-plugins=multimedia",
        "--output-dir=dist",
        "--output-filename=EyeTimer",
        *[f"--include-data-files={f}={f}" for f in DATA_FILES],
    ]

    if sys.platform == "win32":
        cmd += [
            "--windows-console-mode=disable",
            "--windows-icon-from-ico=icon.ico",
        ]
    elif sys.platform == "darwin":
        cmd += ["--macos-app-icon=icon.png"]

    cmd.append(str(ROOT / "eye_timer.py"))

    subprocess.run(cmd, check=True, cwd=ROOT)

    suffix = ".exe" if sys.platform == "win32" else ""
    print(f"\nDone → {DIST / f'EyeTimer{suffix}'}")


if __name__ == "__main__":
    main()
