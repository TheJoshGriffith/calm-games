#pragma once

#include "sudoku/Variant.h"

#include <cstdint>
#include <optional>

namespace sudoku {

struct VariantPuzzle {
    Rules rules;
    Grid puzzle {};
    Grid solution {};
    std::uint64_t seed = 0;
    int clues = 0; // givens left on the grid
};

// Version of generateVariant()'s output, embedded in variant puzzle codes and
// share links, which rebuild the puzzle from (variant, seed). Anything that
// changes which puzzle a seed produces MUST bump this; the golden test in
// variant_tests.cpp guards against accidents.
inline constexpr int VariantGeneratorVersion = 1;

// Generates a variant puzzle: a solved grid under the variant's global rules,
// clues read off that solution, then givens removed while reasoning alone
// (no guessing) still solves it, which also proves the solution unique.
// Deterministic: the same (variant, seed) always gives the same puzzle.
// Returns nullopt for Classic (use generate()) or if every attempt failed.
std::optional<VariantPuzzle> generateVariant(Variant variant, std::uint64_t seed, int maxAttempts = 40);

} // namespace sudoku
