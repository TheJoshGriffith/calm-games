#!/usr/bin/env python3
"""Draw a game's store listing art for Google Play and the App Store.

    python3 tools/make_store_art.py --game sudoku --screens store-screenshots --out store-art

Makes, for Google Play, <out>/<game>/feature-graphic.png (1024 x 500, the
banner at the top of the listing: icon, name and subtitle from
store/<game>.json beside the first light phone screenshot) and
<out>/<game>/icon-512.png (the listing and search icon, from the iOS app
icon); and, for the App Store product page header, header-3840x1646.png and
header-5244x2950.png (three phones on the theme ground, no text, since the
App Store lays the app's name and icon over the header). Needs Pillow.
"""

import argparse
import json
import pathlib

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[1]
FONTS = ROOT / "framework" / "qt" / "fonts"

# Theme.qml, light.
GROUND = "#F2F4F3"
INK = "#1F2A30"
MUTED = "#5E6E76"
ACCENT = "#3D7D78"
LINE = "#D5DCDF"


def rounded(image, radius):
    mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, *image.size), radius, fill=255)
    image.putalpha(mask)
    return image


def phone(shot, width):
    """A phone of `width` pixels showing the screenshot at `shot`."""
    screen = Image.open(shot).convert("RGBA")
    screen = screen.resize((width, round(screen.height * width / screen.width)), Image.LANCZOS)
    frame = max(4, width // 30)
    body = Image.new("RGBA", (width + 2 * frame, screen.height + 2 * frame), INK)
    body.alpha_composite(rounded(screen, width // 9), (frame, frame))
    return rounded(body, width // 9 + frame)


def header(game, screens, size):
    """The App Store header: the first three shots on phones, centred, so any
    crop the store makes keeps the middle phone."""
    width, height = size
    art = Image.new("RGBA", size, GROUND)
    # A soft wash of the accent behind the phones.
    wash = Image.new("L", size, 0)
    ImageDraw.Draw(wash).ellipse((width * 0.2, height * 0.1, width * 0.8, height * 1.3), fill=40)
    wash = wash.filter(ImageFilter.GaussianBlur(height / 8))
    art = Image.composite(Image.new("RGBA", size, ACCENT), art, wash)

    shots = sorted(screens.glob(f"{game}/iphone-6.9/0[1-3]-*-light.png"))
    themes = ["light", "dark", "light"]
    shots = [pathlib.Path(str(s).replace("-light.png", f"-{t}.png")) for s, t in zip(shots, themes)]
    if len(shots) < 3:
        return art.convert("RGB")
    middle_h = height * 0.84
    side_h = middle_h * 0.86
    ratio = 1320 / 2868
    side = [phone(shots[0], round(side_h * ratio)), phone(shots[2], round(side_h * ratio))]
    centre = phone(shots[1], round(middle_h * ratio))
    gap = centre.width * 0.18
    top = round(height * 0.08)
    for image, x in [(side[0], width / 2 - centre.width / 2 - gap - side[0].width),
                     (side[1], width / 2 + centre.width / 2 + gap)]:
        art.alpha_composite(image, (round(x), top + round((middle_h - side_h) / 2)))
    art.alpha_composite(centre, (round(width / 2 - centre.width / 2), top))
    return art.convert("RGB")


def feature_graphic(game, listing, icon, screens):
    width, height = 1024, 500
    art = Image.new("RGBA", (width, height), GROUND)
    draw = ImageDraw.Draw(art)

    # A phone showing the game, rising from the bottom right.
    shot = sorted(screens.glob(f"{game}/iphone-6.9/01-*-light.png"))
    if shot:
        phone_w = 300
        phone = Image.open(shot[0]).convert("RGBA")
        phone = phone.resize((phone_w, round(phone.height * phone_w / phone.width)), Image.LANCZOS)
        frame = 10
        body = Image.new("RGBA", (phone_w + 2 * frame, phone.height + 2 * frame), INK)
        body.alpha_composite(rounded(phone, 34), (frame, frame))
        body = rounded(body, 44)
        art.alpha_composite(body, (width - body.width - 64, 56))

    # Icon, name and subtitle on the left.
    left = 64
    tile = rounded(icon.convert("RGBA").resize((148, 148), Image.LANCZOS), 34)
    art.alpha_composite(tile, (left, 84))
    brand, _, title = listing["name"].partition(" ")
    draw.text((left, 262), brand.upper(), font=ImageFont.truetype(str(FONTS / "Nunito-Bold.ttf"), 26), fill=ACCENT)
    draw.text((left - 3, 292), title, font=ImageFont.truetype(str(FONTS / "Nunito-ExtraBold.ttf"), 72), fill=INK)
    draw.text((left, 386), listing["subtitle"], font=ImageFont.truetype(str(FONTS / "Nunito-SemiBold.ttf"), 30),
              fill=MUTED)
    return art.convert("RGB")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game", required=True)
    parser.add_argument("--screens", default="store-screenshots", help="where store_screenshots.py put the images")
    parser.add_argument("--out", default="store-art")
    opts = parser.parse_args()

    listing = json.loads((ROOT / "store" / f"{opts.game}.json").read_text())
    icon = Image.open(ROOT / "games" / opts.game / "app" / "ios" / "Assets.xcassets" / "AppIcon.appiconset" / "icon-1024.png")
    out = pathlib.Path(opts.out, opts.game)
    out.mkdir(parents=True, exist_ok=True)
    feature_graphic(opts.game, listing, icon, pathlib.Path(opts.screens)).save(out / "feature-graphic.png", optimize=True)
    icon.convert("RGB").resize((512, 512), Image.LANCZOS).save(out / "icon-512.png", optimize=True)
    for size in [(3840, 1646), (5244, 2950)]:
        header(opts.game, pathlib.Path(opts.screens), size).save(out / f"header-{size[0]}x{size[1]}.png", optimize=True)
    print(out)


if __name__ == "__main__":
    main()
