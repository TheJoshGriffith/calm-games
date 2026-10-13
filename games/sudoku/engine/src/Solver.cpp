#include "sudoku/Solver.h"

#include <calm/Rng.h>

namespace sudoku {

namespace {

// Backtracking search with row/column/box masks and minimum-remaining-values
// cell choice. Fast enough to run thousands of uniqueness checks per second.
class Search {
public:
    explicit Search(const Grid& grid)
        : m_grid(grid)
    {
        for (int i = 0; i < 81; ++i) {
            const int v = m_grid[i];
            if (v == 0)
                continue;
            const Mask b = bit(v);
            if ((m_rows[rowOf(i)] | m_cols[colOf(i)] | m_boxes[boxOf(i)]) & b)
                m_valid = false;
            m_rows[rowOf(i)] |= b;
            m_cols[colOf(i)] |= b;
            m_boxes[boxOf(i)] |= b;
        }
    }

    int count(int limit)
    {
        if (!m_valid)
            return 0;
        m_limit = limit;
        m_found = 0;
        recurse(nullptr);
        return m_found;
    }

    std::optional<Grid> first(calm::Rng* rng)
    {
        if (!m_valid)
            return std::nullopt;
        m_limit = 1;
        m_found = 0;
        recurse(rng);
        if (m_found == 0)
            return std::nullopt;
        return m_solution;
    }

private:
    Mask candidates(int cell) const
    {
        return static_cast<Mask>(AllDigits & ~(m_rows[rowOf(cell)] | m_cols[colOf(cell)] | m_boxes[boxOf(cell)]));
    }

    void recurse(calm::Rng* rng)
    {
        int best = -1;
        int bestCount = 10;
        Mask bestMask = 0;
        for (int i = 0; i < 81; ++i) {
            if (m_grid[i] != 0)
                continue;
            const Mask m = candidates(i);
            const int n = popcount(m);
            if (n == 0)
                return;
            if (n < bestCount) {
                best = i;
                bestCount = n;
                bestMask = m;
                if (n == 1)
                    break;
            }
        }

        if (best < 0) {
            if (++m_found == 1)
                m_solution = m_grid;
            return;
        }

        int digits[9];
        int n = 0;
        for (int d = 1; d <= 9; ++d)
            if (bestMask & bit(d))
                digits[n++] = d;
        if (rng)
            rng->shuffle(digits, digits + n);

        const int r = rowOf(best), c = colOf(best), b = boxOf(best);
        for (int k = 0; k < n && m_found < m_limit; ++k) {
            const int d = digits[k];
            const Mask dm = bit(d);
            m_grid[best] = static_cast<std::uint8_t>(d);
            m_rows[r] |= dm;
            m_cols[c] |= dm;
            m_boxes[b] |= dm;
            recurse(rng);
            m_rows[r] &= static_cast<Mask>(~dm);
            m_cols[c] &= static_cast<Mask>(~dm);
            m_boxes[b] &= static_cast<Mask>(~dm);
            m_grid[best] = 0;
        }
    }

    Grid m_grid;
    Grid m_solution {};
    Mask m_rows[9] {};
    Mask m_cols[9] {};
    Mask m_boxes[9] {};
    bool m_valid = true;
    int m_limit = 2;
    int m_found = 0;
};

} // namespace

int countSolutions(const Grid& grid, int limit)
{
    return Search(grid).count(limit);
}

std::optional<Grid> solve(const Grid& grid)
{
    return Search(grid).first(nullptr);
}

Grid randomSolvedGrid(calm::Rng& rng)
{
    Grid empty {};
    // An empty grid always has a solution, so this cannot fail.
    return *Search(empty).first(&rng);
}

} // namespace sudoku
