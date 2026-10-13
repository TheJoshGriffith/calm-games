#!/usr/bin/env python3
"""Assembles the static web site for one Calm game from a Qt for WebAssembly build.

    python3 tools/web/make-site.py --build-dir build-wasm --game sudoku --out site [--build-id abc123]

The site is plain static files for Cloudflare Pages (or python3 -m http.server):
the app's <Target>.js / <Target>.wasm / qtloader.js, tools/web/index.html (the
branded loading page) and _headers, icons drawn from the game's iOS app icon,
and anything the game adds in games/<name>/app/web/site/ (e.g. _redirects).
"""

import argparse
import os
import re
import shutil
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEB = Path(__file__).resolve().parent


def game_info(name):
    """The app's CMake target and display title for games/<name>/app."""
    cmake = (ROOT / "games" / name / "app" / "CMakeLists.txt").read_text()
    target = re.search(r"qt_add_executable\((\w+)", cmake).group(1)
    return target, name.capitalize()


def make_icons(name, out):
    icon = ROOT / "games" / name / "app" / "ios" / "Assets.xcassets" / "AppIcon.appiconset" / "icon-1024.png"
    if not icon.exists():
        print(f"note: no {icon.relative_to(ROOT)}, the site has no icons", file=sys.stderr)
        return
    try:
        from PIL import Image
    except ImportError:
        print("note: Pillow is not installed, the site has no icons", file=sys.stderr)
        return
    src = Image.open(icon).convert("RGB")
    for filename, size in [("favicon-32.png", 32), ("apple-touch-icon.png", 180), ("icon-192.png", 192)]:
        src.resize((size, size), Image.LANCZOS).save(out / filename, optimize=True)
    src.resize((48, 48), Image.LANCZOS).save(out / "favicon.ico", sizes=[(16, 16), (32, 32), (48, 48)])


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--game", required=True, help="folder name under games/, e.g. sudoku")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--build-id", default=str(int(time.time())))
    args = parser.parse_args()

    target, title = game_info(args.game)
    app_dir = args.build_dir / "games" / args.game / "app"
    files = [f"{target}.js", f"{target}.wasm", "qtloader.js"]
    missing = [f for f in files if not (app_dir / f).exists()]
    if missing:
        sys.exit(f"missing {', '.join(missing)} in {app_dir} (is this a Qt for WebAssembly build?)")

    out = args.out
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    for f in files:
        shutil.copy2(app_dir / f, out / f)
    shutil.copy2(WEB / "_headers", out / "_headers")
    make_icons(args.game, out)

    # Our branded page replaces Qt's generated <Target>.html. The build id
    # makes fresh URLs for the engine files on every deploy, so they can be
    # cached hard (see _headers).
    page = (WEB / "index.html").read_text()
    for token, value in {"__TITLE__": f"Gwynant {title}", "__TARGET__": target, "__BUILD__": args.build_id}.items():
        page = page.replace(token, value)
    (out / "index.html").write_text(page)

    # Game-specific static files (and overrides) last.
    extra = ROOT / "games" / args.game / "app" / "web" / "site"
    if extra.is_dir():
        shutil.copytree(extra, out, dirs_exist_ok=True)

    # Apple's Universal Links file names the app by team ID, which comes from
    # the APPLE_TEAM_ID environment variable; without it the file is left out.
    aasa = out / ".well-known" / "apple-app-site-association"
    if aasa.exists():
        team = os.environ.get("APPLE_TEAM_ID", "").strip()
        if team:
            aasa.write_text(aasa.read_text().replace("@APPLE_TEAM_ID@", team))
        else:
            print("note: APPLE_TEAM_ID is not set, the site has no apple-app-site-association", file=sys.stderr)
            aasa.unlink()
            if not any(aasa.parent.iterdir()):
                aasa.parent.rmdir()

    for f in sorted(p for p in out.rglob("*") if p.is_file()):
        print(f"{f.stat().st_size:>12}  {f.relative_to(out)}")


if __name__ == "__main__":
    main()
