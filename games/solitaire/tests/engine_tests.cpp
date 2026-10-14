// Dependency-free tests for the Solitaire engine, run by CTest.

#include <solitaire/Game.h>
#include <solitaire/Solver.h>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <optional>
#include <set>
#include <string>
#include <utility>
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
Game position(std::string text)
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
    auto game = Game::parse(text);
    if (!game) {
        std::fprintf(stderr, "  bad position: %s\n", text.c_str());
        ++g_failures;
        return {};
    }
    return *game;
}

void dealLayout()
{
    const Game game = Game::deal(Variant::Klondike, 42, 1);
    CHECK(game.stock().size() == 24);
    CHECK(game.waste().empty());
    std::set<int> ids;
    for (const Card& card : game.stock()) {
        CHECK(!card.faceUp);
        ids.insert(card.id);
    }
    for (int c = 0; c < 7; ++c) {
        const Pile& column = game.tableau(c);
        CHECK(int(column.size()) == c + 1);
        for (int i = 0; i < int(column.size()); ++i) {
            CHECK(column[i].faceUp == (i == c));
            ids.insert(column[i].id);
        }
    }
    CHECK(ids.size() == 52);
    CHECK(Game::deal(Variant::Klondike, 42, 1) == game);
    CHECK(!(Game::deal(Variant::Klondike, 43, 1) == game));
}

void serializeRoundTrip()
{
    Game game = Game::deal(Variant::Klondike, 7, 3);
    game.draw();
    const auto text = game.serialize();
    const auto back = Game::parse(text);
    CHECK(back.has_value());
    CHECK(back && *back == game);
    CHECK(back && back->drawCount() == 3);
    CHECK(!Game::parse("2" + text.substr(1)));
    CHECK(!Game::parse(text.substr(0, text.size() - 2)));
    CHECK(!Game::parse("garbage"));
}

void drawAndRecycle()
{
    Game one = Game::deal(Variant::Klondike, 1, 1);
    const Card top = one.stock().back();
    CHECK(one.draw() == DrawResult::Drew);
    CHECK(one.waste().size() == 1 && one.waste().back().id == top.id && one.waste().back().faceUp);

    Game three = Game::deal(Variant::Klondike, 1, 3);
    for (int i = 0; i < 8; ++i)
        CHECK(three.draw() == DrawResult::Drew);
    CHECK(three.stock().empty() && three.waste().size() == 24);
    CHECK(three.draw() == DrawResult::Recycled);
    CHECK(three.stock().size() == 24 && three.waste().empty());
    // Recycling restores the original order.
    CHECK(three.stock() == Game::deal(Variant::Klondike, 1, 3).stock());
}

void moveRules()
{
    // Waste: 7h. Tableau: 8s on column 0, Kd alone in 1, a face-down card
    // under the 6c in column 2, column 3 empty.
    Game game = position(
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
    Game game = position(
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
    const Game stuck = position(
        "1|*|AH|AS||||2S|||||Ad|Ac");
    CHECK(!stuck.canMove({ Waste, 0, home(Suit::Spades) }));

    // Otherwise the leftmost column it fits on, empty or not.
    const Game columns = position("1|*|6H|||||KD|8S|7S||Kc|KH|7C");
    best = columns.bestMove(Waste, 0);
    CHECK(best && best->to == col(2));
    best = position("1|*|KH|||||8S||7S|||Kc|").bestMove(Waste, 0);
    CHECK(best && best->to == col(1)); // the leftmost empty column
    // A king alone in its column stays put, and some cards fit nowhere.
    CHECK(!columns.bestMove(col(0), 0));
    CHECK(!columns.bestMove(col(5), 0));
    CHECK(!columns.bestMove(col(1), 0));
}

void hintsAndAutoFinish()
{
    const Game opening = Game::deal(Variant::Klondike, 99, 1);
    CHECK(opening.hint().has_value());

    // Everything face up and nothing hidden: auto-finish plays it out.
    Game game = position(
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
    const Game nearly = position(
        "1|||AS2S3S4S5S6S7S8S9STSJSQS|AH2H3H4H5H6H7H8H9HTHJHQHKH|AD2D3D4D5D6D7D8D9DTDJDQDKD|AC2C3C4C5C6C7C8C9CTCJCQCKC|KS||||||");
    CHECK(solve(nearly) == Verdict::Winnable);
    CHECK(solveExhaustive(nearly) == Verdict::Winnable);

    // Winning needs the king of hearts brought back down to an empty column
    // to hold the queen of spades. The quick search skips that move; the
    // exhaustive one finds it.
    const Game comeback = position(
        "1|||AS2S3S4S5S6S7S8S9STS|AH2H3H4H5H6H7H8H9HTHJHQHKH|AD2D3D4D5D6D7D8D9DTDJDQDKD|AC2C3C4C5C6C7C8C9CTCJCQCKC|JsQS|KS|||||");
    CHECK(solve(comeback) == Verdict::Unwinnable);
    CHECK(solveExhaustive(comeback) == Verdict::Winnable);

    // Found by random play: no sequence of moves wins from here.
    const Game dead = position(
        "1|6dTh6cQcJc|4CJHKS5H7C4D9D4S|AS2S|||AC2C|KCQDJSTD9S8D7S6H5C4H3S||6sQs9H8C|2dTc2hJD|KhAd9C8H|3d3c7hKd3H|Ts5sAh5dQh7d8S");
    CHECK(solveExhaustive(dead) == Verdict::Unwinnable);

    // A slice at a time reaches the same verdicts, and keeps them.
    auto stepped = [](const Game& game, int budget) {
        SteppedSolve search(game, budget);
        int slices = 0;
        std::optional<Verdict> verdict;
        while (!(verdict = search.step(500)))
            ++slices;
        CHECK(search.step(500) == verdict);
        return std::pair { *verdict, slices };
    };
    CHECK(stepped(nearly, 400000).first == Verdict::Winnable);
    CHECK(stepped(comeback, 400000).first == Verdict::Winnable);
    const auto [deadVerdict, deadSlices] = stepped(dead, 400000);
    CHECK(deadVerdict == Verdict::Unwinnable);
    CHECK(deadSlices > 1);
    CHECK(stepped(dead, 1000).first == Verdict::Unknown);

    // Proven deals really are winnable, and most random draw-one deals are.
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    int proven = 0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        const Deal deal = dealWinnable(Variant::Klondike, seed * 7919, 1);
        proven += deal.proven;
        CHECK(deal.game.stock().size() == 24);
        if (seed <= 3) {
            // The solver's line really wins when played out.
            const auto path = solvePath(deal.game, 150000);
            CHECK(path.has_value());
            Game game = deal.game;
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
        provenThree += dealWinnable(Variant::Klondike, seed * 104729, 3).proven;
    std::printf("  5 draw-three winnable deals: %d proven\n", provenThree);
    CHECK(provenThree == 5);
}

// A hand-built position of any variant, where "*" stands for every card of
// that variant's deck not placed elsewhere, face down (in the stock).
Game layout(Variant variant, int option, std::string text)
{
    const std::string ranks = "A23456789TJQK";
    const std::string suits = "SHDC";
    std::set<int> used;
    for (std::size_t i = 0; i + 1 < text.size(); ++i) {
        const auto rank = ranks.find(text[i]);
        const auto suit = suits.find(char(std::toupper(static_cast<unsigned char>(text[i + 1]))));
        if (rank == std::string::npos || suit == std::string::npos || text[i + 1] == '|')
            continue;
        int copy = 0;
        if (i + 3 < text.size() && text[i + 2] == '.')
            copy = text[i + 3] - '0';
        used.insert(makeCard(static_cast<Suit>(suit), int(rank) + 1, false, copy).id);
        i += copy ? 3 : 1;
    }
    std::string rest;
    for (int id : Game::deal(variant, 1, option).deck()) {
        if (used.count(id))
            continue;
        const Card card { static_cast<std::uint16_t>(id), false };
        rest += ranks[card.rank() - 1];
        rest += char(std::tolower(static_cast<unsigned char>(suits[int(card.suit())])));
        if (card.copy())
            rest += "." + std::to_string(card.copy());
    }
    if (const auto star = text.find('*'); star != std::string::npos)
        text.replace(star, 1, rest);
    auto game = Game::parse(text);
    if (!game) {
        std::fprintf(stderr, "  bad position: %s\n", text.c_str());
        ++g_failures;
        return {};
    }
    return *game;
}

int countCards(const Game& game)
{
    int total = int(game.stock().size() + game.waste().size());
    for (int i = 0; i < game.foundationCount(); ++i)
        total += int(game.foundation(i).size());
    for (int i = 0; i < game.tableauCount(); ++i)
        total += int(game.tableau(i).size());
    for (int i = 0; i < game.cellCount(); ++i)
        total += int(game.cell(i).size());
    return total;
}

void everyVariantDeals()
{
    const std::pair<Variant, int> all[] = {
        { Variant::Klondike, 1 }, { Variant::Klondike, 3 }, { Variant::FreeCell, 0 }, { Variant::Spider, 1 },
        { Variant::Spider, 2 }, { Variant::Spider, 4 }, { Variant::Yukon, 0 }, { Variant::Pyramid, 0 },
        { Variant::TriPeaks, 0 }, { Variant::Golf, 0 },
    };
    for (const auto& [variant, option] : all) {
        const Game game = Game::deal(variant, 5, option);
        CHECK(game.variant() == variant);
        CHECK(countCards(game) == game.deckSize());
        CHECK(int(game.deck().size()) == game.deckSize());
        const auto again = Game::parse(game.serialize());
        CHECK(again.has_value());
        CHECK(again && *again == game);
        CHECK(Game::deal(variant, 5, option) == game);
        CHECK(!game.won());
        CHECK(!game.legalMoves().empty());
        CHECK(variantFromKey(variantKey(variant)) == variant);
    }

    const Game freecell = Game::deal(Variant::FreeCell, 9);
    CHECK(freecell.tableau(0).size() == 7 && freecell.tableau(7).size() == 6);
    CHECK(freecell.cellCount() == 4 && !freecell.canDraw());

    const Game spider = Game::deal(Variant::Spider, 9, 2);
    CHECK(spider.deckSize() == 104 && spider.stock().size() == 50);
    CHECK(spider.tableau(0).size() == 6 && spider.tableau(9).size() == 5);
    CHECK(spider.tableau(0).back().faceUp && !spider.tableau(0).front().faceUp);
    for (int column = 0; column < 10; ++column)
        for (const Card& card : spider.tableau(column))
            CHECK(card.suit() == Suit::Spades || card.suit() == Suit::Hearts);

    const Game yukon = Game::deal(Variant::Yukon, 9);
    CHECK(yukon.tableau(0).size() == 1 && yukon.tableau(6).size() == 11);
    CHECK(!yukon.tableau(6)[5].faceUp && yukon.tableau(6)[6].faceUp);
    CHECK(yukon.stock().empty());

    const Game tripeaks = Game::deal(Variant::TriPeaks, 9);
    CHECK(tripeaks.stock().size() == 23 && tripeaks.waste().size() == 1);
    CHECK(!tripeaks.tableau(0).back().faceUp && tripeaks.tableau(27).back().faceUp);

    const Game golf = Game::deal(Variant::Golf, 9);
    CHECK(golf.stock().size() == 16 && golf.waste().size() == 1 && golf.tableau(6).size() == 5);
    CHECK(golf.goal() == 35 && golf.progress() == 0);
}

void freeCellRules()
{
    // Three cells full and no empty column: two cards move at once.
    const std::string base = "F|0|*||||||JDTS9H8S|QC|KD|KH|KS|KC|QD|%|2S|3S|4S|";
    std::string full = base;
    full.replace(full.find('%'), 1, "QH");
    const Game crowded = layout(Variant::FreeCell, 0, full);
    CHECK(crowded.canPick(col(0), 0));
    CHECK(!crowded.canMove({ col(0), 0, col(1) }));
    CHECK(crowded.canMove({ col(0), 2, { PileKind::Tableau, 7 } }) == false); // 9H onto QH: no
    // An empty column doubles it.
    std::string open = base;
    open.replace(open.find('%'), 1, "");
    Game roomy = layout(Variant::FreeCell, 0, open);
    CHECK(roomy.canMove({ col(0), 0, col(1) }));
    CHECK(!roomy.canMove({ col(0), 0, col(7) })); // into the empty column: only two
    CHECK(roomy.canMove({ col(0), 2, col(7) }));
    // A lone card can go to a free cell, and back out onto a column.
    CHECK(roomy.move({ col(0), 3, { PileKind::Cell, 3 } }).ok);
    CHECK(!roomy.canMove({ col(0), 2, { PileKind::Cell, 0 } }));
    CHECK(roomy.canPick({ PileKind::Cell, 3 }, 0));
    CHECK(roomy.canMove({ { PileKind::Cell, 3 }, 0, col(7) }));
    // A double tap tries home, then a column, then a free cell.
    const auto best = roomy.bestMove(col(0), 2);
    CHECK(best && best->to == col(7));
}

void spiderRules()
{
    // A spade run from king to two, and the ace beside it: playing the ace
    // completes the run, which goes home and turns the card beneath over.
    Game game = layout(Variant::Spider, 1,
        "S1|0|*||||||||||5s.1KSQSJSTS9S8S7S6S5S4S3S2S|AS|7S.1|7S.2|7S.3|7S.4|7S.5|7S.6|7S.7|8S.19S.1");
    CHECK(game.canDraw());
    const MoveResult result = game.move({ col(1), 0, col(0) });
    CHECK(result.ok && result.toFoundation && result.flipped);
    CHECK(game.foundation(0).size() == 13);
    CHECK(game.tableau(0).size() == 1 && game.tableau(0).back().faceUp);
    CHECK(game.tableau(1).empty());
    // An empty column blocks dealing.
    CHECK(!game.canDraw());
    // Only same-suit runs move together; any card builds on the next rank.
    Game mixed = layout(Variant::Spider, 2,
        "S2|0|*||||||||||9S8H|9H8H.1|TS|TH|KS|KS.1|KH|KH.1|QS|QH");
    CHECK(!mixed.canPick(col(0), 0));
    CHECK(mixed.canPick(col(1), 0));
    CHECK(mixed.canMove({ col(1), 0, col(2) }));
    const auto best = mixed.bestMove(col(1), 0);
    CHECK(best && best->to == col(3)); // the ten of its own suit
}

void yukonRules()
{
    // Any face-up card moves with everything on it.
    Game game = layout(Variant::Yukon, 0, "Y|0|*||||||KS|9hQH2C5D|8C||||");
    CHECK(game.canPick(col(1), 1));
    CHECK(game.canMove({ col(1), 1, col(0) }));
    const MoveResult result = game.move({ col(1), 1, col(0) });
    CHECK(result.ok && result.flipped);
    CHECK(game.tableau(0).size() == 4 && game.tableau(1).size() == 1);
}

void pyramidRules()
{
    // Rows of the pyramid, apex first; kings go alone, other pairs make 13.
    std::string text = "P|0|*|5H|";
    text += "|"; // the discard pile
    const std::string places[28] = { "2H", "3H", "4H", "6H", "7H", "8H", "9H", "TH", "JH", "QH", "AS", "2S", "3S", "4S", "5S",
        "6S", "7S", "8S", "9S", "TS", "JS", "QS", "KS", "AD", "2D", "3D", "4D", "8D" };
    for (int i = 0; i < 28; ++i)
        text += places[i] + std::string(i < 27 ? "|" : "");
    Game game = layout(Variant::Pyramid, 0, text);
    CHECK(game.uncovered(27) && !game.uncovered(20));
    CHECK(game.canMove({ col(27), 0, { PileKind::Waste, 0 } })); // 8 + 5
    CHECK(!game.canMove({ col(26), 0, col(27) })); // 4 + 8
    CHECK(game.canMove({ col(22), 0, { PileKind::Foundation, 0 } })); // king
    CHECK(game.move({ col(27), 0, { PileKind::Waste, 0 } }).ok);
    CHECK(game.foundation(0).size() == 2 && game.waste().empty() && game.progress() == 1);
    CHECK(game.move({ col(22), 0, { PileKind::Foundation, 0 } }).ok);
    // Three passes through the stock, then no more.
    Game passes = Game::deal(Variant::Pyramid, 3);
    int turns = 0;
    while (passes.canDraw()) {
        passes.draw();
        ++turns;
    }
    CHECK(turns == 3 * 24 + 2);
    CHECK(passes.passesLeft() == 0);
}

void peaksAndGolfRules()
{
    Game peaks = Game::deal(Variant::TriPeaks, 11);
    // Clearing both cards over a face-down card turns it over.
    const Game opening = peaks;
    bool flipped = false;
    for (int guard = 0; guard < 200 && !flipped; ++guard) {
        const auto moves = peaks.legalMoves();
        if (moves.empty())
            break;
        const Hint& h = moves.front();
        if (h.draw)
            peaks.draw();
        else
            flipped = peaks.move(h.move).flipped;
    }
    CHECK(flipped || peaks.stock().empty());
    // Ranks wrap from king to ace in TriPeaks, but not in Golf, where
    // nothing goes on a king.
    std::string tp = "T|0|*|KH|";
    for (int i = 0; i < 27; ++i)
        tp += "|";
    tp += "AS";
    Game wrap = layout(Variant::TriPeaks, 0, tp);
    CHECK(wrap.canMove({ col(27), 0, { PileKind::Waste, 0 } }));
    const Game golf = layout(Variant::Golf, 0, "G|0|*|KH||||||QS|AS");
    CHECK(!golf.canMove({ col(5), 0, { PileKind::Waste, 0 } }));
    CHECK(!golf.canMove({ col(6), 0, { PileKind::Waste, 0 } }));
    const Game golf2 = layout(Variant::Golf, 0, "G|0|*|QH||||||KS|JS");
    CHECK(golf2.canMove({ col(5), 0, { PileKind::Waste, 0 } }));
    CHECK(golf2.canMove({ col(6), 0, { PileKind::Waste, 0 } }));
    CHECK(golf2.bestMove(col(6), 0).has_value());
}

void variantSolver()
{
    const Variant solvable[] = { Variant::FreeCell, Variant::Yukon, Variant::Pyramid, Variant::TriPeaks, Variant::Golf };
    for (Variant variant : solvable) {
        int proven = 0;
        for (std::uint64_t seed = 1; seed <= 3; ++seed) {
            const Deal deal = dealWinnable(variant, seed * 104729, 0, 12);
            if (!deal.proven)
                continue;
            ++proven;
            const auto path = solvePath(deal.game, 150000);
            CHECK(path.has_value());
            Game game = deal.game;
            for (const Hint& step : path.value_or(std::vector<Hint> {})) {
                if (step.draw)
                    CHECK(game.draw() != DrawResult::Nothing);
                else
                    CHECK(game.move(step.move).ok);
            }
            CHECK(game.won());
        }
        std::printf("  %s: %d of 3 deals proven\n", std::string(variantKey(variant)).c_str(), proven);
        CHECK(proven >= 2);
    }
    // Spider is dealt as it comes.
    CHECK(!dealWinnable(Variant::Spider, 1, 1).proven);
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
        { "everyVariantDeals", everyVariantDeals },
        { "freeCellRules", freeCellRules },
        { "spiderRules", spiderRules },
        { "yukonRules", yukonRules },
        { "pyramidRules", pyramidRules },
        { "peaksAndGolfRules", peaksAndGolfRules },
        { "variantSolver", variantSolver },
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
