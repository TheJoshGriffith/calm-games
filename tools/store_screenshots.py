#!/usr/bin/env python3
"""Capture App Store and Google Play screenshots of one game in the iOS simulator.

The game's simulator build is installed fresh for every shot on a 6.9" iPhone
(1320 x 2868) and a 13" iPad (2064 x 2752), launched with the shot's
development flags, and captured with `simctl io screenshot`, in light and in
dark (the simulator's appearance; the app follows the system theme). The
status bar shows Apple's usual 9:41 and a full battery.

Google Play phone images are the iPhone ones without the status bar and home
indicator, which also brings them under Play's 2:1 limit. The iPad images
also suit Play's tablet slots.

    python3 tools/store_screenshots.py --game sudoku --app path/to/CalmSudoku.app --out store-screenshots

Images land in <out>/<game>/<device>/<nn>-<shot>-<theme>.png. Needs Xcode and
Pillow. (The desktop build's --screenshot can't stand in: offscreen grabs at a
scale factor above 1 leave out layered items such as icons and card faces.)
"""

import argparse
import json
import pathlib
import subprocess
import sys
import time

from PIL import Image

# name: (device type to look for, exact screenshot size).
DEVICES = {
    "iphone-6.9": ("Pro Max", (1320, 2868)),
    "ipad-13": ("iPad Pro 13-inch", (2064, 2752)),
}

# Play phone images: the iPhone shot with these many pixels cut from the top
# (status bar) and bottom (home indicator).
PLAY_CROP = (186, 102)

THEMES = ("light", "dark")

# The shots in store order: (file name, launch arguments).
GAMES = {
    "sudoku": [
        ("game", ["--play", "Medium", "--focus", "5"]),
        ("notes", ["--play", "Hard", "--demo-input"]),
        ("killer", ["--variant", "killer"]),
        ("variants", ["--variants"]),
        ("home", []),
        ("settings", ["--settings"]),
    ],
    "solitaire": [
        ("klondike", ["--play", "Klondike"]),
        ("spider", ["--play", "Spider · Two suits"]),
        ("freecell", ["--play", "FreeCell"]),
        ("tripeaks", ["--play", "TriPeaks"]),
        ("home", []),
        ("settings", ["--settings"]),
    ],
    "minesweeper": [
        ("game", ["--play", "Medium", "--tap-centre"]),
        ("easy", ["--play", "Easy", "--tap-centre"]),
        ("home", []),
        ("settings", ["--settings"]),
    ],
    "pinball": [
        ("game", ["--play", "Classic", "--launch"]),
        ("gentle", ["--play", "Gentle"]),
        ("home", []),
        ("settings", ["--settings"]),
    ],
    "mahjong": [
        ("garden", ["--play", "Garden", "--select"]),
        ("fortress", ["--play", "Fortress"]),
        ("butterfly", ["--play", "Butterfly", "--hint"]),
        ("home", []),
        ("settings", ["--settings"]),
    ],
}


def simctl(*args, check=True):
    return subprocess.run(["xcrun", "simctl", *args], check=check, capture_output=True, text=True).stdout


def create_device(pattern):
    """Boots a new simulator of the newest device type whose name has `pattern`."""
    runtimes = [r for r in json.loads(simctl("list", "runtimes", "-j"))["runtimes"]
                if r.get("platform") == "iOS" and r.get("isAvailable")]
    runtime = max(runtimes, key=lambda r: [int(p) for p in r["version"].split(".")])
    types = [t for t in runtime["supportedDeviceTypes"] if pattern in t["name"]]
    if not types:
        raise RuntimeError(f"no '{pattern}' simulator for iOS {runtime['version']}")
    device_type = types[-1]
    print(f"{device_type['name']}, iOS {runtime['version']}", flush=True)
    udid = simctl("create", f"Store {device_type['name']}", device_type["identifier"], runtime["identifier"]).strip()
    simctl("boot", udid)
    simctl("bootstatus", udid, "-b")
    simctl("status_bar", udid, "override", "--time", "9:41", "--dataNetwork", "wifi", "--wifiMode", "active",
           "--wifiBars", "3", "--cellularMode", "active", "--cellularBars", "4",
           "--batteryState", "charged", "--batteryLevel", "100")
    return udid


def capture(udid, app, bundle, args, out, settle):
    simctl("uninstall", udid, bundle, check=False)
    simctl("install", udid, str(app))
    simctl("launch", udid, bundle, *args)
    time.sleep(settle)
    out.parent.mkdir(parents=True, exist_ok=True)
    simctl("io", udid, "screenshot", "--type=png", str(out))
    simctl("terminate", udid, bundle, check=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game", required=True, choices=list(GAMES))
    parser.add_argument("--app", required=True, help="the game's iphonesimulator .app bundle")
    parser.add_argument("--out", default="store-screenshots", help="output directory")
    parser.add_argument("--settle", type=float, default=8, help="seconds from launch to capture")
    opts = parser.parse_args()

    bundle = f"com.thejoshgriffith.calm.{opts.game}"
    failures = 0
    for device, (pattern, size) in DEVICES.items():
        udid = create_device(pattern)
        try:
            for theme in THEMES:
                simctl("ui", udid, "appearance", theme)
                for number, (shot, args) in enumerate(GAMES[opts.game], start=1):
                    name = f"{number:02d}-{shot}-{theme}.png"
                    out = pathlib.Path(opts.out, opts.game, device, name)
                    capture(udid, opts.app, bundle, args, out, opts.settle)
                    with Image.open(out) as image:
                        if image.size != size:
                            print(f"{out}: {image.size}, expected {size}", file=sys.stderr)
                            failures += 1
                            continue
                        print(out, flush=True)
                        if device == "iphone-6.9":
                            top, bottom = PLAY_CROP
                            play = pathlib.Path(opts.out, opts.game, "play-phone", name)
                            play.parent.mkdir(parents=True, exist_ok=True)
                            image.crop((0, top, size[0], size[1] - bottom)).save(play)
        finally:
            simctl("shutdown", udid, check=False)
            simctl("delete", udid, check=False)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
