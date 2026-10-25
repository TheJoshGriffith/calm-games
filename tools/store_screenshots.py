#!/usr/bin/env python3
"""Render App Store and Google Play screenshots of every game, headless.

Each shot starts the desktop build with QT_QPA_PLATFORM=offscreen at a store
size (logical size x scale factor = the exact pixel size the store asks for),
in light and in dark, and saves the window with --screenshot. The theme comes
from a throwaway settings file, so the player's own settings are untouched.

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    python3 tools/store_screenshots.py --build build --out store-screenshots

Images land in <out>/<game>/<device>/<nn>-<shot>-<theme>.png.
"""

import argparse
import os
import pathlib
import subprocess
import sys
import tempfile

# name: (logical width, logical height, scale) -> exact store pixel size.
DEVICES = {
    # App Store Connect: 6.9" iPhone, 1320 x 2868 portrait.
    "iphone-6.9": (440, 956, 3),
    # App Store Connect: 13" iPad, 2064 x 2752 portrait. Also fine for
    # Google Play's 7" and 10" tablet slots (aspect under 2:1, sides < 3840).
    "ipad-13": (1032, 1376, 2),
    # Google Play phone: 1080 x 2160 (9:18, the tallest Play accepts).
    "play-phone": (360, 720, 3),
}

THEMES = ("light", "dark")

# The executable target, the QSettings application name, and the shots in
# store order: (file name, command-line arguments).
GAMES = {
    "sudoku": ("CalmSudoku", "Sudoku", [
        ("game", ["--play", "Medium", "--focus", "5"]),
        ("notes", ["--play", "Hard", "--demo-input"]),
        ("killer", ["--variant", "killer"]),
        ("variants", ["--variants"]),
        ("home", []),
        ("settings", ["--settings"]),
    ]),
    "solitaire": ("CalmSolitaire", "Solitaire", [
        ("klondike", ["--play", "Klondike"]),
        ("spider", ["--play", "Spider · Two suits"]),
        ("freecell", ["--play", "FreeCell"]),
        ("tripeaks", ["--play", "TriPeaks"]),
        ("home", []),
        ("settings", ["--settings"]),
    ]),
    "minesweeper": ("CalmMinesweeper", "Minesweeper", [
        ("game", ["--play", "Medium", "--tap-centre"]),
        ("easy", ["--play", "Easy", "--tap-centre"]),
        ("home", []),
        ("settings", ["--settings"]),
    ]),
    "pinball": ("CalmPinball", "Pinball", [
        ("game", ["--play", "Classic", "--launch"]),
        ("gentle", ["--play", "Gentle"]),
        ("home", []),
        ("settings", ["--settings"]),
    ]),
    "mahjong": ("CalmMahjong", "Mahjong", [
        ("garden", ["--play", "Garden", "--select"]),
        ("fortress", ["--play", "Fortress"]),
        ("butterfly", ["--play", "Butterfly", "--hint"]),
        ("home", []),
        ("settings", ["--settings"]),
    ]),
}


def find_executable(build, target):
    for path in pathlib.Path(build).glob(f"games/*/app/{target}"):
        if path.is_file() and os.access(path, os.X_OK):
            return path
    return None


def capture(exe, app_name, args, device, theme, out):
    width, height, scale = DEVICES[device]
    with tempfile.TemporaryDirectory() as config:
        # Linux QSettings: $XDG_CONFIG_HOME/<organisation>/<application>.conf.
        conf = pathlib.Path(config, "CalmGames", f"{app_name}.conf")
        conf.parent.mkdir(parents=True)
        conf.write_text(f"[app]\ntheme={theme}\n")
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_SCALE_FACTOR=str(scale),
                   XDG_CONFIG_HOME=config, XDG_DATA_HOME=config, XDG_CACHE_HOME=config)
        out.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(exe), *args, "--size", f"{width}x{height}", "--screenshot", str(out)],
                       env=env, check=True, timeout=60,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not out.exists():
        raise RuntimeError(f"no screenshot written: {out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", default="build", help="CMake build directory")
    parser.add_argument("--out", default="store-screenshots", help="output directory")
    parser.add_argument("--games", nargs="*", default=list(GAMES), help="games to capture")
    parser.add_argument("--devices", nargs="*", default=list(DEVICES), help="device sizes to capture")
    opts = parser.parse_args()

    failures = 0
    for game in opts.games:
        target, app_name, shots = GAMES[game]
        exe = find_executable(opts.build, target)
        if exe is None:
            print(f"{game}: {target} not built, skipped", file=sys.stderr)
            failures += 1
            continue
        for device in opts.devices:
            for number, (shot, args) in enumerate(shots, start=1):
                for theme in THEMES:
                    out = pathlib.Path(opts.out, game, device, f"{number:02d}-{shot}-{theme}.png")
                    try:
                        capture(exe, app_name, args, device, theme, out)
                        print(out)
                    except (subprocess.SubprocessError, RuntimeError) as error:
                        print(f"{out}: {error}", file=sys.stderr)
                        failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
