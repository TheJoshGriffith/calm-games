#!/usr/bin/env python3
"""Draws the Sudoku app icon (1024x1024, no transparency, as the stores require).

    python3 tools/make_sudoku_icon.py games/sudoku/app/ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SIZE = 1024
SAGE = "#3D7D78"
BOARD = "#FFFFFF"
LINE = "#D5DCDF"
SELECTION = "#CFE5E1"
INK = "#1F2A30"
ENTRY = "#2F6690"


def main():
    out = Path(sys.argv[1])
    img = Image.new("RGB", (SIZE, SIZE), SAGE)
    d = ImageDraw.Draw(img)

    # One 3x3 box of the board, floating on sage.
    board = 620
    x0 = y0 = (SIZE - board) // 2
    d.rounded_rectangle([x0, y0, x0 + board, y0 + board], radius=72, fill=BOARD)
    cell = board / 3

    # The selected cell.
    sx, sy = x0 + cell * 2, y0 + cell * 1
    d.rounded_rectangle([sx + 14, sy + 14, sx + cell - 14, sy + cell - 14], radius=36, fill=SELECTION)

    for i in (1, 2):
        d.line([x0 + i * cell, y0, x0 + i * cell, y0 + board], fill=LINE, width=8)
        d.line([x0, y0 + i * cell, x0 + board, y0 + i * cell], fill=LINE, width=8)

    bold = ImageFont.truetype(str(ROOT / "framework/qt/fonts/Nunito-ExtraBold.ttf"), 150)
    semi = ImageFont.truetype(str(ROOT / "framework/qt/fonts/Nunito-Bold.ttf"), 150)
    digits = [(0, 0, "5", bold, INK), (1, 0, "3", bold, INK), (0, 2, "9", bold, INK),
              (1, 1, "7", semi, ENTRY), (2, 2, "1", bold, INK)]
    for col, row, text, font, colour in digits:
        cx = x0 + cell * col + cell / 2
        cy = y0 + cell * row + cell / 2
        d.text((cx, cy), text, font=font, fill=colour, anchor="mm")

    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)


if __name__ == "__main__":
    main()
