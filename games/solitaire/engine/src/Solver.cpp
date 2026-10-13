#include <solitaire/Solver.h>

#include <calm/Rng.h>

#include <algorithm>
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

// Columns are interchangeable, so they are hashed in sorted order.
std::uint64_t hashGame(const Klondike& game)
{
    std::array<std::uint64_t, TableauCount> columns {};
    for (int c = 0; c < TableauCount; ++c)
        columns[c] = hashPile(game.tableau(c), 7);
    std::sort(columns.begin(), columns.end());
    std::uint64_t h = hashPile(game.stock(), 1);
    h = calm::Rng::mix(h, hashPile(game.waste(), 2));
    for (int s = 0; s < SuitCount; ++s)
        h = calm::Rng::mix(h, game.foundation(s).size());
    for (auto column : columns)
        h = calm::Rng::mix(h, column);
    return h;
}

int foundationRank(const Klondike& game, Suit suit)
{
    return int(game.foundation(int(suit)).size());
}

// A card can go home with no regret once every card that could still be
// built on it (the next rank down in the other colour) is home already.
bool safeToFoundation(const Klondike& game, const Card& card)
{
    if (card.rank() <= 2)
        return true;
    const Suit a = card.red() ? Suit::Spades : Suit::Hearts;
    const Suit b = card.red() ? Suit::Clubs : Suit::Diamonds;
    return foundationRank(game, a) >= card.rank() - 1 && foundationRank(game, b) >= card.rank() - 1;
}

// Plays every safe home move, noting them in `played` if given.
void playSafeMoves(Klondike& game, std::vector<Hint>* played = nullptr)
{
    bool progress = true;
    while (progress) {
        progress = false;
        for (int c = 0; c < TableauCount + 1; ++c) {
            const PileRef from = c < TableauCount ? PileRef { PileKind::Tableau, c } : PileRef { PileKind::Waste, 0 };
            const Pile& source = game.pile(from);
            if (source.empty() || !safeToFoundation(game, source.back()))
                continue;
            const Move up { from, int(source.size()) - 1, { PileKind::Foundation, int(source.back().suit()) } };
            if (game.move(up).ok) {
                progress = true;
                if (played)
                    played->push_back({ false, up });
            }
        }
    }
}

class Search {
public:
    explicit Search(int budget)
        : m_budget(budget)
    {
    }

    Verdict run(const Klondike& start)
    {
        // Iterative depth-first search: games can run hundreds of moves
        // deep, too deep for the small stacks of worker threads.
        struct Frame {
            Klondike game;
            std::vector<Hint> moves;
            std::size_t next = 0;
            std::vector<Hint> via; // the steps that led here
        };
        std::vector<Frame> stack;
        auto enter = [&](Klondike game, std::vector<Hint> via) {
            playSafeMoves(game, &via);
            if (game.won()) {
                path.clear();
                for (std::size_t i = 1; i < stack.size(); ++i)
                    path.insert(path.end(), stack[i].via.begin(), stack[i].via.end());
                if (!stack.empty())
                    path.insert(path.end(), via.begin(), via.end());
                else
                    path = via;
                return true;
            }
            ++m_nodes;
            if (m_seen.insert(hashGame(game)).second) {
                auto moves = candidates(game);
                stack.push_back({ std::move(game), std::move(moves), 0, std::move(via) });
            }
            return false;
        };

        if (enter(start, {}))
            return Verdict::Winnable;
        // The root's own safe moves come first on any winning line.
        const std::vector<Hint> rootMoves = stack.empty() ? std::vector<Hint> {} : stack.front().via;
        while (!stack.empty()) {
            if (m_nodes >= m_budget)
                return Verdict::Unknown;
            Frame& top = stack.back();
            if (top.next >= top.moves.size()) {
                stack.pop_back();
                continue;
            }
            const Hint step = top.moves[top.next++];
            Klondike next = top.game;
            if (step.draw)
                next.draw();
            else if (!next.move(step.move).ok)
                continue;
            if (enter(std::move(next), { step })) {
                path.insert(path.begin(), rootMoves.begin(), rootMoves.end());
                return Verdict::Winnable;
            }
        }
        return Verdict::Unwinnable;
    }

private:
    static std::vector<Hint> candidates(const Klondike& game)
    {
        std::vector<Hint> out;
        const Pile& waste = game.waste();

        // Home moves first: they never hurt for long.
        for (int c = 0; c < TableauCount; ++c) {
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
        for (int c = 0; c < TableauCount; ++c) {
            const Pile& source = game.tableau(c);
            for (int start = 0; start < int(source.size()); ++start) {
                if (!source[start].faceUp)
                    continue;
                bool useful;
                if (start == 0) {
                    useful = true; // checked against an empty target below
                } else {
                    const Card& below = source[start - 1];
                    useful = !below.faceUp
                        || below.rank() == int(game.foundation(int(below.suit())).size()) + 1;
                }
                if (!useful)
                    continue;
                bool triedEmpty = false;
                for (int t = 0; t < TableauCount; ++t) {
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
            for (int t = 0; t < TableauCount; ++t) {
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
            if (f.size() < 3)
                continue;
            for (int t = 0; t < TableauCount; ++t) {
                if (game.tableau(t).empty())
                    continue;
                Move m { { PileKind::Foundation, s }, int(f.size()) - 1, { PileKind::Tableau, t } };
                if (game.canMove(m))
                    out.push_back({ false, m });
            }
        }

        if (!game.stock().empty() || !waste.empty())
            out.push_back({ true, {} });
        return out;
    }

public:
    // After a win is found: every step of it, from the start position.
    std::vector<Hint> path;

private:
    int m_budget;
    int m_nodes = 0;
    std::unordered_set<std::uint64_t> m_seen;
};

} // namespace

Verdict solve(const Klondike& game, int nodeBudget)
{
    return Search(nodeBudget).run(game);
}

std::optional<std::vector<Hint>> solvePath(const Klondike& game, int nodeBudget)
{
    Search search(nodeBudget);
    if (search.run(game) != Verdict::Winnable)
        return std::nullopt;
    return search.path;
}

Deal dealWinnable(std::uint64_t seed, int drawCount, int attempts, int nodeBudget)
{
    Deal deal;
    std::uint64_t next = seed;
    for (int i = 0; i < std::max(1, attempts); ++i) {
        deal.seed = i == 0 ? seed : calm::Rng::splitMix(next);
        deal.game = Klondike::deal(deal.seed, drawCount);
        if (solve(deal.game, nodeBudget) == Verdict::Winnable) {
            deal.proven = true;
            return deal;
        }
    }
    return deal;
}

} // namespace solitaire
