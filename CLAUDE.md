# Calm Games

Free, ad-free, open-source Qt 6 games (Sudoku first; Solitaire and Minesweeper
next) sharing one framework. Read `docs/FRAMEWORK.md` before writing game code.

## Build and test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Without Qt installed, only the pure C++ engines and their tests build.
`sudoku_app_smoke` fails on any QML warning, so keep QML warning-free.

Screens can be captured headless for review:
`QT_QPA_PLATFORM=offscreen ./build/games/sudoku/app/CalmSudoku --play Hard --screenshot shot.png`
(every game also takes `--size <w>x<h>`). Offscreen grabs only render layered
items (icons, card faces) at scale factor 1, so store screenshots come from the
iOS simulator instead: the "Store screenshots" workflow runs
`tools/store_screenshots.py` and pushes the images to the `store-screenshots` branch,
with Google Play art from `tools/make_store_art.py`. Store listing text lives in
`store/<game>.json`; "Upload store screenshots" and "Upload store listing" push
the images and text to App Store Connect.

## Ownership

- `framework/` has one owner (the Sudoku/framework thread). Game threads work
  only in `games/<name>/` and ask for framework changes instead of making them.
- Root `CMakeLists.txt` auto-discovers `games/*`; don't edit it to add a game.
- Engines are pure C++20 with no Qt dependency and their own tests.
