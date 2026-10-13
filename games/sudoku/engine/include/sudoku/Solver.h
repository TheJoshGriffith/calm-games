#pragma once

#include "sudoku/Board.h"

#include <optional>

namespace calm {
class Rng;
}

namespace sudoku {

// Counts solutions by backtracking, stopping once `limit` are found.
// countSolutions(grid, 2) == 1 is the uniqueness check used by the generator.
int countSolutions(const Grid& grid, int limit = 2);

// Returns the first solution found, or nullopt if there is none.
std::optional<Grid> solve(const Grid& grid);

// A uniformly shuffled, completely filled valid grid.
Grid randomSolvedGrid(calm::Rng& rng);

} // namespace sudoku
