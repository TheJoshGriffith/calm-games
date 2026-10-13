#!/usr/bin/env python3
"""Draws the Minesweeper app icon (1024x1024, no transparency, as the stores require),
in the style of the Sudoku icon: a small piece of the board floating on sage.

    python3 games/minesweeper/app/ios/make_icon.py games/minesweeper/app/ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[4]
SIZE = 1024
SAGE = "#3D7D78"
OPEN = "#FFFFFF"
HIDDEN = "#E3E9EB"
INK = "#1F2A30"
ENTRY = "#2F6690"


def main():
    out = Path(sys.argv[1])
    img = Image.new("RGB", (SIZE, SIZE), SAGE)
    d = ImageDraw.Draw(img)

    # A 3x3 corner of the field: opened cells, a flag, a still-hidden tile.
    # The numbers are true for mines under the flag and the hidden tile.
    board = 660
    gap = 22
    cell = (board - 2 * gap) / 3
    x0 = y0 = (SIZE - board) // 2
    bold = ImageFont.truetype(str(ROOT / "framework/qt/fonts/Nunito-ExtraBold.ttf"), 170)

    layout = [
        ["", "1", "F"],
        ["", "2", "2"],
        ["", "1", "H"],
    ]
    for row, cells in enumerate(layout):
        for col, kind in enumerate(cells):
            x = x0 + col * (cell + gap)
            y = y0 + row * (cell + gap)
            hidden = kind in ("F", "H")
            d.rounded_rectangle([x, y, x + cell, y + cell], radius=44, fill=HIDDEN if hidden else OPEN)
            cx, cy = x + cell / 2, y + cell / 2
            if kind in ("1", "2"):
                d.text((cx, cy + 4), kind, font=bold, fill=SAGE if kind == "1" else ENTRY, anchor="mm")
            elif kind == "F":
                # Pennant flag, like the in-game icon.
                s = cell / 24
                pole_x = x + 7 * s
                d.line([pole_x, y + 4 * s, pole_x, y + 21 * s], fill=SAGE, width=int(2.4 * s))
                d.polygon([(pole_x, y + 4 * s), (x + 18 * s, y + 4 * s), (x + 15 * s, y + 8 * s),
                           (x + 18 * s, y + 12 * s), (pole_x, y + 12 * s)], fill=SAGE)

    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)


if __name__ == "__main__":
    main()
