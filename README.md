# Calm Games

Free, ad-free, open-source puzzle games for iOS and Android, built with Qt 6.
No ads, no in-app purchases, no accounts, no tracking.

The first game is **Sudoku**: endless generated puzzles, each with exactly one
solution, graded Easy to Expert by the solving techniques they need. Solitaire
will follow on the same framework.

## Layout

```
framework/core/        Shared, game-agnostic C++ (deterministic RNG, undo stack)
games/sudoku/engine/   Pure C++ Sudoku engine: board, solver, generator, grader, hints
games/sudoku/tests/    Engine tests (run by CTest)
tools/sudoku-gen/      Command-line puzzle generator and grader
```

The Qt/QML app shell, theming, sounds and saving arrive in the next milestone.

## Building

Requires CMake 3.21+ and a C++20 compiler. The engine has no other dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

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

GPL-3.0-or-later. See [LICENSE](LICENSE). Qt is used under the LGPLv3.
