# Calm Games

Free, ad-free, open-source puzzle games for iOS and Android, built with Qt 6.
No ads, no in-app purchases, no accounts, no tracking.

The first game is **Sudoku**: endless generated puzzles, each with exactly one
solution, graded Easy to Expert by the solving techniques they need. Solitaire
will follow on the same framework.

## Layout

```
framework/core/        Shared, game-agnostic C++ (deterministic RNG, undo stack)
framework/qt/          QML module "Calm": app shell, theme, components, sounds,
                       haptics, settings, saving, stats, GameSession base class
games/sudoku/engine/   Pure C++ Sudoku engine: board, solver, generator, grader, hints
games/sudoku/app/      The Sudoku app (SudokuGame session + QML screens)
games/sudoku/tests/    Engine tests and session tests (run by CTest)
tools/sudoku-gen/      Command-line puzzle generator and grader
tools/make_sounds.py   Synthesises the framework's sound set
```

See [docs/FRAMEWORK.md](docs/FRAMEWORK.md) for how games plug into the framework.

## Building

Requires CMake 3.21+ and a C++20 compiler. The engines have no other
dependencies; the apps need Qt 6.4+ (6.8 LTS recommended) with Qt Multimedia.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/games/sudoku/app/CalmSudoku
```

Gwynant Sudoku also runs in the browser (Qt for WebAssembly, hosted on
Cloudflare Pages); see [docs/WEB.md](docs/WEB.md) for the web build and the
one-time Cloudflare setup.

Generate puzzles:

```sh
./build/tools/sudoku-gen/sudoku-gen --level hard --count 5 --seed 42 --pretty
./build/tools/sudoku-gen/sudoku-gen --grade 530070000600195000098000060800060003400803001700020006060000280000419005000080079
```

## How puzzles are made

1. A full grid is filled by a randomised backtracking solver, seeded so any
   puzzle can be recreated from its seed.
2. Cells are removed in symmetric pairs; a removal is undone if the puzzle
   stops having exactly one solution, or becomes harder than the target level.
3. A logical solver grades the result by the hardest technique a person needs:

| Level  | Hardest technique needed                        |
| ------ | ----------------------------------------------- |
| Easy   | Naked and hidden singles (at least 36 givens)   |
| Medium | Locked candidates, naked and hidden pairs       |
| Hard   | Naked and hidden triples, X-Wing                |
| Expert | Swordfish, XY-Wing                              |

The same logical solver powers hints.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE). Qt is used under the LGPLv3. The
Nunito font is under the SIL Open Font License (`framework/qt/fonts/OFL.txt`).
The sounds are synthesised by `tools/make_sounds.py` and are part of this project.
