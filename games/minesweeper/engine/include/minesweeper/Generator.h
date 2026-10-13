#pragma once

#include <minesweeper/Field.h>

#include <calm/Rng.h>

#include <string_view>

namespace minesweeper {

struct Layout {
    int cols;
    int rows;
    int mines;
};

enum class Difficulty { Easy, Medium, Hard, Expert };

// Sized for a phone held upright.
Layout layoutFor(Difficulty difficulty);
const char* difficultyName(Difficulty difficulty);
bool difficultyFromName(std::string_view name, Difficulty& out);

// Lays mines in `field` for a first tap on `firstCell`. The tapped cell and
// its neighbours are always clear, so the first tap opens an area. With
// `noGuessing`, keeps drawing until the board can be cleared by logic alone
// (giving up after `maxAttempts` and keeping the last, still-safe-start draw).
// Returns true if the layout is guaranteed guess-free.
bool layMines(Field& field, int firstCell, calm::Rng& rng, bool noGuessing, int maxAttempts = 4000);

} // namespace minesweeper
