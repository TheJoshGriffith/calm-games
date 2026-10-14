#include <solitaire/Solver.h>

#include <calm/Rng.h>

#include <algorithm>
#include <cstdlib>
#include <array>
#include <unordered_set>
#include <vector>

namespace solitaire {

namespace {

std::uint64_t hashPile(const Pile& pile, std::uint64_t salt)
{
    std::uint64_t h = salt;
    for (const Card& card : pile)
        h = calm::Rng::mix(h, card.id * 2u + (card.faceUp ? 1u : 0u));
    return calm::Rng::mix(h, pile.size());
}

// Where the layout's columns (or cells) are interchangeable, they are
// hashed in sorted order.
std::uint64_t hashGame(const Game& game)
{
    const Variant variant = game.variant();
    const bool placed = variant == Variant::Pyramid || variant == Variant::TriPeaks;
    std::array<std::uint64_t, 32> columns {};
    const int columnCount = std::min(game.tableauCount(), int(columns.size()));
    for (int c = 0; c < columnCount; ++c)
        columns[c] = hashPile(game.tableau(c), 7);
    if (!placed)
        std::sort(columns.begin(), columns.begin() + columnCount);
    std::array<std::uint64_t, 4> cells {};
    const int cellCount = std::min(game.cellCount(), int(cells.size()));
    for (int c = 0; c < cellCount; ++c)
        cells[c] = hashPile(game.cell(c), 11);
    for (int i = 1; i < cellCount; ++i)
        for (int j = i; j > 0 && cells[j] < cells[j - 1]; --j)
            std::swap(cells[j], cells[j - 1]);
    std::uint64_t h = hashPile(game.stock(), 1);
    // Cards under the waste's top are never played again in TriPeaks and Golf.
    if ((variant == Variant::TriPeaks || variant == Variant::Golf) && !game.waste().empty())
        h = calm::Rng::mix(h, game.waste().back().id);
    else
        h = calm::Rng::mix(h, hashPile(game.waste(), 2));
    h = calm::Rng::mix(h, std::uint64_t(game.passesLeft() + 1));
    for (int s = 0; s < game.foundationCount(); ++s)
        h = calm::Rng::mix(h, game.foundation(s).size());
    for (int c = 0; c < columnCount; ++c)
        h = calm::Rng::mix(h, columns[c]);
    for (int c = 0; c < cellCount; ++c)
        h = calm::Rng::mix(h, cells[c]);
    return h;
}

int foundationRank(const Game& game, Suit suit)
{
    return int(game.foundation(int(suit)).size());
}

// A card can go home with no regret once every card that could still be
// built on it (the next rank down in the other colour) is home already.
bool safeToFoundation(const Game& game, const Card& card)
{
    if (card.rank() <= 2)
        return true;
    const Suit a = card.red() ? Suit::Spades : Suit::Hearts;
    const Suit b = card.red() ? Suit::Clubs : Suit::Diamonds;
    return foundationRank(game, a) >= card.rank() - 1 && foundationRank(game, b) >= card.rank() - 1;
}

// How far a position looks from a win (lower is closer), for trying the
// most promising moves first in FreeCell, Yukon and Spider.
int distance(const Game& game)
{
    const Variant variant = game.variant();
    int score = 4 * (game.goal() - game.progress());
    std::array<int, SuitCount> need {};
    if (variant != Variant::Spider)
        for (int s = 0; s < SuitCount; ++s)
            need[s] = int(game.foundation(s).size()) + 1;
    for (int c = 0; c < game.tableauCount(); ++c) {
        const Pile& column = game.tableau(c);
        if (column.empty()) {
            score -= 3;
            continue;
        }
        for (std::size_t i = 0; i < column.size(); ++i) {
            const Card& card = column[i];
            if (!card.faceUp) {
                score += 3;
                continue;
            }
            if (i > 0) {
                const Card& below = column[i - 1];
                const bool sits = below.faceUp && below.rank() == card.rank() + 1
                    && (variant == Variant::Spider ? below.suit() == card.suit() : below.red() != card.red());
                if (!sits)
                    ++score;
            }
            // Cards lying on one that is wanted home next.
            if (variant != Variant::Spider && card.rank() == need[int(card.suit())])
                score += int(column.size() - 1 - i);
        }
    }
    for (int c = 0; c < game.cellCount(); ++c)
        score += game.cell(c).empty() ? 0 : 1;
    return score;
}

// Plays every move that can't hurt, noting them in `played` if given: safe
// cards home in Game, FreeCell and Yukon; kings off in Pyramid.
void playSafeMoves(Game& game, std::vector<Hint>* played = nullptr)
{
    const Variant variant = game.variant();
    std::vector<PileRef> sources;
    if (variant == Variant::Klondike || variant == Variant::FreeCell || variant == Variant::Yukon) {
        for (int c = 0; c < game.tableauCount(); ++c)
            sources.push_back({ PileKind::Tableau, c });
        for (int c = 0; c < game.cellCount(); ++c)
            sources.push_back({ PileKind::Cell, c });
        sources.push_back({ PileKind::Waste, 0 });
    } else if (variant == Variant::Pyramid) {
        for (int c = 0; c < game.tableauCount(); ++c)
            sources.push_back({ PileKind::Tableau, c });
        sources.push_back({ PileKind::Waste, 0 });
    } else {
        return;
    }
    bool progress = true;
    while (progress) {
        progress = false;
        for (const PileRef& from : sources) {
            const Pile& source = game.pile(from);
            if (source.empty())
                continue;
            const Card& card = source.back();
            Move up { from, int(source.size()) - 1, { PileKind::Foundation, 0 } };
            if (variant == Variant::Pyramid) {
                if (card.rank() != RankCount)
                    continue;
            } else {
                if (!safeToFoundation(game, card))
                    continue;
                up.to.index = int(card.suit());
            }
            if (game.move(up).ok) {
                progress = true;
                if (played)
                    played->push_back({ false, up });
            }
        }
    }
}

// TriPeaks and Golf have so few kinds of move that a search over a compact
// form of the position is far quicker than the general one: which cards are
// left in the layout, how far through the stock, and the waste's top rank.
class PeaksSearch {
public:
    PeaksSearch(const Game& game, int budget, const std::atomic_bool* stop)
        : m_golf(game.variant() == Variant::Golf)
        , m_budget(budget)
        , m_stop(stop)
    {
        m_places = game.tableauCount();
        for (int i = 0; i < m_places; ++i) {
            const Pile& pile = game.tableau(i);
            m_cards.push_back({});
            for (const Card& card : pile)
                m_cards.back().push_back(card.rank());
            m_left.push_back(int(pile.size()));
            if (!m_golf)
                m_covers.push_back(coversOf(i));
        }
        const Pile& stock = game.stock();
        for (auto it = stock.rbegin(); it != stock.rend(); ++it)
            m_stock.push_back(it->rank());
        m_top = game.waste().empty() ? 0 : game.waste().back().rank();
        for (int left : m_left)
            m_remaining += left;
    }

    Verdict run()
    {
        if (search())
            return Verdict::Winnable;
        return m_outOfBudget ? Verdict::Unknown : Verdict::Unwinnable;
    }

    std::vector<Hint> path;

private:
    static std::array<int, 2> coversOf(int index)
    {
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

    bool free(int i) const
    {
        if (m_left[i] == 0)
            return false;
        if (m_golf)
            return true;
        for (int over : m_covers[i])
            if (over >= 0 && m_left[over] > 0)
                return false;
        return true;
    }

    bool fits(int rank) const
    {
        if (m_top == 0)
            return false;
        const int gap = std::abs(rank - m_top);
        if (m_golf)
            return m_top != RankCount && gap == 1;
        return gap == 1 || gap == RankCount - 1;
    }

    std::uint64_t key() const
    {
        std::uint64_t k = 0;
        for (int left : m_left)
            k = k * (m_golf ? 6 : 2) + std::uint64_t(left);
        k = k * 64 + std::uint64_t(m_drawn);
        return k * 16 + std::uint64_t(m_top);
    }

    bool search()
    {
        if (m_remaining == 0)
            return true;
        if (m_nodes >= m_budget || (m_stop && m_stop->load(std::memory_order_relaxed))) {
            m_outOfBudget = true;
            return false;
        }
        ++m_nodes;
        const std::uint64_t k = key();
        if (m_dead.count(k))
            return false;
        for (int i = 0; i < m_places; ++i) {
            if (!free(i))
                continue;
            const int rank = m_cards[i][m_left[i] - 1];
            if (!fits(rank))
                continue;
            const int top = m_top;
            --m_left[i];
            --m_remaining;
            m_top = rank;
            path.push_back({ false, { { PileKind::Tableau, i }, m_left[i], { PileKind::Waste, 0 } } });
            if (search())
                return true;
            path.pop_back();
            m_top = top;
            ++m_remaining;
            ++m_left[i];
        }
        if (m_drawn < int(m_stock.size())) {
            const int top = m_top;
            m_top = m_stock[m_drawn++];
            path.push_back({ true, {} });
            if (search())
                return true;
            path.pop_back();
            --m_drawn;
            m_top = top;
        }
        if (!m_outOfBudget)
            m_dead.insert(k);
        return false;
    }

    bool m_golf;
    int m_budget;
    const std::atomic_bool* m_stop;
    int m_places = 0;
    std::vector<std::vector<int>> m_cards; // ranks, bottom first
    std::vector<int> m_left;
    std::vector<std::array<int, 2>> m_covers;
    std::vector<int> m_stock; // ranks in the order they are drawn
    int m_drawn = 0;
    int m_top = 0;
    int m_remaining = 0;
    int m_nodes = 0;
    bool m_outOfBudget = false;
    std::unordered_set<std::uint64_t> m_dead;
};

class Search {
public:
    Search(int budget, bool exhaustive, const std::atomic_bool* stop = nullptr)
        : m_budget(budget)
        , m_exhaustive(exhaustive)
        , m_stop(stop)
    {
    }

    Verdict run(const Game& start)
    {
        if (begin(start))
            return Verdict::Winnable;
        return *advance(m_budget);
    }

    // Starts the search at `start`; true if that is a win already.
    bool begin(const Game& start)
    {
        if (start.variant() == Variant::TriPeaks || start.variant() == Variant::Golf) {
            PeaksSearch peaks(start, m_budget, m_stop);
            m_quick = peaks.run();
            path = std::move(peaks.path);
            return m_quick == Verdict::Winnable;
        }
        if (enter(start, {}))
            return true;
        // The root's own safe moves come first on any winning line.
        if (!m_stack.empty())
            m_rootMoves = m_stack.front().via;
        return false;
    }

    // Searches up to `slice` more positions: the verdict, or nothing if the
    // search should carry on later.
    std::optional<Verdict> advance(int slice)
    {
        if (m_quick)
            return m_quick;
        const long long until = static_cast<long long>(m_nodes) + slice;
        while (!m_stack.empty()) {
            if (m_nodes >= m_budget || (m_stop && m_stop->load(std::memory_order_relaxed)))
                return Verdict::Unknown;
            if (m_nodes >= until)
                return std::nullopt;
            Frame& top = m_stack.back();
            if (top.next >= top.moves.size()) {
                m_stack.pop_back();
                continue;
            }
            const Hint step = top.moves[top.next++];
            Game next = top.game;
            if (step.draw)
                next.draw();
            else if (!next.move(step.move).ok)
                continue;
            if (enter(std::move(next), { step })) {
                path.insert(path.begin(), m_rootMoves.begin(), m_rootMoves.end());
                m_stack.clear();
                return Verdict::Winnable;
            }
        }
        return Verdict::Unwinnable;
    }

private:
    // Iterative depth-first search: games can run hundreds of moves deep,
    // too deep for the small stacks of worker threads.
    struct Frame {
        Game game;
        std::vector<Hint> moves;
        std::size_t next = 0;
        std::vector<Hint> via; // the steps that led here
    };

    // Plays the safe moves from `game` and pushes it to search; true if won.
    bool enter(Game game, std::vector<Hint> via)
    {
        playSafeMoves(game, &via);
        if (game.won()) {
            path.clear();
            for (std::size_t i = 1; i < m_stack.size(); ++i)
                path.insert(path.end(), m_stack[i].via.begin(), m_stack[i].via.end());
            if (!m_stack.empty())
                path.insert(path.end(), via.begin(), via.end());
            else
                path = via;
            return true;
        }
        ++m_nodes;
        if (m_seen.insert(hashGame(game)).second) {
            auto moves = candidates(game);
            m_stack.push_back({ std::move(game), std::move(moves), 0, std::move(via) });
        }
        return false;
    }

    std::vector<Hint> candidates(const Game& game) const
    {
        if (game.variant() != Variant::Klondike)
            return otherCandidates(game);
        std::vector<Hint> out;
        const Pile& waste = game.waste();

        // Home moves first: they never hurt for long.
        for (int c = 0; c < game.tableauCount(); ++c) {
            const Pile& source = game.tableau(c);
            if (source.empty())
                continue;
            Move up { { PileKind::Tableau, c }, int(source.size()) - 1, { PileKind::Foundation, int(source.back().suit()) } };
            if (game.canMove(up))
                out.push_back({ false, up });
        }
        if (!waste.empty()) {
            Move up { { PileKind::Waste, 0 }, int(waste.size()) - 1, { PileKind::Foundation, int(waste.back().suit()) } };
            if (game.canMove(up))
                out.push_back({ false, up });
        }

        // Tableau moves that reveal a card, free a column or send the card
        // beneath home.
        for (int c = 0; c < game.tableauCount(); ++c) {
            const Pile& source = game.tableau(c);
            for (int start = 0; start < int(source.size()); ++start) {
                if (!source[start].faceUp)
                    continue;
                bool useful;
                if (start == 0 || m_exhaustive) {
                    useful = true; // checked against an empty target below
                } else {
                    const Card& below = source[start - 1];
                    useful = !below.faceUp
                        || below.rank() == int(game.foundation(int(below.suit())).size()) + 1;
                }
                if (!useful)
                    continue;
                bool triedEmpty = false;
                for (int t = 0; t < game.tableauCount(); ++t) {
                    const bool empty = game.tableau(t).empty();
                    if (empty && (start == 0 || triedEmpty))
                        continue;
                    Move m { { PileKind::Tableau, c }, start, { PileKind::Tableau, t } };
                    if (game.canMove(m)) {
                        out.push_back({ false, m });
                        triedEmpty = triedEmpty || empty;
                    }
                }
            }
        }

        // The waste card onto the tableau.
        if (!waste.empty()) {
            bool triedEmpty = false;
            for (int t = 0; t < game.tableauCount(); ++t) {
                const bool empty = game.tableau(t).empty();
                if (empty && triedEmpty)
                    continue;
                Move m { { PileKind::Waste, 0 }, int(waste.size()) - 1, { PileKind::Tableau, t } };
                if (game.canMove(m)) {
                    out.push_back({ false, m });
                    triedEmpty = triedEmpty || empty;
                }
            }
        }

        // Cards back down from the foundations, to hold a card that needs
        // a home on the tableau.
        for (int s = 0; s < SuitCount; ++s) {
            const Pile& f = game.foundation(s);
            if (f.empty() || (f.size() < 3 && !m_exhaustive))
                continue;
            bool triedEmpty = false;
            for (int t = 0; t < game.tableauCount(); ++t) {
                const bool empty = game.tableau(t).empty();
                if (empty && (triedEmpty || !m_exhaustive))
                    continue;
                Move m { { PileKind::Foundation, s }, int(f.size()) - 1, { PileKind::Tableau, t } };
                if (game.canMove(m)) {
                    out.push_back({ false, m });
                    triedEmpty = triedEmpty || empty;
                }
            }
        }

        if (!game.stock().empty() || !waste.empty())
            out.push_back({ true, {} });
        return out;
    }

    // Every variant but Klondike: the legal moves, minus ones that repeat
    // another (moves to a second empty column or cell) or gain nothing (a
    // whole column into an empty one), best first.
    std::vector<Hint> otherCandidates(const Game& game) const
    {
        const Variant variant = game.variant();
        std::vector<Hint> moves = game.legalMoves();
        std::vector<std::pair<int, Hint>> ranked;
        int firstEmptyColumn = -1;
        for (int c = 0; c < game.tableauCount() && firstEmptyColumn < 0; ++c)
            if (game.tableau(c).empty())
                firstEmptyColumn = c;
        int firstEmptyCell = -1;
        for (int c = 0; c < game.cellCount() && firstEmptyCell < 0; ++c)
            if (game.cell(c).empty())
                firstEmptyCell = c;
        const bool columns = variant == Variant::FreeCell || variant == Variant::Spider || variant == Variant::Yukon;

        for (const Hint& h : moves) {
            if (h.draw) {
                ranked.push_back({ 9, h });
                continue;
            }
            const Move& m = h.move;
            int rank = 4;
            if (m.to.kind == PileKind::Foundation || m.to.kind == PileKind::Waste) {
                if (m.from.kind == PileKind::Foundation && !m_exhaustive)
                    continue;
                rank = m.from.kind == PileKind::Foundation ? 8 : 0;
            } else if (m.to.kind == PileKind::Cell) {
                if (m.to.index != firstEmptyCell)
                    continue;
                rank = 7;
            } else if (columns && m.to.kind == PileKind::Tableau && game.tableau(m.to.index).empty()) {
                if (m.to.index != firstEmptyColumn)
                    continue;
                if (m.from.kind == PileKind::Tableau && m.start == 0)
                    continue;
                rank = 6;
            } else if (columns && m.from.kind == PileKind::Tableau) {
                const Pile& source = game.tableau(m.from.index);
                const Card& card = source[m.start];
                const Pile& target = game.tableau(m.to.index);
                if (m.start == 0 || !source[m.start - 1].faceUp) {
                    rank = 1; // frees a column or turns a card over
                } else if (variant == Variant::Spider) {
                    const Card& below = source[m.start - 1];
                    const bool suited = below.suit() == card.suit() && below.rank() == card.rank() + 1;
                    const bool joins = target.back().suit() == card.suit();
                    if (suited && !joins && !m_exhaustive)
                        continue; // breaks a suited run for nothing
                    rank = joins ? 2 : 5;
                } else {
                    rank = 3;
                }
            } else if (m.from.kind == PileKind::Cell) {
                rank = 2;
            }
            ranked.push_back({ rank, h });
        }
        if (variant == Variant::FreeCell || variant == Variant::Yukon || variant == Variant::Spider) {
            for (auto& entry : ranked) {
                if (entry.second.draw) {
                    entry.first += 1000;
                    continue;
                }
                Game next = game;
                next.move(entry.second.move);
                entry.first += 16 * distance(next);
            }
        }
        std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<Hint> out;
        out.reserve(ranked.size());
        for (const auto& entry : ranked)
            out.push_back(entry.second);
        return out;
    }

public:
    // After a win is found: every step of it, from the start position.
    std::vector<Hint> path;

private:
    int m_budget;
    bool m_exhaustive;
    const std::atomic_bool* m_stop;
    int m_nodes = 0;
    std::unordered_set<std::uint64_t> m_seen;
    std::vector<Frame> m_stack;
    std::vector<Hint> m_rootMoves;
    std::optional<Verdict> m_quick; // TriPeaks and Golf, settled in begin()
};

} // namespace

struct SteppedSolve::Impl {
    Search search;
    std::optional<Verdict> verdict;
};

SteppedSolve::SteppedSolve(const Game& game, int nodeBudget, bool exhaustive)
    : m_impl(std::make_unique<Impl>(Impl { Search(nodeBudget, exhaustive), std::nullopt }))
{
    if (m_impl->search.begin(game))
        m_impl->verdict = Verdict::Winnable;
}

SteppedSolve::~SteppedSolve() = default;
SteppedSolve::SteppedSolve(SteppedSolve&&) noexcept = default;
SteppedSolve& SteppedSolve::operator=(SteppedSolve&&) noexcept = default;

std::optional<Verdict> SteppedSolve::step(int nodes)
{
    if (!m_impl->verdict)
        m_impl->verdict = m_impl->search.advance(nodes);
    return m_impl->verdict;
}

Verdict solve(const Game& game, int nodeBudget)
{
    return Search(nodeBudget, false).run(game);
}

Verdict solveExhaustive(const Game& game, int nodeBudget, const std::atomic_bool* stop)
{
    return Search(nodeBudget, true, stop).run(game);
}

std::optional<std::vector<Hint>> solvePath(const Game& game, int nodeBudget)
{
    Search search(nodeBudget, false);
    if (search.run(game) != Verdict::Winnable)
        return std::nullopt;
    return search.path;
}

SteppedDeal::SteppedDeal(Variant variant, std::uint64_t seed, int option, int attempts, int nodeBudget)
    : m_variant(variant)
    , m_option(option)
    , m_attempts(std::max(1, attempts))
    , m_budget(nodeBudget)
    , m_seed(seed)
    , m_next(seed)
{
}

std::optional<Deal> SteppedDeal::step(int nodes)
{
    if (!m_search) {
        if (m_attempt > 0 && (m_variant == Variant::Spider || m_attempt >= m_attempts))
            return m_deal; // out of tries: the last deal, unproven
        m_deal.seed = m_attempt == 0 ? m_seed : calm::Rng::splitMix(m_next);
        m_deal.game = Game::deal(m_variant, m_deal.seed, m_option);
        m_deal.proven = false;
        ++m_attempt;
        if (m_variant == Variant::Spider)
            return m_deal;
        m_search.emplace(m_deal.game, m_budget, false);
    }
    const auto verdict = m_search->step(nodes);
    if (!verdict)
        return std::nullopt;
    m_search.reset();
    if (*verdict == Verdict::Winnable) {
        m_deal.proven = true;
        return m_deal;
    }
    if (m_attempt >= m_attempts)
        return m_deal;
    return std::nullopt;
}

Deal dealWinnable(Variant variant, std::uint64_t seed, int option, int attempts, int nodeBudget)
{
    SteppedDeal search(variant, seed, option, attempts, nodeBudget);
    while (true)
        if (auto deal = search.step(nodeBudget + 1))
            return *deal;
}

} // namespace solitaire
