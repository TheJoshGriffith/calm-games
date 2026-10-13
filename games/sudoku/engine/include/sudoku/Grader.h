#pragma once

#include "sudoku/Board.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace sudoku {

// Human solving techniques, ordered from easiest to hardest.
enum class Technique : std::uint8_t {
    None,
    NakedSingle,
    HiddenSingle,
    LockedCandidates, // pointing and claiming
    NakedPair,
    HiddenPair,
    NakedTriple,
    HiddenTriple,
    XWing,
    Swordfish,
    XYWing,
};
inline constexpr int TechniqueCount = static_cast<int>(Technique::XYWing) + 1;

enum class Difficulty : std::uint8_t { Easy, Medium, Hard, Expert };
inline constexpr int DifficultyCount = 4;

std::string_view techniqueName(Technique technique) noexcept;
std::string_view difficultyName(Difficulty difficulty) noexcept;
std::optional<Difficulty> parseDifficulty(std::string_view name) noexcept;

// The difficulty a puzzle earns when `hardest` is the hardest technique it needs.
Difficulty difficultyFor(Technique hardest) noexcept;

struct GradeResult {
    bool solved = false; // false: logic alone got stuck (harder than Expert)
    Technique hardest = Technique::None;
    std::array<int, TechniqueCount> uses {};
    Grid grid {}; // the grid as far as logic got

    std::optional<Difficulty> difficulty() const noexcept
    {
        if (!solved)
            return std::nullopt;
        return difficultyFor(hardest);
    }
};

// Solves the puzzle the way a person would, cheapest technique first, and
// records which techniques were needed.
GradeResult grade(const Grid& puzzle);

struct Hint {
    Technique technique = Technique::None; // hardest technique needed to reach it
    int cell = -1;
    int digit = 0;
};

// The next cell a person could fill by logic from this position, and the
// technique that unlocks it. nullopt if logic finds no placement (including
// when the position already contains a mistake that causes a contradiction).
std::optional<Hint> hint(const Grid& position);

} // namespace sudoku
