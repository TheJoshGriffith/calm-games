#include <minesweeper/Solver.h>

#include <algorithm>

namespace minesweeper {

namespace {

// "Exactly `mines` of these unknown cells are mines."
struct Constraint {
    std::vector<int> cells; // sorted
    int mines = 0;
};

// Each cell is unknown, known safe (revealed) or known mine. Works on a copy
// of the player's knowledge so the solver never peeks at hidden mines.
struct Knowledge {
    const Field& field;
    std::vector<std::int8_t> known; // 0 unknown, 1 safe, 2 mine

    explicit Knowledge(const Field& f)
        : field(f)
        , known(static_cast<std::size_t>(f.cellCount()), 0)
    {
        for (int cell = 0; cell < f.cellCount(); ++cell) {
            if (f.state(cell) == Cell::Revealed)
                known[cell] = 1;
            else if (f.state(cell) == Cell::Flagged)
                known[cell] = 2;
        }
    }

    std::vector<Constraint> constraints() const
    {
        std::vector<Constraint> out;
        for (int cell = 0; cell < field.cellCount(); ++cell) {
            if (known[cell] != 1 || field.isMine(cell))
                continue;
            Constraint c;
            c.mines = field.adjacentMines(cell);
            for (int n : field.neighbours(cell)) {
                if (known[n] == 0)
                    c.cells.push_back(n);
                else if (known[n] == 2)
                    --c.mines;
            }
            if (!c.cells.empty())
                out.push_back(std::move(c));
        }
        return out;
    }
};

bool isSubset(const std::vector<int>& a, const std::vector<int>& b)
{
    return std::includes(b.begin(), b.end(), a.begin(), a.end());
}

// One round of deduction. Appends what it proves; returns false if nothing.
bool deduceRound(const Knowledge& k, Deductions& out)
{
    const std::vector<Constraint> cs = k.constraints();
    std::vector<std::int8_t> found(k.known.size(), 0);
    auto mark = [&](const std::vector<int>& cells, std::int8_t what) {
        for (int c : cells) {
            if (found[c] == 0) {
                found[c] = what;
                (what == 1 ? out.safe : out.mines).push_back(c);
            }
        }
    };

    for (const Constraint& c : cs) {
        if (c.mines == 0)
            mark(c.cells, 1);
        else if (c.mines == static_cast<int>(c.cells.size()))
            mark(c.cells, 2);
    }
    if (!out.safe.empty() || !out.mines.empty())
        return true;

    // Overlapping numbers: if A's cells lie within B's, the rest of B holds
    // exactly B - A mines.
    for (const Constraint& a : cs) {
        for (const Constraint& b : cs) {
            if (&a == &b || a.cells.size() >= b.cells.size() || !isSubset(a.cells, b.cells))
                continue;
            std::vector<int> rest;
            std::set_difference(b.cells.begin(), b.cells.end(), a.cells.begin(), a.cells.end(),
                std::back_inserter(rest));
            const int restMines = b.mines - a.mines;
            if (restMines == 0)
                mark(rest, 1);
            else if (restMines == static_cast<int>(rest.size()))
                mark(rest, 2);
        }
    }
    if (!out.safe.empty() || !out.mines.empty())
        return true;

    // Endgame: the total mine count settles every remaining unknown cell.
    int unknown = 0;
    int minesLeft = k.field.mineCount();
    for (std::size_t cell = 0; cell < k.known.size(); ++cell) {
        unknown += k.known[cell] == 0;
        minesLeft -= k.known[cell] == 2;
    }
    if (unknown > 0 && (minesLeft == 0 || minesLeft == unknown)) {
        std::vector<int> all;
        for (std::size_t cell = 0; cell < k.known.size(); ++cell)
            if (k.known[cell] == 0)
                all.push_back(static_cast<int>(cell));
        mark(all, minesLeft == 0 ? 1 : 2);
    }
    return !out.safe.empty() || !out.mines.empty();
}

} // namespace

Deductions deduce(const Field& field)
{
    Deductions out;
    if (field.minesPlaced())
        deduceRound(Knowledge(field), out);
    return out;
}

bool solvableWithoutGuessing(const Field& layout, int start)
{
    Field field = layout;
    for (int cell = 0; cell < field.cellCount(); ++cell)
        field.setState(cell, Cell::Hidden);
    if (field.reveal(start).hitMine)
        return false;
    while (!field.cleared()) {
        Deductions d;
        if (!deduceRound(Knowledge(field), d))
            return false;
        for (int cell : d.mines)
            field.setState(cell, Cell::Flagged);
        for (int cell : d.safe)
            if (field.reveal(cell).hitMine)
                return false; // cannot happen with sound rules
    }
    return true;
}

} // namespace minesweeper
