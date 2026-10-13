#pragma once

#include <minesweeper/Field.h>

namespace minesweeper {

// Plays a laid-out field the way a careful person would, using only what the
// numbers prove: single-number rules, overlapping pairs of numbers, and the
// total mine count. Returns true if every safe cell can be opened from
// `start` without ever guessing.
bool solvableWithoutGuessing(const Field& layout, int start);

// Cells whose content the visible numbers prove: safe cells to open and
// mines to flag. Flags and revealed cells in `field` are taken as known.
// Used for hints.
struct Deductions {
    std::vector<int> safe;
    std::vector<int> mines;
};
Deductions deduce(const Field& field);

} // namespace minesweeper
