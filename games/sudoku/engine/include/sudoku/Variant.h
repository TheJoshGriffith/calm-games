#pragma once

#include "sudoku/Board.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sudoku {

// Variant Sudoku: the classic rules plus extra global rules (diagonals,
// anti-knight, irregular regions...) and clues drawn on the grid (cages,
// lines, dots...). Classic games keep using Generator/Grader; variants are
// described by Rules and solved by VariantModel (VariantSolver.h).
enum class Variant : std::uint8_t {
    Classic,
    // Global rules
    Diagonal,
    AntiKnight,
    AntiKing,
    Windoku,
    NonConsecutive,
    Jigsaw,
    // Clue shapes
    Killer,
    Thermo,
    Arrow,
    Sandwich,
    Kropki,
    XV,
    EvenOdd,
    LittleKiller,
    Whispers,
    Renban,
    Palindrome,
};
inline constexpr int VariantCount = static_cast<int>(Variant::Palindrome) + 1;

// English display name, also the level id the app records stats under
// ("Killer", "Anti-Knight").
std::string_view variantName(Variant variant) noexcept;
// Stable lower-case id for saves ("killer", "anti-knight").
std::string_view variantId(Variant variant) noexcept;
// Two capital letters for puzzle codes ("KL"); empty for Classic.
std::string_view variantCode(Variant variant) noexcept;
// Accepts a name, an id or a code, ignoring case.
std::optional<Variant> parseVariant(std::string_view text) noexcept;

enum class DotKind : std::uint8_t {
    White, // Kropki: the digits are consecutive
    Black, // Kropki: one digit is double the other
    X, // XV: the digits sum to 10
    V, // XV: the digits sum to 5
};

// A dot or letter on the edge between two orthogonally adjacent cells (a < b).
struct Dot {
    int a = 0;
    int b = 0;
    DotKind kind = DotKind::White;
    bool operator==(const Dot&) const = default;
};

// Killer cage: distinct digits summing to `sum`.
struct Cage {
    std::vector<int> cells;
    int sum = 0;
    bool operator==(const Cage&) const = default;
};

// The digits along the path sum to the digit in the circle.
struct ArrowClue {
    int circle = 0;
    std::vector<int> path; // starts next to the circle
    bool operator==(const ArrowClue&) const = default;
};

// A sum outside the grid for the diagonal it points along; digits may repeat
// unless they share a row, column or region.
struct LittleKillerClue {
    int row = 0; // outside the grid: -1 or 9 on at least one axis
    int col = 0;
    int dr = 1; // direction into the grid, +-1 each
    int dc = 1;
    int sum = 0;
    bool operator==(const LittleKillerClue&) const = default;
};
// The cells a little killer clue covers, from its first cell inwards.
std::vector<int> littleKillerCells(const LittleKillerClue& clue);

// Everything that defines a variant puzzle besides its givens.
struct Rules {
    Variant variant = Variant::Classic;
    // Region (box) of each cell, 0..8; the 3x3 boxes unless Jigsaw.
    std::array<std::uint8_t, 81> region {};
    bool diagonals = false; // both main diagonals hold 1-9
    bool antiKnight = false; // a knight's move apart: never the same digit
    bool antiKing = false; // touching diagonally: never the same digit
    bool windoku = false; // four extra 3x3 boxes hold 1-9
    bool nonConsecutive = false; // orthogonal neighbours never consecutive
    std::vector<Cage> cages;
    std::vector<std::vector<int>> thermos; // bulb first, increasing
    std::vector<ArrowClue> arrows;
    std::array<std::int8_t, 9> rowSandwich {}; // -1 = no clue
    std::array<std::int8_t, 9> colSandwich {};
    std::vector<Dot> dots;
    // Every dot is shown: a pair without one has neither relation.
    bool dotsComplete = false;
    std::array<std::uint8_t, 81> parity {}; // 0 none, 1 even, 2 odd
    std::vector<LittleKillerClue> littleKillers;
    std::vector<std::vector<int>> whispers; // neighbours differ by 5 or more
    std::vector<std::vector<int>> renbans; // a set of consecutive digits
    std::vector<std::vector<int>> palindromes; // reads the same both ways

    bool operator==(const Rules&) const = default;
};

// The 3x3 boxes as regions.
std::array<std::uint8_t, 81> standardRegions() noexcept;
// The rules a variant starts from before any clues are added.
Rules baseRules(Variant variant);
// True when the regions are nine connected groups of nine cells.
bool validRegions(const std::array<std::uint8_t, 81>& region);

// True if `grid` is full and obeys every rule.
bool satisfies(const Rules& rules, const Grid& grid);
// Cells that break a rule given the digits placed so far (0 = empty): a
// repeated digit, a broken line or dot, a cage or sum that can no longer add
// up. Only filled cells are flagged.
std::array<bool, 81> conflicts(const Rules& rules, const Grid& grid);

// A compact text form for saves. decodeRules rejects anything malformed.
std::string encodeRules(const Rules& rules);
std::optional<Rules> decodeRules(std::string_view text);

} // namespace sudoku
