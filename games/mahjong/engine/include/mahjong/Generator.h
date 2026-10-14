#pragma once

#include <mahjong/Board.h>

#include <calm/Rng.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace mahjong {

// The pairs to take, in order, to clear a deal (the order it was built in).
using Plan = std::vector<std::pair<int, int>>;

// Deals tiles onto every slot of `board` (all slots back on the table) so
// the game can always be cleared: it builds the deal by taking random free
// pairs off a full table, so playing those pairs in that order wins. The same
// layout and seed always give the same deal. Layouts larger than 144 tiles,
// or with an odd number of slots, can't be dealt and return false.
bool deal(Board& board, std::uint64_t seed, Plan* plan = nullptr);

// Rearranges the tiles still on the table, keeping the same faces, so the
// rest can still be cleared. Returns false (and changes nothing) if the
// remaining shape can't be cleared at all (say, two tiles stacked up).
bool reshuffle(Board& board, calm::Rng& rng, Plan* plan = nullptr);

// Puts `pairs` onto the tiles on the table (one pair per two tiles), in an
// order that can be cleared. Used by deal() and reshuffle().
bool arrange(Board& board, std::vector<std::pair<int, int>> pairs, calm::Rng& rng, Plan* plan = nullptr, int attempts = 400);

// Whether playing `plan` from the board as it is now clears it.
bool planWorks(Board board, const Plan& plan);

} // namespace mahjong
