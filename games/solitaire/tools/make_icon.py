#!/usr/bin/env python3
"""Draws the Solitaire app icon (1024x1024, no transparency, as the stores require).

    python3 games/solitaire/tools/make_icon.py games/solitaire/app/ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png
"""

import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3]
SIZE = 1024
SAGE = "#3D7D78"
CARD = "#FFFFFF"
BACK = "#86C2BA"
INK = "#1F2A30"
RED = "#B4483B"
SCALE = 4  # draw large, then shrink, for smooth edges


def card(width, height, fill, inner=None):
    layer = Image.new("RGBA", (width * SCALE, height * SCALE), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle([0, 0, width * SCALE - 1, height * SCALE - 1], radius=56 * SCALE, fill=fill)
    if inner:
        m = 34 * SCALE
        d.rounded_rectangle([m, m, width * SCALE - m, height * SCALE - m], radius=30 * SCALE,
                            outline=inner, width=8 * SCALE)
    return layer, d


def heart(d, cx, cy, size, colour):
    # The classic parametric heart, size wide.
    k = size / 34
    points = []
    for i in range(720):
        t = 2 * math.pi * i / 720
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        points.append((cx + x * k, cy - y * k))
    d.polygon(points, fill=colour)


def main():
    out = Path(sys.argv[1])
    img = Image.new("RGBA", (SIZE, SIZE), SAGE)
    w, h = 420, 590

    back, _ = card(w, h, BACK, inner="#F2F4F3")
    back = back.resize((w, h), Image.LANCZOS).rotate(12, expand=True, resample=Image.BICUBIC)
    img.alpha_composite(back, (int(SIZE / 2 - back.width / 2 - 95), int(SIZE / 2 - back.height / 2 - 10)))

    face, d = card(w, h, CARD)
    font = ImageFont.truetype(str(ROOT / "framework/qt/fonts/Nunito-ExtraBold.ttf"), 150 * SCALE)
    d.text((52 * SCALE, 20 * SCALE), "A", font=font, fill=RED)
    heart(d, w * SCALE / 2, h * SCALE * 0.6, 300 * SCALE, RED)
    face = face.resize((w, h), Image.LANCZOS).rotate(-8, expand=True, resample=Image.BICUBIC)
    img.alpha_composite(face, (int(SIZE / 2 - face.width / 2 + 80), int(SIZE / 2 - face.height / 2 + 10)))

    out.parent.mkdir(parents=True, exist_ok=True)
    img.convert("RGB").save(out)


if __name__ == "__main__":
    main()
