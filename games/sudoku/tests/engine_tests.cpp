// Dependency-free tests for the Sudoku engine, run by CTest.

#include <calm/Rng.h>
#include <calm/UndoStack.h>
#include <sudoku/Generator.h>
#include <sudoku/Solver.h>

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(expr)                                                                   \
    do {                                                                              \
        if (!(expr)) {                                                                \
            std::fprintf(stderr, "  FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

const char* kClassic = "530070000600195000098000060800060003400803001700020006060000280000419005000080079";
const char* kClassicSolution = "534678912672195348198342567859761423426853791713924856961537284287419635345286179";

void boardBasics()
{
    CHECK(sudoku::units()[0][8] == 8);
    CHECK(sudoku::units()[9 + 1][2] == 19);
    CHECK(sudoku::units()[18 + 4][0] == 30);
    for (int i = 0; i < 81; ++i)
        CHECK(sudoku::peers()[i].size() == 20);
    CHECK(sudoku::arePeers(0, 80) == false);
    CHECK(sudoku::arePeers(0, 20) == true);

    const auto grid = sudoku::parseGrid(kClassic);
    CHECK(grid.has_value());
    CHECK(sudoku::formatGrid(*grid) == kClassic);
    CHECK(sudoku::clueCount(*grid) == 30);
    CHECK(!sudoku::parseGrid("123").has_value());
    CHECK(sudoku::parseGrid(std::string(81, '.')).has_value());
}

void solverFindsKnownSolution()
{
    const auto grid = *sudoku::parseGrid(kClassic);
    CHECK(sudoku::countSolutions(grid, 2) == 1);
    const auto solved = sudoku::solve(grid);
    CHECK(solved.has_value());
    CHECK(sudoku::formatGrid(*solved) == kClassicSolution);
    CHECK(sudoku::isSolved(*solved));
}

void solverDetectsMultipleAndNoSolutions()
{
    sudoku::Grid empty {};
    CHECK(sudoku::countSolutions(empty, 2) == 2);

    auto broken = *sudoku::parseGrid(kClassic);
    broken[2] = 5; // duplicate 5 in row 1
    CHECK(sudoku::countSolutions(broken, 2) == 0);
}

void randomGridsAreValidAndDeterministic()
{
    calm::Rng a(7), b(7), c(8);
    const auto ga = sudoku::randomSolvedGrid(a);
    const auto gb = sudoku::randomSolvedGrid(b);
    const auto gc = sudoku::randomSolvedGrid(c);
    CHECK(sudoku::isSolved(ga));
    CHECK(ga == gb);
    CHECK(ga != gc);
}

void graderOnClassicIsEasy()
{
    const auto result = sudoku::grade(*sudoku::parseGrid(kClassic));
    CHECK(result.solved);
    CHECK(result.difficulty() == sudoku::Difficulty::Easy);
    CHECK(sudoku::formatGrid(result.grid) == kClassicSolution);
}

void graderNeedsAdvancedTechnique()
{
    // A published X-Wing example: singles stall until the X-Wing elimination.
    const auto grid = *sudoku::parseGrid(
        "100000569492056108056109240009640801064010000218035604040500016905061402621000005");
    const auto result = sudoku::grade(grid);
    CHECK(result.solved);
    CHECK(result.hardest >= sudoku::Technique::LockedCandidates);
}

void hintPlacesCorrectDigit()
{
    const auto grid = *sudoku::parseGrid(kClassic);
    const auto solution = *sudoku::parseGrid(kClassicSolution);
    const auto h = sudoku::hint(grid);
    CHECK(h.has_value());
    if (h) {
        CHECK(grid[h->cell] == 0);
        CHECK(solution[h->cell] == h->digit);
    }

    auto wrong = grid;
    wrong[2] = 5; // contradiction
    CHECK(!sudoku::hint(wrong).has_value());
}

void generatorHitsEveryLevel()
{
    for (int level = 0; level < sudoku::DifficultyCount; ++level) {
        const auto difficulty = static_cast<sudoku::Difficulty>(level);
        int generated = 0;
        for (std::uint64_t seed = 1; seed <= 4; ++seed) {
            const auto p = sudoku::generate(difficulty, seed);
            if (!p)
                continue;
            ++generated;
            CHECK(sudoku::countSolutions(p->puzzle, 2) == 1);
            CHECK(sudoku::isSolved(p->solution));
            CHECK(*sudoku::solve(p->puzzle) == p->solution);
            CHECK(p->clues == sudoku::clueCount(p->puzzle));
            CHECK(p->clues >= sudoku::minimumClues(difficulty));
            const auto g = sudoku::grade(p->puzzle);
            CHECK(g.solved);
            CHECK(g.difficulty() == difficulty);
            for (int i = 0; i < 81; ++i)
                CHECK(p->puzzle[i] == 0 || p->puzzle[i] == p->solution[i]);
        }
        if (generated == 0)
            std::fprintf(stderr, "  no %s puzzle generated\n", std::string(sudoku::difficultyName(difficulty)).c_str());
        CHECK(generated == 4);
    }
}

void generatorIsDeterministic()
{
    const auto a = sudoku::generate(sudoku::Difficulty::Hard, 1234);
    const auto b = sudoku::generate(sudoku::Difficulty::Hard, 1234);
    CHECK(a.has_value() && b.has_value());
    if (a && b)
        CHECK(a->puzzle == b->puzzle);
}

void undoStackRoundTrips()
{
    calm::UndoStack<int> stack;
    stack.push(1);
    stack.push(2);
    auto s = stack.undo(3);
    CHECK(s == 2);
    s = stack.undo(*s);
    CHECK(s == 1);
    CHECK(!stack.canUndo());
    s = stack.redo(*s);
    CHECK(s == 2);
    stack.push(9);
    CHECK(!stack.canRedo());
}

} // namespace

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        { "board basics", boardBasics },
        { "solver finds known solution", solverFindsKnownSolution },
        { "solver detects multiple and no solutions", solverDetectsMultipleAndNoSolutions },
        { "random grids are valid and deterministic", randomGridsAreValidAndDeterministic },
        { "grader rates classic puzzle Easy", graderOnClassicIsEasy },
        { "grader needs advanced technique", graderNeedsAdvancedTechnique },
        { "hint places correct digit", hintPlacesCorrectDigit },
        { "generator hits every level", generatorHitsEveryLevel },
        { "generator is deterministic", generatorIsDeterministic },
        { "undo stack round trips", undoStackRoundTrips },
    };
    for (const auto& [name, fn] : tests) {
        const int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", name);
    }
    std::printf("%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
