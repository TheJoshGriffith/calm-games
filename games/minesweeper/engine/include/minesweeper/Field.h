#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace minesweeper {

enum class Cell : std::uint8_t { Hidden, Revealed, Flagged };

// Up to eight neighbouring cell indices.
struct Neighbours {
    std::array<int, 8> cells {};
    int count = 0;
    const int* begin() const { return cells.data(); }
    const int* end() const { return cells.data() + count; }
};

struct RevealResult {
    int opened = 0;       // cells newly revealed
    bool hitMine = false; // a mine was revealed
    int mineCell = -1;    // which one
};

// A rectangular minefield: where the mines are and what the player has
// revealed or flagged. Cells are indexed row-major, index = row * cols + col.
// Mines are placed separately (see Generator) so the first reveal is safe.
class Field {
public:
    Field() = default;
    Field(int cols, int rows, int mines);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    int cellCount() const { return m_cols * m_rows; }
    int mineCount() const { return m_mines; }
    bool minesPlaced() const { return m_placed; }

    // Sets the mine layout; `mineCells` must hold mineCount() distinct cells.
    void placeMines(const std::vector<int>& mineCells);
    void clearMines();

    bool isMine(int cell) const { return m_mine[cell] != 0; }
    int adjacentMines(int cell) const { return m_adjacent[cell]; }
    Cell state(int cell) const { return m_state[cell]; }
    Neighbours neighbours(int cell) const;
    int flaggedNeighbours(int cell) const;

    // Reveals a hidden, unflagged cell; empty cells open their neighbours.
    RevealResult reveal(int cell);
    // On a revealed number whose flags match it, reveals the other neighbours.
    RevealResult chord(int cell);
    // Flags or unflags a hidden cell. Returns false if the cell is revealed.
    bool toggleFlag(int cell);
    // Shows every mine (after a loss) without counting them as safe reveals.
    void setState(int cell, Cell state) { m_state[cell] = state; }

    int flagCount() const;
    int revealedCount() const;
    bool cleared() const { return m_placed && revealedCount() == cellCount() - m_mines; }

    const std::vector<std::uint8_t>& mines() const { return m_mine; }
    const std::vector<Cell>& states() const { return m_state; }
    void setStates(const std::vector<Cell>& states) { m_state = states; }

private:
    int m_cols = 0;
    int m_rows = 0;
    int m_mines = 0;
    bool m_placed = false;
    std::vector<std::uint8_t> m_mine;
    std::vector<std::uint8_t> m_adjacent;
    std::vector<Cell> m_state;
};

} // namespace minesweeper
