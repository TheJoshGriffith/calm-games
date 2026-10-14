// Dependency-free tests for the Minesweeper engine, run by CTest.

#include <minesweeper/Field.h>
#include <minesweeper/Generator.h>
#include <minesweeper/Solver.h>

#include <calm/Rng.h>

#include <chrono>
#include <cstdio>
#include <functional>
#include <vector>

using namespace minesweeper;

namespace {

int g_failures = 0;

#define CHECK(expr)                                                                   \
    do {                                                                              \
        if (!(expr)) {                                                                \
            std::fprintf(stderr, "  FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

// 4x3 field, mines at (0,3) and (2,3):
//   . . 1 *
//   . . 2 2
//   . . 1 *
Field smallField()
{
    Field f(4, 3, 2);
    f.placeMines({ 3, 11 });
    return f;
}

void neighboursAndCounts()
{
    Field f = smallField();
    CHECK(f.neighbours(0).count == 3);
    CHECK(f.neighbours(5).count == 8);
    CHECK(f.neighbours(11).count == 3);
    CHECK(f.adjacentMines(2) == 1);
    CHECK(f.adjacentMines(6) == 2);
    CHECK(f.adjacentMines(7) == 2);
    CHECK(f.adjacentMines(0) == 0);
}

void revealFloodsEmptyArea()
{
    Field f = smallField();
    const RevealResult r = f.reveal(0);
    CHECK(!r.hitMine);
    CHECK(r.opened == 9); // columns 0-2
    CHECK(f.state(7) == Cell::Hidden);
    CHECK(f.reveal(0).opened == 0); // already open
    f.reveal(7);
    CHECK(f.cleared());
}

void flagsAndChords()
{
    Field f = smallField();
    CHECK(f.toggleFlag(3));
    CHECK(f.state(3) == Cell::Flagged);
    CHECK(f.reveal(3).opened == 0); // flagged cells don't open
    f.reveal(2);
    CHECK(!f.toggleFlag(2));        // revealed cells can't be flagged
    CHECK(f.chord(2).opened > 0);   // flag count matches the 1
    CHECK(f.state(7) == Cell::Revealed);
    CHECK(f.toggleFlag(3));
    CHECK(f.flagCount() == 0);
}

void flagAroundOnlyWhenCertain()
{
    Field f = smallField();
    f.reveal(0);                    // opens columns 0-2
    CHECK(f.flagAround(0) == 0);    // empty cell
    CHECK(f.flagAround(3) == 0);    // hidden cell
    CHECK(f.flagAround(6) == 0);    // the 2 at (1,2) has three hidden neighbours
    CHECK(f.flagAround(2) == 0);    // the 1 at (0,2) has two hidden neighbours
    f.reveal(7);                    // the 2 at (1,3): its hidden neighbours are both mines
    CHECK(f.flagAround(7) == 2);
    CHECK(f.state(3) == Cell::Flagged);
    CHECK(f.state(11) == Cell::Flagged);
    CHECK(f.flagAround(7) == 0);    // nothing left to flag
    CHECK(f.flagCount() == 2);
}

void wrongChordHitsMine()
{
    Field f = smallField();
    f.reveal(6);           // the 2 at row 1, column 2
    f.toggleFlag(2);       // wrong: (0,2) is safe
    f.toggleFlag(11);
    const RevealResult r = f.chord(6);
    CHECK(r.hitMine);
    CHECK(r.mineCell == 3);
}

void firstTapIsAlwaysOpenArea()
{
    calm::Rng rng(7);
    for (Difficulty d : { Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert }) {
        const Layout l = layoutFor(d);
        for (int i = 0; i < 20; ++i) {
            Field f(l.cols, l.rows, l.mines);
            const int first = static_cast<int>(rng.below(static_cast<std::uint32_t>(f.cellCount())));
            layMines(f, first, rng, false);
            CHECK(f.mineCount() == l.mines);
            CHECK(!f.isMine(first));
            CHECK(f.adjacentMines(first) == 0);
            CHECK(f.reveal(first).opened > 1);
        }
    }
}

void noGuessBoardsSolveByLogic()
{
    using Clock = std::chrono::steady_clock;
    for (Difficulty d : { Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert }) {
        const Layout l = layoutFor(d);
        auto worst = Clock::duration::zero();
        int guaranteed = 0;
        for (int seed = 1; seed <= 10; ++seed) {
            calm::Rng rng(static_cast<std::uint64_t>(seed) * 977);
            Field f(l.cols, l.rows, l.mines);
            const int first = (l.rows / 2) * l.cols + l.cols / 2;
            const auto start = Clock::now();
            const bool ok = layMines(f, first, rng, true);
            worst = std::max(worst, Clock::now() - start);
            guaranteed += ok;
            if (ok)
                CHECK(solvableWithoutGuessing(f, first));
        }
        CHECK(guaranteed == 10);
        std::printf("     %s: slowest no-guess board %lld ms\n", difficultyName(d),
            static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(worst).count()));
    }
}

void deduceFindsSafeCells()
{
    Field f = smallField();
    f.reveal(0);
    const Deductions d = deduce(f);
    // The 1 at (0,2) and 1 at (2,2) each have one unknown mine-neighbour pair.
    CHECK(!d.mines.empty() || !d.safe.empty());
    for (int c : d.safe)
        CHECK(!f.isMine(c));
    for (int c : d.mines)
        CHECK(f.isMine(c));
}

void generationIsDeterministic()
{
    const Layout l = layoutFor(Difficulty::Hard);
    Field a(l.cols, l.rows, l.mines);
    Field b(l.cols, l.rows, l.mines);
    calm::Rng ra(42);
    calm::Rng rb(42);
    layMines(a, 5, ra, true);
    layMines(b, 5, rb, true);
    CHECK(a.mines() == b.mines());
}

void difficultyNamesRoundTrip()
{
    for (Difficulty d : { Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert }) {
        Difficulty back = Difficulty::Easy;
        CHECK(difficultyFromName(difficultyName(d), back));
        CHECK(back == d);
    }
    Difficulty unused;
    CHECK(!difficultyFromName("Impossible", unused));
}

} // namespace

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        { "neighbours and counts", neighboursAndCounts },
        { "reveal floods empty area", revealFloodsEmptyArea },
        { "flags and chords", flagsAndChords },
        { "flag around only when certain", flagAroundOnlyWhenCertain },
        { "wrong chord hits mine", wrongChordHitsMine },
        { "first tap is always an open area", firstTapIsAlwaysOpenArea },
        { "no-guess boards solve by logic", noGuessBoardsSolveByLogic },
        { "deduce finds safe cells", deduceFindsSafeCells },
        { "generation is deterministic", generationIsDeterministic },
        { "difficulty names round trip", difficultyNamesRoundTrip },
    };
    for (const auto& [name, fn] : tests) {
        const int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", name);
    }
    std::printf("%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
