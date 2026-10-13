#include <solitaire/Klondike.h>

#include <calm/Rng.h>

#include <algorithm>
#include <stdexcept>

namespace solitaire {

namespace {

constexpr std::string_view Ranks = "A23456789TJQK";
constexpr std::string_view SuitsDown = "shdc";
constexpr std::string_view SuitsUp = "SHDC";

int topRank(const Pile& pile)
{
    return pile.empty() ? 0 : pile.back().rank();
}

} // namespace

Klondike Klondike::deal(std::uint64_t seed, int drawCount)
{
    std::array<std::uint8_t, DeckSize> deck {};
    for (int i = 0; i < DeckSize; ++i)
        deck[i] = static_cast<std::uint8_t>(i);
    calm::Rng rng(seed);
    rng.shuffle(deck.begin(), deck.end());

    Klondike game;
    game.m_drawCount = drawCount == 3 ? 3 : 1;
    int next = 0;
    for (int row = 0; row < TableauCount; ++row) {
        for (int column = row; column < TableauCount; ++column)
            game.m_tableau[column].push_back({ deck[next++], column == row });
    }
    while (next < DeckSize)
        game.m_stock.push_back({ deck[next++], false });
    return game;
}

const Pile& Klondike::pile(PileRef ref) const
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
    }
    throw std::out_of_range("pile");
}

Pile& Klondike::mutablePile(PileRef ref)
{
    return const_cast<Pile&>(pile(ref));
}

DrawResult Klondike::draw()
{
    if (!m_stock.empty()) {
        for (int i = 0; i < m_drawCount && !m_stock.empty(); ++i) {
            Card card = m_stock.back();
            m_stock.pop_back();
            card.faceUp = true;
            m_waste.push_back(card);
        }
        return DrawResult::Drew;
    }
    if (m_waste.empty())
        return DrawResult::Nothing;
    while (!m_waste.empty()) {
        Card card = m_waste.back();
        m_waste.pop_back();
        card.faceUp = false;
        m_stock.push_back(card);
    }
    return DrawResult::Recycled;
}

bool Klondike::fitsOn(const Card& card, PileRef to) const
{
    if (to.kind == PileKind::Foundation) {
        if (to.index < 0 || to.index >= SuitCount || int(card.suit()) != to.index)
            return false;
        return card.rank() == topRank(m_foundations[to.index]) + 1;
    }
    if (to.kind == PileKind::Tableau) {
        if (to.index < 0 || to.index >= TableauCount)
            return false;
        const Pile& target = m_tableau[to.index];
        if (target.empty())
            return card.rank() == 13;
        const Card& top = target.back();
        return top.faceUp && top.red() != card.red() && top.rank() == card.rank() + 1;
    }
    return false;
}

bool Klondike::canMove(const Move& move) const
{
    if (move.from == move.to)
        return false;
    if (move.from.kind == PileKind::Stock || move.to.kind == PileKind::Stock || move.to.kind == PileKind::Waste)
        return false;
    if (move.from.kind == PileKind::Foundation && (move.from.index < 0 || move.from.index >= SuitCount))
        return false;
    if (move.from.kind == PileKind::Tableau && (move.from.index < 0 || move.from.index >= TableauCount))
        return false;

    const Pile& source = pile(move.from);
    if (move.start < 0 || move.start >= int(source.size()))
        return false;
    const int count = int(source.size()) - move.start;
    // Only tableau runs move as a group; elsewhere just the top card moves.
    if (count > 1 && (move.from.kind != PileKind::Tableau || move.to.kind != PileKind::Tableau))
        return false;
    if (!source[move.start].faceUp)
        return false;
    return fitsOn(source[move.start], move.to);
}

MoveResult Klondike::move(const Move& move)
{
    MoveResult result;
    if (!canMove(move))
        return result;
    Pile& source = mutablePile(move.from);
    Pile& target = mutablePile(move.to);
    target.insert(target.end(), source.begin() + move.start, source.end());
    source.erase(source.begin() + move.start, source.end());
    if (move.from.kind == PileKind::Tableau && !source.empty() && !source.back().faceUp) {
        source.back().faceUp = true;
        result.flipped = true;
    }
    result.ok = true;
    result.toFoundation = move.to.kind == PileKind::Foundation;
    return result;
}

std::optional<Move> Klondike::bestMove(PileRef from, int start) const
{
    if (from.kind == PileKind::Stock)
        return std::nullopt;
    const Pile& source = pile(from);
    if (start < 0 || start >= int(source.size()))
        return std::nullopt;
    const Card& card = source[start];

    if (from.kind != PileKind::Foundation && start == int(source.size()) - 1) {
        Move up { from, start, { PileKind::Foundation, int(card.suit()) } };
        if (canMove(up))
            return up;
    }

    for (int column = 0; column < TableauCount; ++column) {
        Move candidate { from, start, { PileKind::Tableau, column } };
        if (!canMove(candidate))
            continue;
        // A king already at the bottom of its column gains nothing.
        if (m_tableau[column].empty() && from.kind == PileKind::Tableau && start == 0)
            continue;
        return candidate;
    }
    return std::nullopt;
}

bool Klondike::isProductive(const Move& move) const
{
    if (move.from.kind != PileKind::Tableau || move.to.kind != PileKind::Tableau)
        return false;
    const Pile& source = m_tableau[move.from.index];
    if (move.start == 0)
        return !m_tableau[move.to.index].empty(); // frees a column
    const Card& below = source[move.start - 1];
    if (!below.faceUp)
        return true; // turns a card over
    // Splitting a run only helps when it sends the card beneath home.
    return below.rank() == topRank(m_foundations[int(below.suit())]) + 1;
}

std::optional<Hint> Klondike::hint() const
{
    // 1. Anything that can go home.
    for (int column = 0; column < TableauCount; ++column) {
        const Pile& source = m_tableau[column];
        if (source.empty())
            continue;
        Move up { { PileKind::Tableau, column }, int(source.size()) - 1, { PileKind::Foundation, int(source.back().suit()) } };
        if (canMove(up))
            return Hint { false, up };
    }
    if (!m_waste.empty()) {
        Move up { { PileKind::Waste, 0 }, int(m_waste.size()) - 1, { PileKind::Foundation, int(m_waste.back().suit()) } };
        if (canMove(up))
            return Hint { false, up };
    }

    // 2. Tableau moves that turn a card over or free a column.
    for (int column = 0; column < TableauCount; ++column) {
        const Pile& source = m_tableau[column];
        for (int start = 0; start < int(source.size()); ++start) {
            if (!source[start].faceUp)
                continue;
            for (int target = 0; target < TableauCount; ++target) {
                Move candidate { { PileKind::Tableau, column }, start, { PileKind::Tableau, target } };
                if (canMove(candidate) && isProductive(candidate))
                    return Hint { false, candidate };
            }
        }
    }

    // 3. The waste card onto the tableau.
    if (!m_waste.empty()) {
        if (auto move = bestMove({ PileKind::Waste, 0 }, int(m_waste.size()) - 1))
            return Hint { false, *move };
    }

    // 4. Turn the stock if any card in it or the waste could be played.
    auto playable = [this](const Card& card) {
        if (fitsOn(card, { PileKind::Foundation, int(card.suit()) }))
            return true;
        for (int column = 0; column < TableauCount; ++column)
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

bool Klondike::canAutoFinish() const
{
    if (won())
        return false;
    for (const Pile& column : m_tableau)
        for (const Card& card : column)
            if (!card.faceUp)
                return false;
    return true;
}

std::optional<Hint> Klondike::autoFinishStep() const
{
    if (!canAutoFinish())
        return std::nullopt;
    std::optional<Move> best;
    int bestRank = RankCount + 1;
    auto consider = [&](PileRef from) {
        const Pile& source = pile(from);
        if (source.empty())
            return;
        Move up { from, int(source.size()) - 1, { PileKind::Foundation, int(source.back().suit()) } };
        if (canMove(up) && source.back().rank() < bestRank) {
            bestRank = source.back().rank();
            best = up;
        }
    };
    for (int column = 0; column < TableauCount; ++column)
        consider({ PileKind::Tableau, column });
    consider({ PileKind::Waste, 0 });
    if (best)
        return Hint { false, *best };
    if (!m_stock.empty() || !m_waste.empty())
        return Hint { true, {} };
    return std::nullopt;
}

int Klondike::foundationCards() const
{
    int total = 0;
    for (const Pile& f : m_foundations)
        total += int(f.size());
    return total;
}

std::string Klondike::serialize() const
{
    std::string out = std::to_string(m_drawCount);
    auto append = [&out](const Pile& pile) {
        out += '|';
        for (const Card& card : pile) {
            out += Ranks[card.rank() - 1];
            out += (card.faceUp ? SuitsUp : SuitsDown)[int(card.suit())];
        }
    };
    append(m_stock);
    append(m_waste);
    for (const Pile& f : m_foundations)
        append(f);
    for (const Pile& t : m_tableau)
        append(t);
    return out;
}

std::optional<Klondike> Klondike::parse(std::string_view text)
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
    if (fields.size() != 3 + SuitCount + TableauCount)
        return std::nullopt;

    Klondike game;
    if (fields[0] == "1")
        game.m_drawCount = 1;
    else if (fields[0] == "3")
        game.m_drawCount = 3;
    else
        return std::nullopt;

    std::array<bool, DeckSize> seen {};
    auto read = [&seen](std::string_view field, Pile& pile) {
        if (field.size() % 2 != 0)
            return false;
        for (std::size_t i = 0; i < field.size(); i += 2) {
            const auto rank = Ranks.find(field[i]);
            auto suit = SuitsDown.find(field[i + 1]);
            const bool faceUp = suit == std::string_view::npos;
            if (faceUp)
                suit = SuitsUp.find(field[i + 1]);
            if (rank == std::string_view::npos || suit == std::string_view::npos)
                return false;
            const Card card = makeCard(static_cast<Suit>(suit), int(rank) + 1, faceUp);
            if (seen[card.id])
                return false;
            seen[card.id] = true;
            pile.push_back(card);
        }
        return true;
    };

    if (!read(fields[1], game.m_stock) || !read(fields[2], game.m_waste))
        return std::nullopt;
    for (int s = 0; s < SuitCount; ++s) {
        Pile& f = game.m_foundations[s];
        if (!read(fields[3 + s], f))
            return std::nullopt;
        for (std::size_t i = 0; i < f.size(); ++i)
            if (int(f[i].suit()) != s || f[i].rank() != int(i) + 1 || !f[i].faceUp)
                return std::nullopt;
    }
    for (int c = 0; c < TableauCount; ++c)
        if (!read(fields[7 + c], game.m_tableau[c]))
            return std::nullopt;
    if (std::find(seen.begin(), seen.end(), false) != seen.end())
        return std::nullopt;
    for (const Card& card : game.m_stock)
        if (card.faceUp)
            return std::nullopt;
    for (const Card& card : game.m_waste)
        if (!card.faceUp)
            return std::nullopt;
    return game;
}

} // namespace solitaire
