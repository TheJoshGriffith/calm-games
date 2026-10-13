#pragma once

#include "sudoku/Board.h"
#include "sudoku/Grader.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sudoku {

// Everything a share link carries: the puzzle plus the player's progress.
// Plain data so the app can map its own board model onto it.
struct SharedBoard {
    Grid givens {};
    Grid entries {}; // player digits, 0 = none; never on a given cell
    std::array<Mask, 81> corner {}; // corner (pencil) marks, bits 1..9
    std::array<Mask, 81> centre {}; // centre marks, bits 1..9
    std::array<std::uint8_t, 81> colour {}; // 0 = none, 1..9 = palette index
    std::optional<Difficulty> level;

    bool operator==(const SharedBoard&) const = default;
};

// Share-link payload (the <payload> in https://<domain>/s/<payload>).
//
// Format version 1: the character '1', then a bit stream written MSB-first
// into base64url characters (A-Z a-z 0-9 - _), zero-padded, no '=':
//
//   header   3 bits level (0 = none, 1..4 = Easy..Expert)
//            1 bit  symmetric givens (180-degree rotation)
//            4 bits sections present: entries, corner, centre, colour
//   givens   presence bitmap: 41 bits (cells 0..40) if symmetric, else 81
//            then the given digits, packed
//   entries  presence bitmap over the non-given cells, then digits, packed
//   corner   presence bitmap over the non-given cells, then 9 bits per mask
//   centre   same as corner
//   colour   presence bitmap over all 81 cells, then colours, packed
//
// "Packed" digits (1..9, stored as 0..8) go three to 10 bits (9^3 = 729),
// with a final pair in 7 bits or a final single in 4 bits.
//
// A section is written only when it is non-empty, so a starting board is
// just the header and givens (about 20-30 characters).
//
// encodeBoard normalises what it cannot represent: entries and marks on
// given cells, digits/colours outside 1..9 and mask bits outside 1..9 are
// dropped. decodeBoard is strict: it rejects anything encodeBoard would not
// have produced (unknown version, bad characters, truncation, trailing data,
// out-of-range values, empty sections, inconsistent givens).
inline constexpr char ShareFormatVersion = '1';

std::string encodeBoard(const SharedBoard& board);
std::optional<SharedBoard> decodeBoard(std::string_view payload);

// Puzzle codes people can type: level letter (E, M, H, X), generator
// version digit, a dash, then the seed in Crockford base32, e.g. "H1-3KX9QZ0A".
//
// New puzzles should draw seeds below 2^PuzzleSeedBits, giving 8-character
// seeds. Larger 64-bit seeds still work and produce 13 characters.
inline constexpr int PuzzleSeedBits = 40;
inline constexpr std::uint64_t PuzzleSeedMask = (std::uint64_t { 1 } << PuzzleSeedBits) - 1;

std::string puzzleCode(Difficulty difficulty, std::uint64_t seed);

// Accepts lowercase, ignores surrounding whitespace and extra dashes in the
// seed, and reads I/L as 1 and O as 0. Accepts 8- or 13-character seeds.
// Rejects codes from another generator version, since they would not
// reproduce the same puzzle.
std::optional<std::pair<Difficulty, std::uint64_t>> parsePuzzleCode(std::string_view code);

} // namespace sudoku
