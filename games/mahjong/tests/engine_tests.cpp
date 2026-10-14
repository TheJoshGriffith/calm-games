// Dependency-free tests for the Mahjong solitaire engine, run by CTest.

#include <mahjong/Board.h>
#include <mahjong/Generator.h>
#include <mahjong/Layout.h>
#include <mahjong/Solver.h>
#include <mahjong/Tiles.h>

#include <calm/Rng.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <vector>

using namespace mahjong;

namespace {

int g_failures = 0;

#define CHECK(expr)                                                                   \
    do {                                                                              \
        if (!(expr)) {                                                                \
            std::fprintf(stderr, "  FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

// Three tiles in a row with one on top of the middle:
//      [3]
//   [0][1][2]
Board rowWithTop()
{
    return Board(parseLayout("Row", 1, { { "###" }, { ".#." } }));
}

void tileKindsAndMatching()
{
    CHECK(suitOf(0) == Suit::Dots && rankOf(0) == 1);
    CHECK(suitOf(17) == Suit::Bamboo && rankOf(17) == 9);
    CHECK(suitOf(27) == Suit::Winds && rankOf(27) == 1);
    CHECK(suitOf(33) == Suit::Dragons && rankOf(33) == 3);
    CHECK(suitOf(41) == Suit::Seasons && rankOf(41) == 4);
    CHECK(matches(5, 5));
    CHECK(!matches(5, 6));
    CHECK(matches(34, 37)); // any two flowers
    CHECK(matches(38, 40)); // any two seasons
    CHECK(!matches(37, 38));
    CHECK(kindName(31) == "Red dragon");

    // A full set: 144 tiles, four of every face (flowers and seasons count
    // as one face each).
    std::map<int, int> groups;
    for (const auto& [a, b] : fullSetPairs()) {
        CHECK(matches(a, b));
        ++groups[matchGroup(a)];
        ++groups[matchGroup(b)];
    }
    CHECK(fullSetPairs().size() == 72);
    CHECK(groups.size() == 36);
    for (const auto& [group, count] : groups)
        CHECK(count == 4);
}

void layoutsAreWellFormed()
{
    CHECK(layouts().size() >= 6);
    CHECK(findLayout("turtle") == findLayout("Turtle"));
    CHECK(findLayout("Turtle")->places.size() == 144);
    CHECK(findLayout("Nowhere") == nullptr);
    for (const Layout& layout : layouts()) {
        CHECK(layout.places.size() % 2 == 0);
        CHECK(layout.places.size() <= std::size_t(FullSetSize));
        CHECK(layout.difficulty >= 1 && layout.difficulty <= 4);
        for (std::size_t i = 0; i < layout.places.size(); ++i) {
            const Slot& a = layout.places[i];
            bool supported = a.z == 0;
            for (std::size_t j = 0; j < layout.places.size(); ++j) {
                const Slot& b = layout.places[j];
                const bool overlap = std::abs(a.x - b.x) < 2 && std::abs(a.y - b.y) < 2;
                // No two tiles in the same place on a layer...
                if (i != j && a.z == b.z && overlap) {
                    std::fprintf(stderr, "  %s: tiles %zu and %zu overlap\n", layout.name.c_str(), i, j);
                    CHECK(false);
                }
                // ...and nothing floats in the air.
                if (b.z == a.z - 1 && overlap)
                    supported = true;
            }
            CHECK(supported);
        }
    }
}

void freeTileRules()
{
    Board board = rowWithTop();
    CHECK(board.size() == 4);
    CHECK(board.isFree(0));  // open on the left
    CHECK(!board.isFree(1)); // covered, and blocked both sides
    CHECK(board.isFree(2));  // open on the right
    CHECK(board.isFree(3));  // on top

    board.setKinds({ 7, 9, 9, 7 });
    CHECK(!board.canMatch(1, 2)); // 1 isn't free
    CHECK(board.canMatch(0, 3));
    CHECK(!board.removePair(0, 0));
    CHECK(board.removePair(0, 3));
    CHECK(board.remaining() == 2);
    CHECK(board.isFree(1)); // uncovered, and now open on the left
    CHECK(board.availablePairs().size() == 1);
    CHECK(board.removePair(1, 2));
    CHECK(board.cleared());

    // Half-tile offsets: a tile half a row down still blocks its neighbour.
    Board offset(parseLayout("Offset", 1, { { "#v#" } }));
    CHECK(offset.isFree(0) && !offset.isFree(1) && offset.isFree(2));
}

// Plays the solver's answer move by move, checking each move is legal.
bool playsOut(Board board, const Solution& solution)
{
    for (const auto& [a, b] : solution.moves)
        if (!board.removePair(a, b))
            return false;
    return board.cleared();
}

void everyLayoutDealsSolvably()
{
    for (const Layout& layout : layouts()) {
        for (std::uint64_t seed = 1; seed <= 12; ++seed) {
            Board board(layout);
            Plan plan;
            CHECK(deal(board, seed, &plan));
            CHECK(planWorks(board, plan));
            std::map<int, int> groups;
            for (int i = 0; i < board.size(); ++i) {
                CHECK(board.kind(i) >= 0 && board.kind(i) < KindCount);
                ++groups[matchGroup(board.kind(i))];
            }
            for (const auto& [group, count] : groups)
                CHECK(count % 2 == 0);
            const Solution solution = solve(board, 500000);
            if (solution.outcome != Solution::Solved)
                std::fprintf(stderr, "  %s seed %llu: not solved\n", layout.name.c_str(), (unsigned long long)seed);
            CHECK(solution.outcome == Solution::Solved);
            CHECK(playsOut(board, solution));
        }
    }
}

void dealsAreDeterministic()
{
    Board a(*findLayout("Turtle"));
    Board b(*findLayout("Turtle"));
    CHECK(deal(a, 42));
    CHECK(deal(b, 42));
    CHECK(a.kinds() == b.kinds());
    Board c(*findLayout("Turtle"));
    CHECK(deal(c, 43));
    CHECK(a.kinds() != c.kinds());
}

void reshuffleKeepsTilesAndStaysSolvable()
{
    Board board(*findLayout("Pyramid"));
    CHECK(deal(board, 7));
    // Play a few moves, then shuffle what's left.
    for (int i = 0; i < 10; ++i) {
        const auto pairs = board.availablePairs();
        CHECK(!pairs.empty());
        if (!pairs.empty())
            board.removePair(pairs.front().first, pairs.front().second);
    }
    std::map<int, int> before;
    for (int i = 0; i < board.size(); ++i)
        if (board.present(i))
            ++before[board.kind(i)];
    calm::Rng rng(99);
    Plan plan;
    CHECK(reshuffle(board, rng, &plan));
    CHECK(planWorks(board, plan));
    std::map<int, int> after;
    for (int i = 0; i < board.size(); ++i)
        if (board.present(i))
            ++after[board.kind(i)];
    CHECK(before == after);
    CHECK(solve(board, 500000).outcome == Solution::Solved);

    // Two tiles stacked up can never be cleared, however they're arranged.
    Board stack(parseLayout("Stack", 1, { { "#" }, { "#" } }));
    stack.setKinds({ 3, 3 });
    CHECK(!reshuffle(stack, rng));
    CHECK(solve(stack).outcome == Solution::Unsolvable);
}

} // namespace

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        { "tile kinds and matching", tileKindsAndMatching },
        { "layouts are well formed", layoutsAreWellFormed },
        { "free tile rules", freeTileRules },
        { "every layout deals solvably", everyLayoutDealsSolvably },
        { "deals are deterministic", dealsAreDeterministic },
        { "reshuffle keeps tiles and stays solvable", reshuffleKeepsTilesAndStaysSolvable },
    };
    for (const auto& [name, fn] : tests) {
        const int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", name);
    }
    std::printf("%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
