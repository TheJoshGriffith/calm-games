#pragma once

#include <mahjong/Board.h>

#include <utility>
#include <vector>

namespace mahjong {

struct Solution {
    enum Outcome { Solved, Unsolvable, Unknown };
    Outcome outcome = Unknown;
    // For Solved: the pairs to take, in order, to clear the table.
    std::vector<std::pair<int, int>> moves;
};

// Searches for a way to clear the tiles still on the table, giving up with
// Unknown after `budget` positions. Moves that can never hurt (the last two
// tiles of a group, or a group whose four tiles are all free) are played
// without branching, which keeps real deals quick to solve.
Solution solve(const Board& board, int budget = 100000);

} // namespace mahjong
