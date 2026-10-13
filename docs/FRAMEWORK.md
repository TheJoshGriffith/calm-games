# The Calm framework

Every Calm game is a QML app built on two shared layers:

- `framework/core` (pure C++, target `Calm::Core`): deterministic `calm::Rng`
  and snapshot-based `calm::UndoStack`. No Qt, so game engines stay testable
  anywhere.
- `framework/qt` (QML module `Calm`, targets `calm_framework` and
  `calm_frameworkplugin`): the app shell, theme, components, sounds, haptics,
  settings, saving and statistics, plus the `GameSession` base class.

## Writing a game

A game lives entirely in `games/<name>/`. The root `CMakeLists.txt` picks up any
folder there that has a `CMakeLists.txt`, so adding a game touches nothing else.

Recommended layout, mirroring `games/sudoku/`:

```
games/<name>/
  CMakeLists.txt     add_subdirectory(engine), app (if CALM_BUILD_APPS), tests
  engine/            pure C++ rules: deal/generate, legal moves, win check
  app/               <Name>Game : GameSession, main.cpp, qml/
  tests/             engine tests (plain C++) and session tests (Qt Test)
```

### The GameSession contract

Subclass `GameSession` (`framework/qt/src/GameSession.h`) and implement:

| Method | Do this |
| --- | --- |
| `gameId()` | A stable id, e.g. `"solitaire"`. Used for saves and stats. |
| `createGame(level)` | Build or start generating a game, then call `beginGame()`. |
| `saveState()` / `restoreState()` | The whole game as a `QVariantMap`, for autosave and resume. |
| `snapshot()` / `applySnapshot()` | A cheap copy of the mutable state, for undo and redo. |

Rules:

- Wrap every player move in `beginMove()` ... `endMove()`. That gives undo,
  autosave and the `stateChanged` signal for free.
- Call `finishGame()` when the player wins. It stops the clock, records the
  win in `Stats`, clears the save and plays the win sound.
- Use `emitFeedback(event, step)` for sounds and haptics with the shared
  vocabulary: `tap`, `soft`, `erase`, `error`, `complete` (step = how many
  completions so far; each one chimes higher), `win`. Don't add sound files in
  a game; ask for a new framework event instead.
- Game-specific options are properties on your session class, stored in
  `QSettings` under `<gameId>/`. App-wide options (theme, sound, haptics,
  reduce motion) are in the `Settings` singleton.

### UI

Start `Main.qml` with `AppWindow { session: yourGame; initialItem: ... }`. It
provides the page stack, transitions, Android back handling, pausing in the
background, and the wiring from `feedback` to `SoundBank` and `Haptics`.
Every feedback event therefore plays a sound and a haptic at the player's
chosen strength (Settings: Haptics on/off, Subtle/Gentle/Firm); games add
nothing for haptics beyond calling `emitFeedback`.

Use the framework components instead of raw controls so every game looks and
feels the same: `CalmPage`, `CalmText`, `CardButton`, `IconButton`,
`ActionButton`, `SegmentedControl`, `ToggleRow`, `SettingsGroup`,
`SettingsPage` (children become your game's settings groups), `BottomSheet`,
`CalmTextField`, and `Icon` with `Icons.*`. `AppWindow.showNote(text)` shows
a short note that fades away. To share a link, call
`Sharing.share(text, anchorRect)`: the share sheet on iOS and in browsers
that have one, the clipboard elsewhere (the window then says "Link copied"). All colours, radii and durations come from `Theme`; never hard-code
them, and respect `Theme.quick`/`gentle`/`slow` so Reduce motion works.

## Changing the framework

Games are developed in parallel, so `framework/` has a single owner at a time.
Game work touches only `games/<name>/`. If a game needs something new from the
framework (a component, a sound event, a Theme token), request it rather than
editing `framework/` from a game branch. Keep framework changes additive so
existing games keep building.
