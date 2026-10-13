// Dependency-free tests for the Klondike engine, run by CTest.

#include <solitaire/Klondike.h>
#include <solitaire/Solver.h>

#include <chrono>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace solitaire;

namespace {

int g_failures = 0;

#define CHECK(expr)                                                                   \
    do {                                                                              \
        if (!(expr)) {                                                                \
            std::fprintf(stderr, "  FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

constexpr PileRef Waste { PileKind::Waste, 0 };
constexpr PileRef col(int c) { return { PileKind::Tableau, c }; }
constexpr PileRef home(Suit s) { return { PileKind::Foundation, int(s) }; }

// A hand-built position: "1|*|waste|f0..f3|t0..t6", where "*" in the stock
// field stands for every card not placed elsewhere, face down.
Klondike position(std::string text)
{
    if (const auto star = text.find('*'); star != std::string::npos) {
        const std::string ranks = "A23456789TJQK";
        const std::string suits = "shdc";
        std::string rest;
        for (char s : suits) {
            for (char r : ranks) {
                const std::string down { r, s };
                const std::string up { r, char(s - 'a' + 'A') };
                if (text.find(down) == std::string::npos && text.find(up) == std::string::npos)
                    rest += down;
            }
        }
        text.replace(star, 1, rest);
    }
    auto game = Klondike::parse(text);
    if (!game) {
        std::fprintf(stderr, "  bad position: %s\n", text.c_str());
        ++g_failures;
        return {};
    }
    return *game;
}

void dealLayout()
{
    const Klondike game = Klondike::deal(42, 1);
    CHECK(game.stock().size() == 24);
    CHECK(game.waste().empty());
    std::set<int> ids;
    for (const Card& card : game.stock()) {
        CHECK(!card.faceUp);
        ids.insert(card.id);
    }
    for (int c = 0; c < TableauCount; ++c) {
        const Pile& column = game.tableau(c);
        CHECK(int(column.size()) == c + 1);
        for (int i = 0; i < int(column.size()); ++i) {
            CHECK(column[i].faceUp == (i == c));
            ids.insert(column[i].id);
        }
    }
    CHECK(ids.size() == 52);
    CHECK(Klondike::deal(42, 1) == game);
    CHECK(!(Klondike::deal(43, 1) == game));
}

void serializeRoundTrip()
{
    Klondike game = Klondike::deal(7, 3);
    game.draw();
    const auto text = game.serialize();
    const auto back = Klondike::parse(text);
    CHECK(back.has_value());
    CHECK(back && *back == game);
    CHECK(back && back->drawCount() == 3);
    CHECK(!Klondike::parse("2" + text.substr(1)));
    CHECK(!Klondike::parse(text.substr(0, text.size() - 2)));
    CHECK(!Klondike::parse("garbage"));
}

void drawAndRecycle()
{
    Klondike one = Klondike::deal(1, 1);
    const Card top = one.stock().back();
    CHECK(one.draw() == DrawResult::Drew);
    CHECK(one.waste().size() == 1 && one.waste().back().id == top.id && one.waste().back().faceUp);

    Klondike three = Klondike::deal(1, 3);
    for (int i = 0; i < 8; ++i)
        CHECK(three.draw() == DrawResult::Drew);
    CHECK(three.stock().empty() && three.waste().size() == 24);
    CHECK(three.draw() == DrawResult::Recycled);
    CHECK(three.stock().size() == 24 && three.waste().empty());
    // Recycling restores the original order.
    CHECK(three.stock() == Klondike::deal(1, 3).stock());
}

void moveRules()
{
    // Waste: 7h. Tableau: 8s on column 0, Kd alone in 1, a face-down card
    // under the 6c in column 2, column 3 empty.
    Klondike game = position(
        "1|*|7H|||||8S|KD|7c6C||||");
    CHECK(game.canMove({ Waste, 0, col(0) }));                  // red 7 on black 8
    CHECK(!game.canMove({ Waste, 0, col(1) }));                 // not on a king
    CHECK(!game.canMove({ Waste, 0, home(Suit::Hearts) }));     // needs ace first
    CHECK(game.canMove({ col(1), 0, col(3) }));                 // king to empty
    CHECK(!game.canMove({ col(2), 0, col(0) }));                // face-down card
    CHECK(!game.canMove({ col(2), 1, col(3) }));                // only kings fill gaps

    const MoveResult r = game.move({ Waste, 0, col(0) });
    CHECK(r.ok && !r.flipped);
    CHECK(game.tableau(0).size() == 2);
    // 6c onto the 7h: column 2's hidden card turns over.
    const MoveResult flip = game.move({ col(2), 1, col(0) });
    CHECK(flip.ok && flip.flipped);
    CHECK(game.tableau(2).back().faceUp);
    // The run 8s-7h-6c moves as one.
    CHECK(!game.canMove({ col(0), 0, col(1) })); // 8 doesn't go on a K
    CHECK(game.tableau(0).size() == 3);
}

void foundationsAndBestMove()
{
    Klondike game = position(
        "1|*|AH|AS||||2S|||||Ad|Ac");
    // Tapping the ace of hearts sends it home.
    auto best = game.bestMove(Waste, 0);
    CHECK(best && best->to == home(Suit::Hearts));
    CHECK(game.move(*best).toFoundation);
    // The two of spades follows its ace.
    best = game.bestMove(col(0), 0);
    CHECK(best && best->to == home(Suit::Spades));
    game.move(*best);
    CHECK(game.foundationCards() == 3);
    // Wrong suit's foundation is refused.
    const Klondike stuck = position(
        "1|*|AH|AS||||2S|||||Ad|Ac");
    CHECK(!stuck.canMove({ Waste, 0, home(Suit::Spades) }));
}

void hintsAndAutoFinish()
{
    const Klondike opening = Klondike::deal(99, 1);
    CHECK(opening.hint().has_value());

    // Everything face up and nothing hidden: auto-finish plays it out.
    Klondike game = position(
        "1|||AS2S3S4S5S6S7S8S9STSJS|AH2H3H4H5H6H7H8H9HTHJH|AD2D3D4D5D6D7D8D9DTDJD|AC2C3C4C5C6C7C8C9CTCJC|KSQH|KHQS|KDQC|KCQD|||");
    CHECK(game.canAutoFinish());
    int steps = 0;
    while (auto step = game.autoFinishStep()) {
        CHECK(!step->draw);
        CHECK(game.move(step->move).toFoundation);
        if (++steps > 52)
            break;
    }
    CHECK(game.won());
    CHECK(steps == 8);
    CHECK(!game.canAutoFinish());
}

void solverAndDeals()
{
    // One move from winning.
    const Klondike nearly = position(
        "1|||AS2S3S4S5S6S7S8S9STSJSQS|AH2H3H4H5H6H7H8H9HTHJHQHKH|AD2D3D4D5D6D7D8D9DTDJDQDKD|AC2C3C4C5C6C7C8C9CTCJCQCKC|KS||||||");
    CHECK(solve(nearly) == Verdict::Winnable);

    // Proven deals really are winnable, and most random draw-one deals are.
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    int proven = 0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        const Deal deal = dealWinnable(seed * 7919, 1);
        proven += deal.proven;
        CHECK(deal.game.stock().size() == 24);
        if (seed <= 3) {
            // The solver's line really wins when played out.
            const auto path = solvePath(deal.game, 150000);
            CHECK(path.has_value());
            Klondike game = deal.game;
            for (const Hint& step : path.value_or(std::vector<Hint> {})) {
                if (step.draw)
                    CHECK(game.draw() != DrawResult::Nothing);
                else
                    CHECK(game.move(step.move).ok);
            }
            CHECK(game.won());
        }
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
    std::printf("  20 draw-one winnable deals: %d proven in %lld ms\n", proven, static_cast<long long>(ms));
    CHECK(proven == 20);

    int provenThree = 0;
    for (std::uint64_t seed = 1; seed <= 5; ++seed)
        provenThree += dealWinnable(seed * 104729, 3).proven;
    std::printf("  5 draw-three winnable deals: %d proven\n", provenThree);
    CHECK(provenThree == 5);
}

} // namespace

int main()
{
    struct Test {
        const char* name;
        void (*run)();
    };
    const Test tests[] = {
        { "dealLayout", dealLayout },
        { "serializeRoundTrip", serializeRoundTrip },
        { "drawAndRecycle", drawAndRecycle },
        { "moveRules", moveRules },
        { "foundationsAndBestMove", foundationsAndBestMove },
        { "hintsAndAutoFinish", hintsAndAutoFinish },
        { "solverAndDeals", solverAndDeals },
    };
    for (const Test& test : tests) {
        std::printf("%s\n", test.name);
        test.run();
    }
    if (g_failures)
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    else
        std::printf("all passed\n");
    return g_failures ? 1 : 0;
}
