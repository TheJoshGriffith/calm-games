#pragma once

#include <solitaire/Game.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace solitaire {

enum class Verdict { Winnable, Unwinnable, Unknown };

// Depth-first search for a win from `game`, giving up after `nodeBudget`
// positions. "Unwinnable" means the search ran out of moves within budget;
// it skips a few rarely useful moves, so treat it as "probably lost".
Verdict solve(const Game& game, int nodeBudget = 150000);

// The same search over every legal move, so "Unwinnable" is a proof: no
// sequence of moves from `game` wins. Slower; "Unknown" when out of budget
// or when `stop` becomes true.
Verdict solveExhaustive(const Game& game, int nodeBudget = 400000, const std::atomic_bool* stop = nullptr);

// solveExhaustive (or solve, when not `exhaustive`) a slice at a time, for a
// single thread that must keep drawing frames in between.
class SteppedSolve {
public:
    explicit SteppedSolve(const Game& game, int nodeBudget = 400000, bool exhaustive = true);
    ~SteppedSolve();
    SteppedSolve(SteppedSolve&&) noexcept;
    SteppedSolve& operator=(SteppedSolve&&) noexcept;

    // Searches up to `nodes` more positions. The verdict once there is one,
    // which then stays the same; nothing while the search goes on.
    std::optional<Verdict> step(int nodes);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Every step of a winning line from `game`, if the solver finds one within
// budget.
std::optional<std::vector<Hint>> solvePath(const Game& game, int nodeBudget = 60000);

struct Deal {
    Game game;
    std::uint64_t seed = 0;
    bool proven = false; // the solver found a win from the opening layout
};

// Deals from `seed`, retrying with derived seeds until the solver proves a
// win (up to `attempts` deals). Falls back to the last deal if none is
// proven, so it always returns a game.
// Spider is dealt as it comes: its search space is too large to prove.
Deal dealWinnable(Variant variant, std::uint64_t seed, int option = 0, int attempts = 40, int nodeBudget = 150000);

// dealWinnable a slice at a time, for a single thread.
class SteppedDeal {
public:
    SteppedDeal(Variant variant, std::uint64_t seed, int option = 0, int attempts = 40, int nodeBudget = 150000);

    // Searches up to `nodes` more positions. The deal once it is settled.
    std::optional<Deal> step(int nodes);

private:
    Variant m_variant;
    int m_option;
    int m_attempts;
    int m_budget;
    int m_attempt = 0;
    std::uint64_t m_seed;
    std::uint64_t m_next;
    Deal m_deal;
    std::optional<SteppedSolve> m_search;
};

} // namespace solitaire
