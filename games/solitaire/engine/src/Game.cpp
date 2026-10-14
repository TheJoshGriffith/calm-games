#include <solitaire/Game.h>

#include <calm/Rng.h>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace solitaire {

namespace {

constexpr std::string_view Ranks = "A23456789TJQK";
constexpr std::string_view SuitsDown = "shdc";
constexpr std::string_view SuitsUp = "SHDC";

constexpr int PyramidRows = 7;
constexpr int PeakPlaces = 28;
constexpr int PyramidPasses = 3;

int topRank(const Pile& pile)
{
    return pile.empty() ? 0 : pile.back().rank();
}

struct Shape {
    int foundations;
    int tableau;
    int cells;
};

Shape shapeOf(Variant variant)
{
    switch (variant) {
    case Variant::Klondike:
    case Variant::Yukon:
        return { SuitCount, 7, 0 };
    case Variant::FreeCell:
        return { SuitCount, 8, 4 };
    case Variant::Spider:
        return { 8, 10, 0 };
    case Variant::Pyramid:
        return { 1, PeakPlaces, 0 };
    case Variant::TriPeaks:
        return { 0, PeakPlaces, 0 };
    case Variant::Golf:
        return { 0, 7, 0 };
    }
    return { 0, 0, 0 };
}

// The places lying on top of a Pyramid or TriPeaks place (up to two).
std::array<int, 2> coverers(Variant variant, int index)
{
    if (variant == Variant::Pyramid) {
        int row = 0;
        while ((row + 1) * (row + 2) / 2 <= index)
            ++row;
        if (row == PyramidRows - 1)
            return { -1, -1 };
        const int column = index - row * (row + 1) / 2;
        const int below = (row + 1) * (row + 2) / 2 + column;
        return { below, below + 1 };
    }
    // TriPeaks rows: 3 peak tops (0-2), 6 (3-8), 9 (9-17), then 10 (18-27).
    if (index < 3)
        return { 3 + 2 * index, 4 + 2 * index };
    if (index < 9) {
        const int j = index - 3;
        const int below = 9 + (j / 2) * 3 + j % 2;
        return { below, below + 1 };
    }
    if (index < 18)
        return { index + 9, index + 10 };
    return { -1, -1 };
}

bool usesPlaces(Variant variant)
{
    return variant == Variant::Pyramid || variant == Variant::TriPeaks;
}

// Klondike, FreeCell and Yukon build foundations by suit from the ace.
bool suitFoundations(Variant variant)
{
    return variant == Variant::Klondike || variant == Variant::FreeCell || variant == Variant::Yukon;
}

} // namespace

std::string_view variantKey(Variant variant)
{
    switch (variant) {
    case Variant::Klondike:
        return "klondike";
    case Variant::FreeCell:
        return "freecell";
    case Variant::Spider:
        return "spider";
    case Variant::Yukon:
        return "yukon";
    case Variant::Pyramid:
        return "pyramid";
    case Variant::TriPeaks:
        return "tripeaks";
    case Variant::Golf:
        return "golf";
    }
    return {};
}

std::optional<Variant> variantFromKey(std::string_view key)
{
    for (int v = 0; v < VariantCount; ++v)
        if (variantKey(static_cast<Variant>(v)) == key)
            return static_cast<Variant>(v);
    return std::nullopt;
}

std::vector<int> Game::deck() const
{
    std::vector<int> ids;
    if (m_variant != Variant::Spider) {
        for (int i = 0; i < DeckSize; ++i)
            ids.push_back(i);
        return ids;
    }
    const int suits = m_option;
    const int copies = 8 / suits;
    for (int copy = 0; copy < copies; ++copy)
        for (int suit = 0; suit < suits; ++suit)
            for (int rank = 1; rank <= RankCount; ++rank)
                ids.push_back(makeCard(static_cast<Suit>(suit), rank, false, copy).id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

Game Game::deal(Variant variant, std::uint64_t seed, int option)
{
    Game game;
    game.m_variant = variant;
    switch (variant) {
    case Variant::Klondike:
        game.m_option = option == 3 ? 3 : 1;
        break;
    case Variant::Spider:
        game.m_option = option == 2 || option == 4 ? option : 1;
        break;
    default:
        game.m_option = 0;
        break;
    }
    const Shape shape = shapeOf(variant);
    game.m_foundations.resize(shape.foundations);
    game.m_tableau.resize(shape.tableau);
    game.m_cells.resize(shape.cells);

    std::vector<int> deck = game.deck();
    calm::Rng rng(seed);
    rng.shuffle(deck.begin(), deck.end());
    std::size_t next = 0;
    auto take = [&](bool faceUp) { return Card { static_cast<std::uint16_t>(deck[next++]), faceUp }; };

    switch (variant) {
    case Variant::Klondike:
        for (int row = 0; row < 7; ++row)
            for (int column = row; column < 7; ++column)
                game.m_tableau[column].push_back(take(column == row));
        break;
    case Variant::FreeCell:
        for (int i = 0; i < DeckSize; ++i)
            game.m_tableau[i % 8].push_back(take(true));
        break;
    case Variant::Spider:
        for (int column = 0; column < 10; ++column) {
            const int count = column < 4 ? 6 : 5;
            for (int i = 0; i < count; ++i)
                game.m_tableau[column].push_back(take(i == count - 1));
        }
        break;
    case Variant::Yukon:
        game.m_tableau[0].push_back(take(true));
        for (int column = 1; column < 7; ++column)
            for (int i = 0; i < column + 5; ++i)
                game.m_tableau[column].push_back(take(i >= column));
        break;
    case Variant::Pyramid:
        for (int place = 0; place < PeakPlaces; ++place)
            game.m_tableau[place].push_back(take(true));
        break;
    case Variant::TriPeaks:
        for (int place = 0; place < PeakPlaces; ++place)
            game.m_tableau[place].push_back(take(place >= 18));
        break;
    case Variant::Golf:
        for (int row = 0; row < 5; ++row)
            for (int column = 0; column < 7; ++column)
                game.m_tableau[column].push_back(take(true));
        break;
    }
    while (next < deck.size())
        game.m_stock.push_back(take(false));
    if (variant == Variant::TriPeaks || variant == Variant::Golf) {
        Card first = game.m_stock.back();
        game.m_stock.pop_back();
        first.faceUp = true;
        game.m_waste.push_back(first);
    }
    return game;
}

int Game::passesLeft() const noexcept
{
    if (m_variant == Variant::Klondike)
        return -1;
    if (m_variant == Variant::Pyramid)
        return PyramidPasses - 1 - m_recycles;
    return 0;
}

bool Game::validRef(PileRef ref) const
{
    switch (ref.kind) {
    case PileKind::Stock:
    case PileKind::Waste:
        return ref.index == 0;
    case PileKind::Foundation:
        return ref.index >= 0 && ref.index < foundationCount();
    case PileKind::Tableau:
        return ref.index >= 0 && ref.index < tableauCount();
    case PileKind::Cell:
        return ref.index >= 0 && ref.index < cellCount();
    }
    return false;
}

const Pile& Game::pile(PileRef ref) const
{
    switch (ref.kind) {
    case PileKind::Stock:
        return m_stock;
    case PileKind::Waste:
        return m_waste;
    case PileKind::Foundation:
        return m_foundations.at(ref.index);
    case PileKind::Tableau:
        return m_tableau.at(ref.index);
    case PileKind::Cell:
        return m_cells.at(ref.index);
    }
    throw std::out_of_range("pile");
}

Pile& Game::mutablePile(PileRef ref)
{
    return const_cast<Pile&>(pile(ref));
}

bool Game::canDraw() const
{
    switch (m_variant) {
    case Variant::Klondike:
        return !m_stock.empty() || !m_waste.empty();
    case Variant::Spider: {
        if (m_stock.empty())
            return false;
        // Every column needs a card first, unless there are too few left.
        int cards = 0;
        bool gap = false;
        for (const Pile& column : m_tableau) {
            cards += int(column.size());
            gap = gap || column.empty();
        }
        return !gap || cards < tableauCount();
    }
    case Variant::Pyramid:
        return !m_stock.empty() || (!m_waste.empty() && passesLeft() > 0);
    case Variant::TriPeaks:
    case Variant::Golf:
        return !m_stock.empty();
    case Variant::FreeCell:
    case Variant::Yukon:
        return false;
    }
    return false;
}

DrawResult Game::draw()
{
    if (!canDraw())
        return DrawResult::Nothing;
    if (m_variant == Variant::Spider) {
        for (Pile& column : m_tableau) {
            if (m_stock.empty())
                break;
            Card card = m_stock.back();
            m_stock.pop_back();
            card.faceUp = true;
            column.push_back(card);
        }
        MoveResult ignored;
        settle(ignored);
        return DrawResult::Drew;
    }
    if (!m_stock.empty()) {
        for (int i = 0; i < drawCount() && !m_stock.empty(); ++i) {
            Card card = m_stock.back();
            m_stock.pop_back();
            card.faceUp = true;
            m_waste.push_back(card);
        }
        return DrawResult::Drew;
    }
    while (!m_waste.empty()) {
        Card card = m_waste.back();
        m_waste.pop_back();
        card.faceUp = false;
        m_stock.push_back(card);
    }
    if (m_variant == Variant::Pyramid)
        ++m_recycles;
    return DrawResult::Recycled;
}

bool Game::uncovered(int index) const
{
    if (!usesPlaces(m_variant) || index < 0 || index >= tableauCount() || m_tableau[index].empty())
        return false;
    for (int over : coverers(m_variant, index))
        if (over >= 0 && !m_tableau[over].empty())
            return false;
    return true;
}

bool Game::runFrom(const Pile& pile, int start) const
{
    for (int i = start + 1; i < int(pile.size()); ++i) {
        const Card& below = pile[i - 1];
        const Card& card = pile[i];
        if (!card.faceUp || card.rank() != below.rank() - 1)
            return false;
        if (m_variant == Variant::Spider ? card.suit() != below.suit() : card.red() == below.red())
            return false;
    }
    return true;
}

bool Game::canPick(PileRef from, int start) const
{
    if (!validRef(from))
        return false;
    const Pile& source = pile(from);
    if (start < 0 || start >= int(source.size()) || !source[start].faceUp)
        return false;
    const bool top = start == int(source.size()) - 1;
    switch (from.kind) {
    case PileKind::Stock:
        return false;
    case PileKind::Waste:
        return top && m_variant != Variant::TriPeaks && m_variant != Variant::Golf;
    case PileKind::Cell:
        return top;
    case PileKind::Foundation:
        return top && (m_variant == Variant::Klondike || m_variant == Variant::Yukon);
    case PileKind::Tableau:
        switch (m_variant) {
        case Variant::Klondike:
        case Variant::Yukon:
            return true;
        case Variant::FreeCell:
        case Variant::Spider:
            return runFrom(source, start);
        case Variant::Pyramid:
        case Variant::TriPeaks:
            return uncovered(from.index);
        case Variant::Golf:
            return top;
        }
    }
    return false;
}

int Game::maxRun(bool toEmpty) const
{
    int cells = 0;
    for (const Pile& c : m_cells)
        cells += c.empty();
    int empty = 0;
    for (const Pile& column : m_tableau)
        empty += column.empty();
    if (toEmpty)
        --empty;
    return (cells + 1) << std::max(0, empty);
}

bool Game::fitsOn(const Card& card, PileRef to) const
{
    if (!validRef(to))
        return false;
    const Pile& target = pile(to);
    switch (to.kind) {
    case PileKind::Foundation:
        if (suitFoundations(m_variant))
            return int(card.suit()) == to.index && card.rank() == topRank(target) + 1;
        if (m_variant == Variant::Pyramid)
            return card.rank() == RankCount;
        return false;
    case PileKind::Cell:
        return target.empty();
    case PileKind::Waste: {
        if (m_variant != Variant::TriPeaks && m_variant != Variant::Golf)
            return false;
        if (target.empty())
            return false;
        const int top = target.back().rank();
        if (m_variant == Variant::Golf)
            return top != RankCount && std::abs(top - card.rank()) == 1;
        const int gap = std::abs(top - card.rank());
        return gap == 1 || gap == RankCount - 1;
    }
    case PileKind::Tableau: {
        if (target.empty()) {
            if (m_variant == Variant::Klondike || m_variant == Variant::Yukon)
                return card.rank() == RankCount;
            return m_variant == Variant::FreeCell || m_variant == Variant::Spider;
        }
        const Card& top = target.back();
        if (!top.faceUp || top.rank() != card.rank() + 1)
            return false;
        switch (m_variant) {
        case Variant::Klondike:
        case Variant::Yukon:
        case Variant::FreeCell:
            return top.red() != card.red();
        case Variant::Spider:
            return true;
        default:
            return false;
        }
    }
    case PileKind::Stock:
        return false;
    }
    return false;
}

std::optional<Card> Game::pyramidCard(PileRef ref) const
{
    if (ref.kind == PileKind::Waste && !m_waste.empty())
        return m_waste.back();
    if (ref.kind == PileKind::Tableau && uncovered(ref.index))
        return m_tableau[ref.index].back();
    return std::nullopt;
}

bool Game::canMove(const Move& move) const
{
    if (move.from == move.to || !validRef(move.to) || !canPick(move.from, move.start))
        return false;
    const Pile& source = pile(move.from);
    const Card& card = source[move.start];
    const int count = int(source.size()) - move.start;

    if (m_variant == Variant::Pyramid) {
        if (move.to.kind == PileKind::Foundation)
            return card.rank() == RankCount;
        const auto partner = pyramidCard(move.to);
        return partner && partner->rank() + card.rank() == RankCount;
    }
    if (m_variant == Variant::TriPeaks || m_variant == Variant::Golf)
        return move.to.kind == PileKind::Waste && fitsOn(card, move.to);

    if (move.to.kind == PileKind::Stock || move.to.kind == PileKind::Waste)
        return false;
    if (move.from.kind == PileKind::Foundation && move.to.kind == PileKind::Foundation)
        return false;
    if (count > 1) {
        if (move.from.kind != PileKind::Tableau || move.to.kind != PileKind::Tableau)
            return false;
        if (m_variant == Variant::FreeCell && count > maxRun(pile(move.to).empty()))
            return false;
    }
    return fitsOn(card, move.to);
}

void Game::settle(MoveResult& result)
{
    if (m_variant == Variant::Spider) {
        for (Pile& column : m_tableau) {
            if (column.size() < std::size_t(RankCount))
                continue;
            const int start = int(column.size()) - RankCount;
            if (!column[start].faceUp || column[start].rank() != RankCount || !runFrom(column, start))
                continue;
            for (Pile& home : m_foundations) {
                if (home.empty()) {
                    home.assign(column.begin() + start, column.end());
                    column.erase(column.begin() + start, column.end());
                    result.toFoundation = true;
                    break;
                }
            }
        }
    }
    if (m_variant == Variant::TriPeaks) {
        for (int place = 0; place < tableauCount(); ++place) {
            Pile& p = m_tableau[place];
            if (!p.empty() && !p.back().faceUp && uncovered(place)) {
                p.back().faceUp = true;
                result.flipped = true;
            }
        }
        return;
    }
    if (m_variant == Variant::Klondike || m_variant == Variant::Yukon || m_variant == Variant::Spider) {
        for (Pile& column : m_tableau) {
            if (!column.empty() && !column.back().faceUp) {
                column.back().faceUp = true;
                result.flipped = true;
            }
        }
    }
}

MoveResult Game::move(const Move& move)
{
    MoveResult result;
    if (!canMove(move))
        return result;
    Pile& source = mutablePile(move.from);

    if (m_variant == Variant::Pyramid) {
        Pile& discard = m_foundations[0];
        discard.push_back(source.back());
        source.pop_back();
        if (move.to.kind != PileKind::Foundation) {
            Pile& partner = mutablePile(move.to);
            discard.push_back(partner.back());
            partner.pop_back();
        }
        result.ok = true;
        result.toFoundation = true;
        return result;
    }

    Pile& target = mutablePile(move.to);
    target.insert(target.end(), source.begin() + move.start, source.end());
    source.erase(source.begin() + move.start, source.end());
    result.ok = true;
    result.toFoundation = move.to.kind == PileKind::Foundation || move.to.kind == PileKind::Waste;
    settle(result);
    return result;
}

std::optional<Move> Game::bestMove(PileRef from, int start) const
{
    if (!canPick(from, start))
        return std::nullopt;
    const Pile& source = pile(from);
    const Card& card = source[start];

    if (m_variant == Variant::Pyramid) {
        const Move king { from, start, { PileKind::Foundation, 0 } };
        if (canMove(king))
            return king;
        const Move withWaste { from, start, { PileKind::Waste, 0 } };
        if (canMove(withWaste))
            return withWaste;
        for (int place = 0; place < tableauCount(); ++place) {
            const Move pair { from, start, { PileKind::Tableau, place } };
            if (canMove(pair))
                return pair;
        }
        return std::nullopt;
    }
    if (m_variant == Variant::TriPeaks || m_variant == Variant::Golf) {
        const Move play { from, start, { PileKind::Waste, 0 } };
        return canMove(play) ? std::optional<Move>(play) : std::nullopt;
    }

    const bool top = start == int(source.size()) - 1;
    if (from.kind != PileKind::Foundation && top) {
        for (int f = 0; f < foundationCount(); ++f) {
            const Move up { from, start, { PileKind::Foundation, f } };
            if (canMove(up))
                return up;
        }
    }

    // A whole column moved to an empty one gains nothing.
    auto pointless = [&](int column) {
        return m_tableau[column].empty() && from.kind == PileKind::Tableau && start == 0;
    };
    if (m_variant == Variant::Spider) {
        // Spider prefers building in suit, then on any card, then a space.
        for (int pass = 0; pass < 3; ++pass) {
            for (int column = 0; column < tableauCount(); ++column) {
                const Pile& target = m_tableau[column];
                if (pass == 0 && (target.empty() || target.back().suit() != card.suit()))
                    continue;
                if (pass == 1 && target.empty())
                    continue;
                const Move candidate { from, start, { PileKind::Tableau, column } };
                if (canMove(candidate) && !pointless(column))
                    return candidate;
            }
        }
        return std::nullopt;
    }

    for (int column = 0; column < tableauCount(); ++column) {
        const Move candidate { from, start, { PileKind::Tableau, column } };
        if (canMove(candidate) && !pointless(column))
            return candidate;
    }
    if (from.kind != PileKind::Cell) {
        for (int c = 0; c < cellCount(); ++c) {
            const Move park { from, start, { PileKind::Cell, c } };
            if (canMove(park))
                return park;
        }
    }
    return std::nullopt;
}

std::vector<Hint> Game::legalMoves() const
{
    std::vector<Hint> out;
    std::vector<PileRef> sources;
    sources.push_back({ PileKind::Waste, 0 });
    for (int i = 0; i < cellCount(); ++i)
        sources.push_back({ PileKind::Cell, i });
    for (int i = 0; i < tableauCount(); ++i)
        sources.push_back({ PileKind::Tableau, i });
    for (int i = 0; i < foundationCount(); ++i)
        sources.push_back({ PileKind::Foundation, i });

    std::vector<PileRef> targets;
    for (int i = 0; i < foundationCount(); ++i)
        targets.push_back({ PileKind::Foundation, i });
    targets.push_back({ PileKind::Waste, 0 });
    for (int i = 0; i < tableauCount(); ++i)
        targets.push_back({ PileKind::Tableau, i });
    for (int i = 0; i < cellCount(); ++i)
        targets.push_back({ PileKind::Cell, i });

    for (const PileRef& from : sources) {
        const Pile& source = pile(from);
        for (int start = 0; start < int(source.size()); ++start) {
            if (!canPick(from, start))
                continue;
            for (const PileRef& to : targets) {
                const Move m { from, start, to };
                // A Pyramid pair is the same either way round; keep one.
                if (m_variant == Variant::Pyramid && to.kind == PileKind::Tableau && from.kind == PileKind::Tableau
                    && to.index < from.index)
                    continue;
                if (m_variant == Variant::Pyramid && to.kind == PileKind::Waste && from.kind == PileKind::Waste)
                    continue;
                if (canMove(m))
                    out.push_back({ false, m });
            }
        }
    }
    if (canDraw())
        out.push_back({ true, {} });
    return out;
}

bool Game::isProductive(const Move& move) const
{
    if (move.from.kind == PileKind::Cell)
        return move.to.kind != PileKind::Cell;
    if (move.from.kind != PileKind::Tableau || move.to.kind != PileKind::Tableau)
        return false;
    const Pile& source = m_tableau[move.from.index];
    const Pile& target = m_tableau[move.to.index];
    if (move.start == 0)
        return !target.empty(); // frees a column
    const Card& below = source[move.start - 1];
    if (!below.faceUp)
        return true; // turns a card over
    const Card& card = source[move.start];
    if (m_variant == Variant::Spider) {
        // Joining a run of its own suit, from one that wasn't.
        return !target.empty() && target.back().suit() == card.suit()
            && (below.suit() != card.suit() || below.rank() != card.rank() + 1);
    }
    if (suitFoundations(m_variant)) {
        // Splitting a run only helps when it sends the card beneath home.
        return below.rank() == topRank(m_foundations[int(below.suit())]) + 1;
    }
    return false;
}

std::optional<Hint> Game::hint() const
{
    const std::vector<Hint> moves = legalMoves();
    // 1. Anything that goes home, pairs off or plays onto the waste.
    for (const Hint& h : moves)
        if (!h.draw && (h.move.to.kind == PileKind::Foundation || h.move.to.kind == PileKind::Waste)
            && h.move.from.kind != PileKind::Foundation)
            return h;
    if (m_variant == Variant::Pyramid) {
        for (const Hint& h : moves)
            if (!h.draw)
                return h;
    }
    // 2. Moves that turn a card over, free a column or build in suit.
    for (const Hint& h : moves)
        if (!h.draw && isProductive(h.move))
            return h;
    // 3. The waste card onto the tableau.
    if (!m_waste.empty()) {
        if (auto m = bestMove({ PileKind::Waste, 0 }, int(m_waste.size()) - 1); m && m->to.kind == PileKind::Tableau)
            return Hint { false, *m };
    }
    // 4. Turn the stock, if any card in it or the waste could be played.
    if (!canDraw())
        return std::nullopt;
    if (m_variant != Variant::Klondike)
        return Hint { true, {} };
    auto playable = [this](const Card& card) {
        if (fitsOn(card, { PileKind::Foundation, int(card.suit()) }))
            return true;
        for (int column = 0; column < tableauCount(); ++column)
            if (fitsOn(card, { PileKind::Tableau, column }))
                return true;
        return false;
    };
    const bool useful = std::any_of(m_stock.begin(), m_stock.end(), playable)
        || std::any_of(m_waste.begin(), m_waste.end() - (m_waste.empty() ? 0 : 1), playable);
    if (useful)
        return Hint { true, {} };
    return std::nullopt;
}

bool Game::canAutoFinish() const
{
    if (won())
        return false;
    switch (m_variant) {
    case Variant::Klondike:
        for (const Pile& column : m_tableau)
            for (const Card& card : column)
                if (!card.faceUp)
                    return false;
        return true;
    case Variant::FreeCell:
    case Variant::Yukon:
        // Each column runs down to its top card, so the lowest card left is
        // always free to go home.
        for (const Pile& column : m_tableau)
            for (std::size_t i = 0; i < column.size(); ++i)
                if (!column[i].faceUp || (i > 0 && column[i].rank() > column[i - 1].rank()))
                    return false;
        return true;
    default:
        return false;
    }
}

std::optional<Hint> Game::autoFinishStep() const
{
    if (!canAutoFinish())
        return std::nullopt;
    std::optional<Move> best;
    int bestRank = RankCount + 1;
    auto consider = [&](PileRef from) {
        const Pile& source = pile(from);
        if (source.empty())
            return;
        const Move up { from, int(source.size()) - 1, { PileKind::Foundation, int(source.back().suit()) } };
        if (canMove(up) && source.back().rank() < bestRank) {
            bestRank = source.back().rank();
            best = up;
        }
    };
    for (int column = 0; column < tableauCount(); ++column)
        consider({ PileKind::Tableau, column });
    for (int c = 0; c < cellCount(); ++c)
        consider({ PileKind::Cell, c });
    consider({ PileKind::Waste, 0 });
    if (best)
        return Hint { false, *best };
    if (canDraw())
        return Hint { true, {} };
    return std::nullopt;
}

int Game::foundationCards() const
{
    int total = 0;
    for (const Pile& f : m_foundations)
        total += int(f.size());
    return total;
}

int Game::goal() const
{
    switch (m_variant) {
    case Variant::Pyramid:
    case Variant::TriPeaks:
        return PeakPlaces;
    case Variant::Golf:
        return 35;
    default:
        return deckSize();
    }
}

int Game::progress() const
{
    if (m_variant == Variant::Pyramid || m_variant == Variant::TriPeaks || m_variant == Variant::Golf) {
        int left = 0;
        for (const Pile& p : m_tableau)
            left += int(p.size());
        return goal() - left;
    }
    return foundationCards();
}

std::string Game::serialize() const
{
    std::string out;
    switch (m_variant) {
    case Variant::Klondike:
        out = std::to_string(m_option);
        break;
    case Variant::FreeCell:
        out = "F";
        break;
    case Variant::Spider:
        out = "S" + std::to_string(m_option);
        break;
    case Variant::Yukon:
        out = "Y";
        break;
    case Variant::Pyramid:
        out = "P";
        break;
    case Variant::TriPeaks:
        out = "T";
        break;
    case Variant::Golf:
        out = "G";
        break;
    }
    if (m_variant != Variant::Klondike)
        out += '|' + std::to_string(m_recycles);
    auto append = [&out](const Pile& pile) {
        out += '|';
        for (const Card& card : pile) {
            out += Ranks[card.rank() - 1];
            out += (card.faceUp ? SuitsUp : SuitsDown)[int(card.suit())];
            if (card.copy() > 0) {
                out += '.';
                out += std::to_string(card.copy());
            }
        }
    };
    append(m_stock);
    append(m_waste);
    for (const Pile& f : m_foundations)
        append(f);
    for (const Pile& t : m_tableau)
        append(t);
    for (const Pile& c : m_cells)
        append(c);
    return out;
}

std::optional<Game> Game::parse(std::string_view text)
{
    std::vector<std::string_view> fields;
    std::size_t begin = 0;
    while (true) {
        const std::size_t bar = text.find('|', begin);
        fields.push_back(text.substr(begin, bar == std::string_view::npos ? std::string_view::npos : bar - begin));
        if (bar == std::string_view::npos)
            break;
        begin = bar + 1;
    }
    if (fields.empty() || fields[0].empty())
        return std::nullopt;

    Game game;
    std::size_t next = 1;
    const std::string_view code = fields[0];
    if (code == "1" || code == "3") {
        game.m_variant = Variant::Klondike;
        game.m_option = code[0] - '0';
    } else {
        switch (code[0]) {
        case 'F':
            game.m_variant = Variant::FreeCell;
            break;
        case 'S':
            game.m_variant = Variant::Spider;
            break;
        case 'Y':
            game.m_variant = Variant::Yukon;
            break;
        case 'P':
            game.m_variant = Variant::Pyramid;
            break;
        case 'T':
            game.m_variant = Variant::TriPeaks;
            break;
        case 'G':
            game.m_variant = Variant::Golf;
            break;
        default:
            return std::nullopt;
        }
        game.m_option = 0;
        if (game.m_variant == Variant::Spider) {
            if (code != "S1" && code != "S2" && code != "S4")
                return std::nullopt;
            game.m_option = code[1] - '0';
        } else if (code.size() != 1) {
            return std::nullopt;
        }
        if (fields.size() < 2 || fields[1].size() != 1 || fields[1][0] < '0' || fields[1][0] > '9')
            return std::nullopt;
        game.m_recycles = fields[1][0] - '0';
        next = 2;
    }

    const Shape shape = shapeOf(game.m_variant);
    if (fields.size() != next + 2 + shape.foundations + shape.tableau + shape.cells)
        return std::nullopt;
    game.m_foundations.resize(shape.foundations);
    game.m_tableau.resize(shape.tableau);
    game.m_cells.resize(shape.cells);

    std::vector<int> deck = game.deck();
    std::vector<bool> seen(2 * DeckSize * 4, false);
    std::vector<bool> allowed(seen.size(), false);
    for (int id : deck)
        allowed[id] = true;
    std::size_t placed = 0;
    auto read = [&](std::string_view field, Pile& pile) {
        std::size_t i = 0;
        while (i < field.size()) {
            if (i + 1 >= field.size())
                return false;
            const auto rank = Ranks.find(field[i]);
            auto suit = SuitsDown.find(field[i + 1]);
            const bool faceUp = suit == std::string_view::npos;
            if (faceUp)
                suit = SuitsUp.find(field[i + 1]);
            if (rank == std::string_view::npos || suit == std::string_view::npos)
                return false;
            i += 2;
            int copy = 0;
            if (i < field.size() && field[i] == '.') {
                if (i + 1 >= field.size() || field[i + 1] < '1' || field[i + 1] > '7')
                    return false;
                copy = field[i + 1] - '0';
                i += 2;
            }
            const Card card = makeCard(static_cast<Suit>(suit), int(rank) + 1, faceUp, copy);
            if (card.id >= seen.size() || !allowed[card.id] || seen[card.id])
                return false;
            seen[card.id] = true;
            ++placed;
            pile.push_back(card);
        }
        return true;
    };

    if (!read(fields[next], game.m_stock) || !read(fields[next + 1], game.m_waste))
        return std::nullopt;
    std::size_t f = next + 2;
    for (Pile& home : game.m_foundations)
        if (!read(fields[f++], home))
            return std::nullopt;
    for (Pile& column : game.m_tableau)
        if (!read(fields[f++], column))
            return std::nullopt;
    for (Pile& cell : game.m_cells)
        if (!read(fields[f++], cell))
            return std::nullopt;
    if (placed != deck.size())
        return std::nullopt;

    for (const Card& card : game.m_stock)
        if (card.faceUp)
            return std::nullopt;
    for (const Card& card : game.m_waste)
        if (!card.faceUp)
            return std::nullopt;
    for (const Pile& cell : game.m_cells)
        if (cell.size() > 1)
            return std::nullopt;
    if (usesPlaces(game.m_variant))
        for (const Pile& place : game.m_tableau)
            if (place.size() > 1)
                return std::nullopt;
    for (int s = 0; s < game.foundationCount(); ++s) {
        const Pile& home = game.m_foundations[s];
        if (suitFoundations(game.m_variant)) {
            for (std::size_t i = 0; i < home.size(); ++i)
                if (int(home[i].suit()) != s || home[i].rank() != int(i) + 1 || !home[i].faceUp)
                    return std::nullopt;
        } else if (game.m_variant == Variant::Spider) {
            if (!home.empty() && (home.size() != std::size_t(RankCount) || home[0].rank() != RankCount || !game.runFrom(home, 0)))
                return std::nullopt;
        }
    }
    if (game.m_recycles >= PyramidPasses && game.m_variant == Variant::Pyramid)
        return std::nullopt;
    return game;
}

} // namespace solitaire
