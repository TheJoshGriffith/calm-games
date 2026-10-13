// sudoku-gen: prints graded puzzles from the Sudoku engine.
//
//   sudoku-gen --level hard --count 5 --seed 42 [--pretty]
//   sudoku-gen --grade 530070000600195000098000060800060003400803001700020006060000280000419005000080079

#include <sudoku/Generator.h>
#include <sudoku/Solver.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

int usage()
{
    std::fprintf(stderr,
        "usage: sudoku-gen [--level easy|medium|hard|expert] [--count N] [--seed S] [--pretty]\n"
        "       sudoku-gen --grade <81-character puzzle>\n");
    return 2;
}

int gradeOnly(std::string_view text)
{
    const auto grid = sudoku::parseGrid(text);
    if (!grid) {
        std::fprintf(stderr, "not a valid 81-character puzzle\n");
        return 2;
    }
    const int solutions = sudoku::countSolutions(*grid, 2);
    const auto g = sudoku::grade(*grid);
    std::printf("solutions: %s\n", solutions == 0 ? "none" : solutions == 1 ? "unique" : "multiple");
    if (g.solved) {
        std::printf("difficulty: %s (hardest: %s)\n",
            std::string(sudoku::difficultyName(*g.difficulty())).c_str(),
            std::string(sudoku::techniqueName(g.hardest)).c_str());
    } else {
        std::printf("difficulty: beyond Expert (logic stalls after %s)\n",
            std::string(sudoku::techniqueName(g.hardest)).c_str());
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    sudoku::Difficulty level = sudoku::Difficulty::Medium;
    int count = 1;
    std::uint64_t seed = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    bool pretty = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--level" && hasValue) {
            const auto parsed = sudoku::parseDifficulty(argv[++i]);
            if (!parsed)
                return usage();
            level = *parsed;
        } else if (arg == "--count" && hasValue) {
            count = std::atoi(argv[++i]);
        } else if (arg == "--seed" && hasValue) {
            seed = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--pretty") {
            pretty = true;
        } else if (arg == "--grade" && hasValue) {
            return gradeOnly(argv[++i]);
        } else {
            return usage();
        }
    }

    for (int n = 0; n < count; ++n) {
        const std::uint64_t puzzleSeed = seed + static_cast<std::uint64_t>(n);
        const auto start = std::chrono::steady_clock::now();
        const auto puzzle = sudoku::generate(level, puzzleSeed);
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (!puzzle) {
            std::printf("seed=%llu no %s puzzle found\n", static_cast<unsigned long long>(puzzleSeed),
                std::string(sudoku::difficultyName(level)).c_str());
            continue;
        }
        std::printf("%s %s hardest=\"%s\" clues=%d seed=%llu ms=%.1f\n",
            sudoku::formatGrid(puzzle->puzzle).c_str(),
            std::string(sudoku::difficultyName(puzzle->difficulty)).c_str(),
            std::string(sudoku::techniqueName(puzzle->hardest)).c_str(),
            puzzle->clues,
            static_cast<unsigned long long>(puzzle->seed),
            ms);
        if (pretty)
            std::printf("%s\n", sudoku::prettyGrid(puzzle->puzzle).c_str());
    }
    return 0;
}
