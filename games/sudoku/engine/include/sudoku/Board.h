#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sudoku {

// A 9x9 grid in row-major order; 0 means empty, 1-9 are digits.
using Grid = std::array<std::uint8_t, 81>;

// Candidate sets use bits 1..9 (bit d set = digit d possible).
using Mask = std::uint16_t;
inline constexpr Mask AllDigits = 0x3FE;

constexpr int rowOf(int cell) noexcept { return cell / 9; }
constexpr int colOf(int cell) noexcept { return cell % 9; }
constexpr int boxOf(int cell) noexcept { return (cell / 27) * 3 + (cell % 9) / 3; }
constexpr Mask bit(int digit) noexcept { return static_cast<Mask>(1u << digit); }

// The 27 units: indices 0-8 are rows, 9-17 columns, 18-26 boxes.
const std::array<std::array<int, 9>, 27>& units();
// The 20 cells sharing a row, column or box with each cell.
const std::array<std::array<int, 20>, 81>& peers();
bool arePeers(int a, int b) noexcept;

// Accepts 81 characters of 1-9 for digits and 0 or '.' for empty cells.
// Whitespace is ignored. Returns nullopt for anything else.
std::optional<Grid> parseGrid(std::string_view text);
// 81 characters, '0' for empty cells.
std::string formatGrid(const Grid& grid);
// Multi-line human-readable rendering with box separators.
std::string prettyGrid(const Grid& grid);

int clueCount(const Grid& grid) noexcept;
// True if no unit contains a repeated digit (empty cells allowed).
bool isConsistent(const Grid& grid) noexcept;
// True if the grid is full and consistent.
bool isSolved(const Grid& grid) noexcept;

int popcount(Mask mask) noexcept;
// Lowest digit present in the mask (mask must be non-zero).
int firstDigit(Mask mask) noexcept;

} // namespace sudoku
