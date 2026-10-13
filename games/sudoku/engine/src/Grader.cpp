#include "sudoku/Grader.h"

#include <algorithm>

namespace sudoku {

namespace {

// All 9-bit masks with exactly `n` bits set, used to enumerate subsets of a
// unit's cells or digits.
const std::array<std::array<int, 128>, 5>& subsetMasks(std::array<int, 5>& counts)
{
    static std::array<int, 5> sizes {};
    static const auto table = [] {
        std::array<std::array<int, 128>, 5> t {};
        for (int m = 0; m < 512; ++m) {
            const int n = popcount(static_cast<Mask>(m));
            if (n >= 2 && n <= 4)
                t[n][sizes[n]++] = m;
        }
        return t;
    }();
    counts = sizes;
    return table;
}

struct Placement {
    int cell = -1;
    int digit = 0;
};

class LogicSolver {
public:
    explicit LogicSolver(const Grid& grid)
    {
        m_cand.fill(AllDigits);
        for (int i = 0; i < 81; ++i)
            if (grid[i] != 0)
                place(i, grid[i]);
    }

    bool complete() const { return clueCount(m_grid) == 81; }
    bool broken() const { return m_broken; }
    const Grid& grid() const { return m_grid; }

    // Applies the cheapest technique that makes progress. Returns the
    // technique used, or None if stuck. `placed` reports a filled cell.
    Technique step(Placement& placed)
    {
        placed = {};
        if (m_broken)
            return Technique::None;
        if (nakedSingle(placed))
            return Technique::NakedSingle;
        if (hiddenSingle(placed))
            return Technique::HiddenSingle;
        if (lockedCandidates())
            return Technique::LockedCandidates;
        if (nakedSubset(2))
            return Technique::NakedPair;
        if (hiddenSubset(2))
            return Technique::HiddenPair;
        if (nakedSubset(3))
            return Technique::NakedTriple;
        if (hiddenSubset(3))
            return Technique::HiddenTriple;
        if (fish(2))
            return Technique::XWing;
        if (fish(3))
            return Technique::Swordfish;
        if (xyWing())
            return Technique::XYWing;
        return Technique::None;
    }

private:
    void place(int cell, int digit)
    {
        if (!(m_cand[cell] & bit(digit)))
            m_broken = true;
        m_grid[cell] = static_cast<std::uint8_t>(digit);
        m_cand[cell] = 0;
        for (int p : peers()[cell]) {
            if (m_grid[p] == digit)
                m_broken = true;
            m_cand[p] &= static_cast<Mask>(~bit(digit));
            if (m_grid[p] == 0 && m_cand[p] == 0)
                m_broken = true;
        }
    }

    bool eliminate(int cell, Mask digits)
    {
        if (m_grid[cell] != 0 || !(m_cand[cell] & digits))
            return false;
        m_cand[cell] &= static_cast<Mask>(~digits);
        if (m_cand[cell] == 0)
            m_broken = true;
        return true;
    }

    bool nakedSingle(Placement& placed)
    {
        for (int i = 0; i < 81; ++i) {
            if (m_grid[i] == 0 && popcount(m_cand[i]) == 1) {
                placed = { i, firstDigit(m_cand[i]) };
                place(placed.cell, placed.digit);
                return true;
            }
        }
        return false;
    }

    bool hiddenSingle(Placement& placed)
    {
        for (const auto& unit : units()) {
            for (int d = 1; d <= 9; ++d) {
                int where = -1;
                int count = 0;
                for (int cell : unit) {
                    if (m_grid[cell] == d) {
                        count = 2; // already placed in this unit
                        break;
                    }
                    if (m_cand[cell] & bit(d)) {
                        where = cell;
                        ++count;
                    }
                }
                if (count == 1) {
                    placed = { where, d };
                    place(where, d);
                    return true;
                }
            }
        }
        return false;
    }

    // Pointing: a digit confined to one row/column within a box is removed
    // from the rest of that row/column. Claiming: a digit confined to one box
    // within a row/column is removed from the rest of that box.
    bool lockedCandidates()
    {
        const auto& u = units();
        for (int base = 0; base < 27; ++base) {
            for (int d = 1; d <= 9; ++d) {
                int rows = 0, cols = 0, boxes = 0, count = 0;
                for (int cell : u[base]) {
                    if (m_cand[cell] & bit(d)) {
                        rows |= 1 << rowOf(cell);
                        cols |= 1 << colOf(cell);
                        boxes |= 1 << boxOf(cell);
                        ++count;
                    }
                }
                if (count < 2)
                    continue;
                int target = -1;
                if (base >= 18) {
                    if (popcount(static_cast<Mask>(rows)) == 1)
                        target = firstDigit(static_cast<Mask>(rows));
                    else if (popcount(static_cast<Mask>(cols)) == 1)
                        target = 9 + firstDigit(static_cast<Mask>(cols));
                } else if (popcount(static_cast<Mask>(boxes)) == 1) {
                    target = 18 + firstDigit(static_cast<Mask>(boxes));
                }
                if (target < 0)
                    continue;
                bool progress = false;
                for (int cell : u[target]) {
                    if (std::find(u[base].begin(), u[base].end(), cell) != u[base].end())
                        continue;
                    progress |= eliminate(cell, bit(d));
                }
                if (progress)
                    return true;
            }
        }
        return false;
    }

    // n cells in a unit whose candidates together cover only n digits.
    bool nakedSubset(int n)
    {
        std::array<int, 5> counts {};
        const auto& masks = subsetMasks(counts);
        for (const auto& unit : units()) {
            for (int k = 0; k < counts[n]; ++k) {
                const int sel = masks[n][k];
                Mask digits = 0;
                bool ok = true;
                for (int j = 0; j < 9 && ok; ++j) {
                    if (!(sel & (1 << j)))
                        continue;
                    const int cell = unit[j];
                    const int size = popcount(m_cand[cell]);
                    if (m_grid[cell] != 0 || size < 2 || size > n)
                        ok = false;
                    digits |= m_cand[cell];
                }
                if (!ok || popcount(digits) != n)
                    continue;
                bool progress = false;
                for (int j = 0; j < 9; ++j)
                    if (!(sel & (1 << j)))
                        progress |= eliminate(unit[j], digits);
                if (progress)
                    return true;
            }
        }
        return false;
    }

    // n digits in a unit that can only go in the same n cells.
    bool hiddenSubset(int n)
    {
        std::array<int, 5> counts {};
        const auto& masks = subsetMasks(counts);
        for (const auto& unit : units()) {
            int positions[10] {};
            for (int d = 1; d <= 9; ++d)
                for (int j = 0; j < 9; ++j)
                    if (m_cand[unit[j]] & bit(d))
                        positions[d] |= 1 << j;
            for (int k = 0; k < counts[n]; ++k) {
                const int digitSel = masks[n][k] << 1; // digits 1..9 as bits 1..9
                int cells = 0;
                bool ok = true;
                for (int d = 1; d <= 9 && ok; ++d) {
                    if (!(digitSel & (1 << d)))
                        continue;
                    if (positions[d] == 0)
                        ok = false; // placed already, or not a candidate anywhere
                    cells |= positions[d];
                }
                if (!ok || popcount(static_cast<Mask>(cells)) != n)
                    continue;
                bool progress = false;
                for (int j = 0; j < 9; ++j)
                    if (cells & (1 << j))
                        progress |= eliminate(unit[j], static_cast<Mask>(AllDigits & ~digitSel));
                if (progress)
                    return true;
            }
        }
        return false;
    }

    // X-Wing (n = 2) and Swordfish (n = 3), in both orientations.
    bool fish(int n)
    {
        std::array<int, 5> counts {};
        const auto& masks = subsetMasks(counts);
        for (int d = 1; d <= 9; ++d) {
            for (int orientation = 0; orientation < 2; ++orientation) {
                // lines[i] = cross positions of d in base line i
                int lines[9] {};
                for (int i = 0; i < 9; ++i) {
                    for (int j = 0; j < 9; ++j) {
                        const int cell = orientation == 0 ? i * 9 + j : j * 9 + i;
                        if (m_cand[cell] & bit(d))
                            lines[i] |= 1 << j;
                    }
                }
                for (int k = 0; k < counts[n]; ++k) {
                    const int sel = masks[n][k];
                    int cover = 0;
                    bool ok = true;
                    for (int i = 0; i < 9 && ok; ++i) {
                        if (!(sel & (1 << i)))
                            continue;
                        const int size = popcount(static_cast<Mask>(lines[i]));
                        if (size < 2 || size > n)
                            ok = false;
                        cover |= lines[i];
                    }
                    if (!ok || popcount(static_cast<Mask>(cover)) != n)
                        continue;
                    bool progress = false;
                    for (int i = 0; i < 9; ++i) {
                        if (sel & (1 << i))
                            continue;
                        for (int j = 0; j < 9; ++j) {
                            if (!(cover & (1 << j)))
                                continue;
                            const int cell = orientation == 0 ? i * 9 + j : j * 9 + i;
                            progress |= eliminate(cell, bit(d));
                        }
                    }
                    if (progress)
                        return true;
                }
            }
        }
        return false;
    }

    // Pivot {a,b} sees pincers {a,c} and {b,c}: c is removed from every cell
    // that sees both pincers.
    bool xyWing()
    {
        for (int pivot = 0; pivot < 81; ++pivot) {
            const Mask pm = m_cand[pivot];
            if (m_grid[pivot] != 0 || popcount(pm) != 2)
                continue;
            for (int q : peers()[pivot]) {
                const Mask qm = m_cand[q];
                if (m_grid[q] != 0 || popcount(qm) != 2 || popcount(static_cast<Mask>(qm & pm)) != 1)
                    continue;
                const Mask c = static_cast<Mask>(qm & ~pm);
                const Mask want = static_cast<Mask>((pm & ~qm) | c);
                for (int r : peers()[pivot]) {
                    if (r == q || m_grid[r] != 0 || m_cand[r] != want)
                        continue;
                    bool progress = false;
                    for (int cell : peers()[q])
                        if (cell != r && cell != pivot && arePeers(cell, r))
                            progress |= eliminate(cell, c);
                    if (progress)
                        return true;
                }
            }
        }
        return false;
    }

    Grid m_grid {};
    std::array<Mask, 81> m_cand {};
    bool m_broken = false;
};

} // namespace

std::string_view techniqueName(Technique technique) noexcept
{
    switch (technique) {
    case Technique::None: return "None";
    case Technique::NakedSingle: return "Naked single";
    case Technique::HiddenSingle: return "Hidden single";
    case Technique::LockedCandidates: return "Locked candidates";
    case Technique::NakedPair: return "Naked pair";
    case Technique::HiddenPair: return "Hidden pair";
    case Technique::NakedTriple: return "Naked triple";
    case Technique::HiddenTriple: return "Hidden triple";
    case Technique::XWing: return "X-Wing";
    case Technique::Swordfish: return "Swordfish";
    case Technique::XYWing: return "XY-Wing";
    }
    return "Unknown";
}

std::string_view difficultyName(Difficulty difficulty) noexcept
{
    switch (difficulty) {
    case Difficulty::Easy: return "Easy";
    case Difficulty::Medium: return "Medium";
    case Difficulty::Hard: return "Hard";
    case Difficulty::Expert: return "Expert";
    }
    return "Unknown";
}

std::optional<Difficulty> parseDifficulty(std::string_view name) noexcept
{
    auto equals = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            const char x = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
            const char y = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
            if (x != y)
                return false;
        }
        return true;
    };
    for (int i = 0; i < DifficultyCount; ++i) {
        const auto d = static_cast<Difficulty>(i);
        if (equals(name, difficultyName(d)))
            return d;
    }
    return std::nullopt;
}

Difficulty difficultyFor(Technique hardest) noexcept
{
    if (hardest <= Technique::HiddenSingle)
        return Difficulty::Easy;
    if (hardest <= Technique::HiddenPair)
        return Difficulty::Medium;
    if (hardest <= Technique::XWing)
        return Difficulty::Hard;
    return Difficulty::Expert;
}

GradeResult grade(const Grid& puzzle)
{
    GradeResult result;
    LogicSolver solver(puzzle);
    Placement placed;
    while (!solver.broken() && !solver.complete()) {
        const Technique t = solver.step(placed);
        if (t == Technique::None)
            break;
        ++result.uses[static_cast<int>(t)];
        result.hardest = std::max(result.hardest, t);
    }
    result.grid = solver.grid();
    result.solved = !solver.broken() && isSolved(result.grid);
    return result;
}

std::optional<Hint> hint(const Grid& position)
{
    if (!isConsistent(position))
        return std::nullopt;
    LogicSolver solver(position);
    Technique hardest = Technique::None;
    Placement placed;
    while (!solver.broken() && !solver.complete()) {
        const Technique t = solver.step(placed);
        if (t == Technique::None)
            return std::nullopt;
        hardest = std::max(hardest, t);
        if (placed.cell >= 0)
            return Hint { hardest, placed.cell, placed.digit };
    }
    return std::nullopt;
}

} // namespace sudoku
