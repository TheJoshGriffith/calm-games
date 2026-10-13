#include <minesweeper/Generator.h>
#include <minesweeper/Solver.h>

#include <algorithm>
#include <vector>

namespace minesweeper {

Layout layoutFor(Difficulty difficulty)
{
    switch (difficulty) {
    case Difficulty::Easy:
        return { 9, 9, 10 };
    case Difficulty::Medium:
        return { 10, 14, 24 };
    case Difficulty::Hard:
        return { 12, 18, 40 };
    case Difficulty::Expert:
        return { 12, 20, 52 };
    }
    return { 9, 9, 10 };
}

const char* difficultyName(Difficulty difficulty)
{
    switch (difficulty) {
    case Difficulty::Easy:
        return "Easy";
    case Difficulty::Medium:
        return "Medium";
    case Difficulty::Hard:
        return "Hard";
    case Difficulty::Expert:
        return "Expert";
    }
    return "Easy";
}

bool difficultyFromName(std::string_view name, Difficulty& out)
{
    for (Difficulty d : { Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert }) {
        if (name == difficultyName(d)) {
            out = d;
            return true;
        }
    }
    return false;
}

bool layMines(Field& field, int firstCell, calm::Rng& rng, bool noGuessing, int maxAttempts)
{
    // Cells that may hold a mine: everything except the first tap and, when
    // there is room, its neighbours.
    std::vector<int> candidates;
    std::vector<std::uint8_t> keepClear(static_cast<std::size_t>(field.cellCount()), 0);
    keepClear[firstCell] = 1;
    const Neighbours around = field.neighbours(firstCell);
    if (field.cellCount() - 1 - around.count >= field.mineCount())
        for (int n : around)
            keepClear[n] = 1;
    for (int cell = 0; cell < field.cellCount(); ++cell)
        if (!keepClear[cell])
            candidates.push_back(cell);

    const int mines = std::min(field.mineCount(), static_cast<int>(candidates.size()));
    for (int attempt = 0; attempt < (noGuessing ? maxAttempts : 1); ++attempt) {
        rng.shuffle(candidates.begin(), candidates.end());
        field.placeMines(std::vector<int>(candidates.begin(), candidates.begin() + mines));
        if (!noGuessing)
            return false;
        if (solvableWithoutGuessing(field, firstCell))
            return true;
    }
    return false;
}

} // namespace minesweeper
