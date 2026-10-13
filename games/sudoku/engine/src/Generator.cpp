#include "sudoku/Generator.h"

#include "sudoku/Solver.h"

#include <calm/Rng.h>

#include <numeric>
#include <vector>

namespace sudoku {

int minimumClues(Difficulty difficulty) noexcept
{
    switch (difficulty) {
    case Difficulty::Easy: return 36;
    case Difficulty::Medium: return 28;
    case Difficulty::Hard: return 22;
    case Difficulty::Expert: return 20;
    }
    return 17;
}

std::optional<Puzzle> generate(Difficulty difficulty, std::uint64_t seed, const GeneratorOptions& options)
{
    const int floor = minimumClues(difficulty);

    for (int attempt = 0; attempt < options.maxAttempts; ++attempt) {
        calm::Rng rng(calm::Rng::mix(seed, static_cast<std::uint64_t>(attempt)));
        const Grid solution = randomSolvedGrid(rng);

        // Removal groups: single cells, or symmetric pairs (the centre alone).
        std::vector<std::vector<int>> groups;
        if (options.symmetric) {
            for (int i = 0; i <= 40; ++i)
                groups.push_back(i == 40 ? std::vector<int> { 40 } : std::vector<int> { i, 80 - i });
        } else {
            for (int i = 0; i < 81; ++i)
                groups.push_back({ i });
        }
        rng.shuffle(groups.begin(), groups.end());

        Grid puzzle = solution;
        int clues = 81;
        // The most-dug position seen so far that grades exactly at target.
        // Digging further often drops a puzzle back to an easier grade, so
        // remembering this makes hitting the target far more likely.
        std::optional<Puzzle> best;
        for (const auto& group : groups) {
            if (clues - static_cast<int>(group.size()) < floor)
                continue;
            for (int cell : group)
                puzzle[cell] = 0;

            bool keep = countSolutions(puzzle, 2) == 1;
            GradeResult g;
            if (keep) {
                // Never dig past the target: harder (or unsolvable by logic)
                // puzzles put the cells back.
                g = grade(puzzle);
                keep = g.solved && difficultyFor(g.hardest) <= difficulty;
            }

            if (keep) {
                clues -= static_cast<int>(group.size());
                if (difficultyFor(g.hardest) == difficulty) {
                    best = Puzzle { puzzle, solution, seed, difficulty, g.hardest, clues };
                }
            } else {
                for (int cell : group)
                    puzzle[cell] = solution[cell];
            }
        }

        if (best)
            return best;
    }
    return std::nullopt;
}

} // namespace sudoku
