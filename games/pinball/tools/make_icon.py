#!/usr/bin/env python3
"""Draws the Pinball app icon (1024x1024) in the Calm palette.

Usage: python3 games/pinball/tools/make_icon.py
Needs Pillow. Writes app/ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png.
"""
import math
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 1024
SS = 4  # supersampling for smooth edges
ACCENT = (61, 125, 120)   # Theme.accent (light)
SURFACE = (255, 255, 255)
SELECTION = (207, 229, 225)
INK = (31, 42, 48)
LINE = (213, 220, 223)

OUT = Path(__file__).resolve().parent.parent / "app/ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png"


def s(v):
    return int(round(v * SS))


def capsule(draw, a, b, r, fill):
    ax, ay = a
    bx, by = b
    length = math.hypot(bx - ax, by - ay)
    nx, ny = -(by - ay) / length * r, (bx - ax) / length * r
    draw.polygon([(s(ax + nx), s(ay + ny)), (s(bx + nx), s(by + ny)),
                  (s(bx - nx), s(by - ny)), (s(ax - nx), s(ay - ny))], fill=fill)
    for x, y in (a, b):
        draw.ellipse([s(x - r), s(y - r), s(x + r), s(y + r)], fill=fill)


def circle(draw, c, r, fill, outline=None, width=0):
    x, y = c
    draw.ellipse([s(x - r), s(y - r), s(x + r), s(y + r)], fill=fill, outline=outline, width=s(width))


def main():
    img = Image.new("RGB", (SIZE * SS, SIZE * SS), ACCENT)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([s(202), s(202), s(822), s(822)], radius=s(96), fill=SURFACE)

    # Bumpers.
    for c, r in (((372, 352), 62), ((604, 408), 74)):
        circle(d, c, r, SELECTION, ACCENT, 14)
        circle(d, c, r * 0.4, SURFACE)
    # Flippers: the left one resting, the right one swung up to strike.
    capsule(d, (300, 662), (462, 724), 24, ACCENT)
    capsule(d, (724, 662), (572, 600), 24, ACCENT)
    # The ball, just struck, with a soft highlight.
    circle(d, (470, 548), 40, INK)
    circle(d, (458, 534), 12, (90, 102, 108))

    img.resize((SIZE, SIZE), Image.LANCZOS).save(OUT)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
