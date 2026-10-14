#!/usr/bin/env python3
"""Draws the Mahjong app icon (1024x1024, no transparency, as the stores require),
in the style of the other Calm icons: a few tiles floating on sage.

    python3 games/mahjong/app/ios/make_icon.py games/mahjong/app/ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 1024
SAGE = "#3D7D78"
FACE = "#FBFCFA"
SIDE = "#A9CBC5"
RED = "#B4483B"
GREEN = "#3D7D78"
BLUE = "#2F6690"


def tile(d, x, y, w, h, depth):
    """A tile with its thickness along the bottom and right; returns the face box."""
    d.rounded_rectangle([x + depth, y + depth, x + w + depth, y + h + depth], radius=w * 0.16, fill=SIDE)
    d.rounded_rectangle([x, y, x + w, y + h], radius=w * 0.16, fill=FACE)
    return x, y, w, h


def ring(d, cx, cy, r, colour, width):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=colour, width=width)


def main():
    out = Path(sys.argv[1])
    img = Image.new("RGB", (SIZE, SIZE), SAGE)
    d = ImageDraw.Draw(img)

    w, h, depth = 300, 390, 30
    # Two tiles side by side, and a third resting on top across them.
    left = tile(d, 190, 430, w, h, depth)
    right = tile(d, 190 + w + 14, 430, w, h, depth)
    top = tile(d, 190 + (w + 14) / 2 - depth, 430 - h / 2 - 2 * depth, w, h, depth)

    # Left: the 1 of Dots, a ringed circle.
    x, y, tw, th = left
    cx, cy = x + tw / 2, y + th / 2
    ring(d, cx, cy, 95, BLUE, 26)
    d.ellipse([cx - 34, cy - 34, cx + 34, cy + 34], fill=RED)

    # Right: the 2 of Bamboo, two sticks.
    x, y, tw, th = right
    for sy in (y + th * 0.28, y + th * 0.72):
        d.rounded_rectangle([x + tw / 2 - 22, sy - 58, x + tw / 2 + 22, sy + 58], radius=22, fill=GREEN)

    # Top: the red dragon, a seal with a bar through it.
    x, y, tw, th = top
    cx, cy = x + tw / 2, y + th / 2
    d.rounded_rectangle([cx - 100, cy - 100, cx + 100, cy + 100], radius=24, outline=RED, width=30)
    d.rounded_rectangle([cx - 15, cy - 140, cx + 15, cy + 140], radius=15, fill=RED)

    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)


if __name__ == "__main__":
    main()
