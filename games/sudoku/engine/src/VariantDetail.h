#pragma once

// Shared between Variant.cpp and VariantSolver.cpp: the rules turned into
// all-different units, peer lists and pairwise relations.

#include "sudoku/Variant.h"

#include <array>
#include <vector>

namespace sudoku::detail {

enum class Relation : std::uint8_t {
    Less, // a < b
    Whisper, // |a - b| >= 5
    Equal,
    Consecutive, // |a - b| == 1
    Double, // a == 2b or b == 2a
    NoKropki, // neither consecutive nor double
    Ten, // a + b == 10
    Five, // a + b == 5
    NoXV, // a + b is neither 5 nor 10
    NotConsecutive, // |a - b| != 1
};

bool holds(Relation relation, int a, int b) noexcept;

struct Edge {
    int a = 0;
    int b = 0;
    Relation relation = Relation::Less;
};

// Rows, columns, regions, then diagonals and windoku boxes when on.
std::vector<std::array<int, 9>> unitsOf(const Rules& rules);
// For each cell, every other cell that must hold a different digit: unit
// mates, knight or king moves, and cells sharing a cage or renban line.
std::array<std::vector<int>, 81> peersOf(const Rules& rules);
// Pairwise rules: thermo steps, whispers, palindromes, dots, non-consecutive.
std::vector<Edge> edgesOf(const Rules& rules);
// Orthogonal neighbour pairs (a < b), row pairs first.
const std::vector<std::pair<int, int>>& orthogonalPairs();
bool validCell(int cell) noexcept;
bool kingAdjacent(int a, int b) noexcept;

} // namespace sudoku::detail
