#include <minesweeper/Field.h>

#include <algorithm>

namespace minesweeper {

Field::Field(int cols, int rows, int mines)
    : m_cols(cols)
    , m_rows(rows)
    , m_mines(mines)
    , m_mine(static_cast<std::size_t>(cols * rows), 0)
    , m_adjacent(static_cast<std::size_t>(cols * rows), 0)
    , m_state(static_cast<std::size_t>(cols * rows), Cell::Hidden)
{
}

Neighbours Field::neighbours(int cell) const
{
    Neighbours n;
    const int r = cell / m_cols;
    const int c = cell % m_cols;
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0)
                continue;
            const int rr = r + dr;
            const int cc = c + dc;
            if (rr >= 0 && rr < m_rows && cc >= 0 && cc < m_cols)
                n.cells[n.count++] = rr * m_cols + cc;
        }
    }
    return n;
}

void Field::placeMines(const std::vector<int>& mineCells)
{
    std::fill(m_mine.begin(), m_mine.end(), 0);
    for (int cell : mineCells)
        m_mine[cell] = 1;
    m_mines = static_cast<int>(std::count(m_mine.begin(), m_mine.end(), 1));
    for (int cell = 0; cell < cellCount(); ++cell) {
        int count = 0;
        for (int n : neighbours(cell))
            count += m_mine[n];
        m_adjacent[cell] = static_cast<std::uint8_t>(count);
    }
    m_placed = true;
}

void Field::clearMines()
{
    std::fill(m_mine.begin(), m_mine.end(), 0);
    std::fill(m_adjacent.begin(), m_adjacent.end(), 0);
    m_placed = false;
}

int Field::flaggedNeighbours(int cell) const
{
    int count = 0;
    for (int n : neighbours(cell))
        count += m_state[n] == Cell::Flagged;
    return count;
}

RevealResult Field::reveal(int cell)
{
    RevealResult result;
    if (m_state[cell] != Cell::Hidden)
        return result;
    if (m_mine[cell]) {
        m_state[cell] = Cell::Revealed;
        result.hitMine = true;
        result.mineCell = cell;
        return result;
    }
    std::vector<int> stack { cell };
    m_state[cell] = Cell::Revealed;
    while (!stack.empty()) {
        const int current = stack.back();
        stack.pop_back();
        ++result.opened;
        if (m_adjacent[current] != 0)
            continue;
        for (int n : neighbours(current)) {
            if (m_state[n] == Cell::Hidden && !m_mine[n]) {
                m_state[n] = Cell::Revealed;
                stack.push_back(n);
            }
        }
    }
    return result;
}

RevealResult Field::chord(int cell)
{
    RevealResult result;
    if (m_state[cell] != Cell::Revealed || m_mine[cell] || m_adjacent[cell] == 0
        || flaggedNeighbours(cell) != m_adjacent[cell])
        return result;
    for (int n : neighbours(cell)) {
        const RevealResult r = reveal(n);
        result.opened += r.opened;
        if (r.hitMine && !result.hitMine) {
            result.hitMine = true;
            result.mineCell = r.mineCell;
        }
    }
    return result;
}

bool Field::toggleFlag(int cell)
{
    if (m_state[cell] == Cell::Revealed)
        return false;
    m_state[cell] = m_state[cell] == Cell::Flagged ? Cell::Hidden : Cell::Flagged;
    return true;
}

int Field::flagAround(int cell)
{
    if (m_state[cell] != Cell::Revealed || m_mine[cell] || m_adjacent[cell] == 0)
        return 0;
    int hidden = 0;
    for (int n : neighbours(cell))
        hidden += m_state[n] == Cell::Hidden;
    if (hidden == 0 || hidden + flaggedNeighbours(cell) != m_adjacent[cell])
        return 0;
    for (int n : neighbours(cell))
        if (m_state[n] == Cell::Hidden)
            m_state[n] = Cell::Flagged;
    return hidden;
}

int Field::flagCount() const
{
    return static_cast<int>(std::count(m_state.begin(), m_state.end(), Cell::Flagged));
}

int Field::revealedCount() const
{
    int count = 0;
    for (int cell = 0; cell < cellCount(); ++cell)
        count += m_state[cell] == Cell::Revealed && !m_mine[cell];
    return count;
}

} // namespace minesweeper
