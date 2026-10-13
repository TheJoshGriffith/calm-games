#include "sudoku/Board.h"

#include <bit>

namespace sudoku {

namespace {

std::array<std::array<int, 9>, 27> buildUnits()
{
    std::array<std::array<int, 9>, 27> result {};
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            result[i][j] = i * 9 + j;
            result[9 + i][j] = j * 9 + i;
            const int boxRow = (i / 3) * 3 + j / 3;
            const int boxCol = (i % 3) * 3 + j % 3;
            result[18 + i][j] = boxRow * 9 + boxCol;
        }
    }
    return result;
}

std::array<std::array<int, 20>, 81> buildPeers()
{
    std::array<std::array<int, 20>, 81> result {};
    for (int cell = 0; cell < 81; ++cell) {
        int n = 0;
        for (int other = 0; other < 81; ++other) {
            if (other == cell)
                continue;
            if (rowOf(other) == rowOf(cell) || colOf(other) == colOf(cell) || boxOf(other) == boxOf(cell))
                result[cell][n++] = other;
        }
    }
    return result;
}

} // namespace

const std::array<std::array<int, 9>, 27>& units()
{
    static const auto table = buildUnits();
    return table;
}

const std::array<std::array<int, 20>, 81>& peers()
{
    static const auto table = buildPeers();
    return table;
}

bool arePeers(int a, int b) noexcept
{
    return a != b && (rowOf(a) == rowOf(b) || colOf(a) == colOf(b) || boxOf(a) == boxOf(b));
}

std::optional<Grid> parseGrid(std::string_view text)
{
    Grid grid {};
    int n = 0;
    for (char c : text) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t')
            continue;
        if (n >= 81)
            return std::nullopt;
        if (c == '.' || c == '0')
            grid[n++] = 0;
        else if (c >= '1' && c <= '9')
            grid[n++] = static_cast<std::uint8_t>(c - '0');
        else
            return std::nullopt;
    }
    if (n != 81)
        return std::nullopt;
    return grid;
}

std::string formatGrid(const Grid& grid)
{
    std::string out(81, '0');
    for (int i = 0; i < 81; ++i)
        out[i] = static_cast<char>('0' + grid[i]);
    return out;
}

std::string prettyGrid(const Grid& grid)
{
    std::string out;
    for (int r = 0; r < 9; ++r) {
        if (r == 3 || r == 6)
            out += "------+-------+------\n";
        for (int c = 0; c < 9; ++c) {
            if (c == 3 || c == 6)
                out += "| ";
            const auto v = grid[r * 9 + c];
            out += v ? static_cast<char>('0' + v) : '.';
            out += c == 8 ? '\n' : ' ';
        }
    }
    return out;
}

int clueCount(const Grid& grid) noexcept
{
    int n = 0;
    for (auto v : grid)
        n += v != 0;
    return n;
}

bool isConsistent(const Grid& grid) noexcept
{
    for (const auto& unit : units()) {
        Mask seen = 0;
        for (int cell : unit) {
            const int v = grid[cell];
            if (v == 0)
                continue;
            if (v > 9 || (seen & bit(v)))
                return false;
            seen |= bit(v);
        }
    }
    return true;
}

bool isSolved(const Grid& grid) noexcept
{
    return clueCount(grid) == 81 && isConsistent(grid);
}

int popcount(Mask mask) noexcept
{
    return std::popcount(static_cast<unsigned>(mask));
}

int firstDigit(Mask mask) noexcept
{
    return std::countr_zero(static_cast<unsigned>(mask));
}

} // namespace sudoku
