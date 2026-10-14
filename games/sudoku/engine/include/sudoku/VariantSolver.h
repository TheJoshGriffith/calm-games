#pragma once

#include "sudoku/Variant.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace calm {
class Rng;
}

namespace sudoku {

// A variant's rules compiled for solving: candidate masks narrowed by
// constraint propagation (singles, locked candidates and each clue's own
// reasoning), with backtracking on top for counting solutions.
class VariantModel {
public:
    explicit VariantModel(const Rules& rules);
    ~VariantModel();
    VariantModel(const VariantModel&) = delete;
    VariantModel& operator=(const VariantModel&) = delete;

    const Rules& rules() const noexcept;
    // Cells that must hold a different digit from `cell`.
    const std::vector<int>& peers(int cell) const noexcept;
    bool arePeers(int a, int b) const noexcept;
    // The all-different units: rows, columns, regions, and any diagonals or
    // extra boxes.
    const std::vector<std::array<int, 9>>& units() const noexcept;
    // Digits a cell may hold on its own (Even-Odd shading).
    Mask allowed(int cell) const noexcept;

    // Counts solutions, stopping at `limit`.
    int countSolutions(const Grid& givens, int limit = 2) const;
    std::optional<Grid> solve(const Grid& givens) const;
    // A random full grid obeying the global rules; nullopt if the search
    // gave up after `nodeLimit` guesses.
    std::optional<Grid> randomSolution(calm::Rng& rng, int nodeLimit = 20000) const;

    struct Logic {
        bool solved = false; // reached the solution without guessing
        bool contradiction = false;
        Grid grid {}; // digits fixed so far
        std::array<Mask, 81> candidates {};
    };
    // Solves by reasoning alone. A solved result is the only solution.
    Logic logicSolve(const Grid& givens) const;

    // An empty cell of `position` that reasoning can fill, and its digit.
    // Simple steps (a digit with one place left, a cell with one digit left)
    // are preferred. nullopt if reasoning finds nothing or a contradiction.
    struct HintStep {
        int cell = -1;
        int digit = 0;
    };
    std::optional<HintStep> hint(const Grid& position) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace sudoku
