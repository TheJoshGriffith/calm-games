#pragma once

#include "sudoku/Board.h"
#include "sudoku/Grader.h"

#include <cstdint>
#include <optional>

namespace sudoku {

struct Puzzle {
    Grid puzzle {};
    Grid solution {};
    std::uint64_t seed = 0;
    Difficulty difficulty = Difficulty::Easy;
    Technique hardest = Technique::None;
    int clues = 0;
};

struct GeneratorOptions {
    int maxAttempts = 3000;
    bool symmetric = true; // remove cells in 180-degree rotational pairs
};

// Version of generate()'s output, embedded in puzzle codes (see Share.h).
// Anything that changes which puzzle a (difficulty, seed) produces -- the
// RNG, randomSolvedGrid, removal order, grader rules, clue floors, default
// options -- MUST bump this, or old codes will silently open different
// puzzles. The golden test in engine_tests.cpp guards against accidents.
inline constexpr int GeneratorVersion = 1;

// Generates a puzzle with exactly one solution whose logical grade matches
// `difficulty`. Deterministic: the same (difficulty, seed) always returns the
// same puzzle. Returns nullopt only if no attempt hit the target grade.
std::optional<Puzzle> generate(Difficulty difficulty, std::uint64_t seed, const GeneratorOptions& options = {});

// Clue floor per level: Easy keeps plenty of givens so it stays relaxed.
int minimumClues(Difficulty difficulty) noexcept;

} // namespace sudoku
