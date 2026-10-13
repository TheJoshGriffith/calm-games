#pragma once

#include <solitaire/Klondike.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

namespace solitaire {

enum class Verdict { Winnable, Unwinnable, Unknown };

// Depth-first search for a win from `game`, giving up after `nodeBudget`
// positions. "Unwinnable" means the search ran out of moves within budget;
// it skips a few rarely useful moves, so treat it as "probably lost".
Verdict solve(const Klondike& game, int nodeBudget = 150000);

// The same search over every legal move, so "Unwinnable" is a proof: no
// sequence of moves from `game` wins. Slower; "Unknown" when out of budget
// or when `stop` becomes true.
Verdict solveExhaustive(const Klondike& game, int nodeBudget = 400000, const std::atomic_bool* stop = nullptr);

// Every step of a winning line from `game`, if the solver finds one within
// budget.
std::optional<std::vector<Hint>> solvePath(const Klondike& game, int nodeBudget = 60000);

struct Deal {
    Klondike game;
    std::uint64_t seed = 0;
    bool proven = false; // the solver found a win from the opening layout
};

// Deals from `seed`, retrying with derived seeds until the solver proves a
// win (up to `attempts` deals). Falls back to the last deal if none is
// proven, so it always returns a game.
Deal dealWinnable(std::uint64_t seed, int drawCount, int attempts = 40, int nodeBudget = 150000);

} // namespace solitaire
