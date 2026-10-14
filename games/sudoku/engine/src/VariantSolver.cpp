#include "sudoku/VariantSolver.h"

#include "VariantDetail.h"

#include <calm/Rng.h>

#include <algorithm>
#include <bit>

namespace sudoku {

namespace {

// A set of cells.
struct Cells {
    std::uint64_t lo = 0; // cells 0..63
    std::uint64_t hi = 0; // cells 64..80

    void set(int i) noexcept { (i < 64 ? lo : hi) |= std::uint64_t { 1 } << (i & 63); }
    bool test(int i) const noexcept { return ((i < 64 ? lo : hi) >> (i & 63)) & 1u; }
    Cells& operator&=(const Cells& o) noexcept
    {
        lo &= o.lo;
        hi &= o.hi;
        return *this;
    }
    template <typename F>
    void forEach(F&& f) const
    {
        for (std::uint64_t w = lo; w; w &= w - 1)
            f(std::countr_zero(w));
        for (std::uint64_t w = hi; w; w &= w - 1)
            f(64 + std::countr_zero(w));
    }
};

struct State {
    std::array<Mask, 81> m {};
    std::array<bool, 81> done {}; // a fixed digit already removed from its peers
};

bool single(Mask m) noexcept { return m && !(m & (m - 1)); }
int highDigit(Mask m) noexcept { return 15 - std::countl_zero(static_cast<std::uint16_t>(m)); }

enum class NaryType : std::uint8_t { Cage, Sum, Arrow, Renban, Sandwich };

struct Nary {
    NaryType type = NaryType::Cage;
    std::vector<int> cells; // Arrow: circle first; Sandwich: the row or column in order
    int sum = 0;
};

using Table = std::array<Mask, 10>;

constexpr int kRelations = static_cast<int>(detail::Relation::NotConsecutive) + 1;
// Cap on the combinations one clue's reasoning may look at in one pass. A
// clue past it is left alone for now; it is checked exactly once its cells
// narrow down, so this only limits deduction strength, never correctness.
constexpr int kNaryBudget = 4096;

} // namespace

struct VariantModel::Impl {
    Rules rules;
    std::vector<std::array<int, 9>> units;
    std::array<std::vector<int>, 81> peers;
    std::array<Cells, 81> peerSet;
    std::array<Mask, 81> unary {};
    // For each relation: forward[a] = digits b may take beside a, and
    // backward[b] = digits a may take beside b.
    std::array<Table, kRelations> forward {};
    std::array<Table, kRelations> backward {};
    struct Link {
        int a, b;
        const Table* forward;
        const Table* backward;
    };
    std::vector<Link> links;
    std::vector<Nary> nary;

    explicit Impl(const Rules& r)
        : rules(r)
        , units(detail::unitsOf(r))
        , peers(detail::peersOf(r))
    {
        for (int c = 0; c < 81; ++c)
            for (int p : peers[c])
                peerSet[c].set(p);
        for (int c = 0; c < 81; ++c) {
            Mask m = AllDigits;
            for (int d = 1; d <= 9; ++d)
                if ((r.parity[c] == 1 && d % 2) || (r.parity[c] == 2 && d % 2 == 0))
                    m &= static_cast<Mask>(~bit(d));
            unary[c] = m;
        }
        for (int k = 0; k < kRelations; ++k)
            for (int a = 1; a <= 9; ++a)
                for (int b = 1; b <= 9; ++b)
                    if (detail::holds(static_cast<detail::Relation>(k), a, b)) {
                        forward[k][a] |= bit(b);
                        backward[k][b] |= bit(a);
                    }
        for (const auto& e : detail::edgesOf(r)) {
            const auto k = static_cast<std::size_t>(e.relation);
            links.push_back({ e.a, e.b, &forward[k], &backward[k] });
        }
        for (const auto& cage : r.cages)
            nary.push_back({ NaryType::Cage, cage.cells, cage.sum });
        for (const auto& clue : r.littleKillers)
            nary.push_back({ NaryType::Sum, littleKillerCells(clue), clue.sum });
        for (const auto& arrow : r.arrows) {
            Nary n { NaryType::Arrow, { arrow.circle }, 0 };
            n.cells.insert(n.cells.end(), arrow.path.begin(), arrow.path.end());
            nary.push_back(n);
        }
        for (const auto& line : r.renbans)
            nary.push_back({ NaryType::Renban, line, 0 });
        for (int k = 0; k < 18; ++k) {
            const int clue = k < 9 ? r.rowSandwich[static_cast<std::size_t>(k)] : r.colSandwich[static_cast<std::size_t>(k - 9)];
            if (clue < 0)
                continue;
            Nary n { NaryType::Sandwich, {}, clue };
            for (int i = 0; i < 9; ++i)
                n.cells.push_back(k < 9 ? k * 9 + i : i * 9 + (k - 9));
            nary.push_back(n);
        }
    }

    bool initial(const Grid& givens, State& s) const
    {
        s.m = unary;
        for (int c = 0; c < 81; ++c) {
            const int g = givens[c];
            if (!g)
                continue;
            if (g > 9 || !(unary[c] & bit(g)))
                return false;
            s.m[c] = bit(g);
        }
        return true;
    }

    // Narrows `s` until nothing more follows. `full` adds the clue
    // reasoning and locked candidates to the singles. False on contradiction.
    bool propagate(State& s, bool full) const
    {
        for (;;) {
            bool changed = false;

            // A cell with one digit left: take it from its peers.
            for (int c = 0; c < 81; ++c) {
                const Mask mc = s.m[c];
                if (!mc)
                    return false;
                if (s.done[c] || !single(mc))
                    continue;
                s.done[c] = true;
                for (int p : peers[c]) {
                    if (s.m[p] & mc) {
                        s.m[p] &= static_cast<Mask>(~mc);
                        if (!s.m[p])
                            return false;
                        changed = true;
                    }
                }
            }
            if (changed)
                continue;

            // A digit with one place left in a unit.
            for (const auto& unit : units) {
                Mask once = 0, twice = 0;
                for (int c : unit) {
                    twice |= once & s.m[c];
                    once |= s.m[c];
                }
                if (once != AllDigits)
                    return false;
                for (Mask singles = once & ~twice; singles; singles &= singles - 1) {
                    const Mask b = singles & static_cast<Mask>(-singles);
                    int at = -1;
                    for (int c : unit)
                        if (s.m[c] & b)
                            at = c;
                    if (at < 0)
                        return false;
                    if (s.m[at] != b) {
                        s.m[at] = b;
                        changed = true;
                    }
                }
            }
            if (changed)
                continue;
            if (!full)
                return true;

            // Pairwise rules.
            for (const auto& link : links) {
                const Mask ma = s.m[link.a], mb = s.m[link.b];
                Mask allowA = 0;
                for (Mask w = mb; w; w &= w - 1)
                    allowA |= (*link.backward)[std::countr_zero(static_cast<unsigned>(w))];
                const Mask na = ma & allowA;
                Mask allowB = 0;
                for (Mask w = na; w; w &= w - 1)
                    allowB |= (*link.forward)[std::countr_zero(static_cast<unsigned>(w))];
                const Mask nb = mb & allowB;
                if (!na || !nb)
                    return false;
                if (na != ma || nb != mb) {
                    s.m[link.a] = na;
                    s.m[link.b] = nb;
                    changed = true;
                }
            }
            if (changed)
                continue;

            // Cages, arrows, sums, renban lines and sandwiches.
            for (const auto& k : nary)
                if (!revise(k, s, changed))
                    return false;
            if (changed)
                continue;

            // Locked candidates: every place left for a digit in a unit sees
            // some cell, so that cell can't hold it.
            for (const auto& unit : units) {
                for (int d = 1; d <= 9; ++d) {
                    const Mask b = bit(d);
                    Cells common { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } };
                    int count = 0;
                    for (int c : unit) {
                        if (s.m[c] & b) {
                            common &= peerSet[c];
                            ++count;
                        }
                    }
                    if (count < 2)
                        continue;
                    bool failed = false;
                    common.forEach([&](int x) {
                        if (x < 81 && (s.m[x] & b)) {
                            s.m[x] &= static_cast<Mask>(~b);
                            failed = failed || !s.m[x];
                            changed = true;
                        }
                    });
                    if (failed)
                        return false;
                }
            }
            if (!changed)
                return true;
        }
    }

    bool revise(const Nary& k, State& s, bool& changed) const
    {
        if (k.type == NaryType::Sandwich)
            return reviseSandwich(k, s, changed);

        const int n = static_cast<int>(k.cells.size());
        if (n > 16)
            return true;
        Search search { *this, k, n };
        for (int i = 0; i < n; ++i) {
            search.mask[i] = s.m[k.cells[static_cast<std::size_t>(i)]];
            search.remaining += popcount(search.mask[i]);
        }
        const int sumStart = k.type == NaryType::Arrow ? 1 : 0;
        search.minRest[n] = search.maxRest[n] = 0;
        for (int i = n - 1; i >= 0; --i) {
            const bool counted = i >= sumStart;
            search.minRest[i] = search.minRest[i + 1] + (counted ? firstDigit(search.mask[i]) : 0);
            search.maxRest[i] = search.maxRest[i + 1] + (counted ? highDigit(search.mask[i]) : 0);
        }
        search.run(0, 0, 10, 0, 0);
        if (search.aborted)
            return true;
        for (int i = 0; i < n; ++i) {
            if (search.support[i] != search.mask[i]) {
                if (!search.support[i])
                    return false;
                s.m[k.cells[static_cast<std::size_t>(i)]] = search.support[i];
                changed = true;
            }
        }
        return true;
    }

    // Finds, for every candidate of a clue's cells, a combination that uses
    // it, and stops once all are covered.
    struct Search {
        const Impl& impl;
        const Nary& k;
        int n;
        Mask mask[17] {};
        Mask support[17] {};
        int value[17] {};
        int minRest[18] {};
        int maxRest[18] {};
        int remaining = 0;
        int budget = kNaryBudget;
        bool aborted = false;
        bool finished = false;

        bool distinctAll() const { return k.type == NaryType::Cage || k.type == NaryType::Renban; }

        void run(int i, int sum, int lo, int hi, Mask used)
        {
            if (finished || aborted)
                return;
            if (--budget < 0) {
                aborted = true;
                return;
            }
            switch (k.type) {
            case NaryType::Cage:
            case NaryType::Sum:
                if (sum + minRest[i] > k.sum || sum + maxRest[i] < k.sum)
                    return;
                break;
            case NaryType::Arrow:
                if (i >= 1 && (sum + minRest[i] > value[0] || sum + maxRest[i] < value[0]))
                    return;
                break;
            case NaryType::Renban:
                if (hi > 0 && hi - lo > n - 1)
                    return;
                break;
            case NaryType::Sandwich:
                break;
            }
            if (i == n) {
                for (int j = 0; j < n; ++j) {
                    const Mask b = bit(value[j]);
                    if (!(support[j] & b)) {
                        support[j] |= b;
                        if (--remaining == 0)
                            finished = true;
                    }
                }
                return;
            }
            const int cell = k.cells[static_cast<std::size_t>(i)];
            for (Mask w = mask[i]; w; w &= w - 1) {
                const int v = std::countr_zero(static_cast<unsigned>(w));
                if (distinctAll()) {
                    if (used & bit(v))
                        continue;
                } else {
                    bool clash = false;
                    for (int j = 0; j < i && !clash; ++j)
                        clash = value[j] == v && impl.peerSet[cell].test(k.cells[static_cast<std::size_t>(j)]);
                    if (clash)
                        continue;
                }
                value[i] = v;
                const bool counted = k.type != NaryType::Arrow || i >= 1;
                run(i + 1, sum + (counted ? v : 0), std::min(lo, v), std::max(hi, v), used | bit(v));
                if (finished || aborted)
                    return;
            }
        }
    };

    // The digits between the 1 and the 9 of a row or column sum to the clue.
    bool reviseSandwich(const Nary& k, State& s, bool& changed) const
    {
        const Mask ends = bit(1) | bit(9);
        const Mask mid = AllDigits & static_cast<Mask>(~ends);
        Mask m[9], support[9] {};
        for (int i = 0; i < 9; ++i)
            m[i] = s.m[k.cells[static_cast<std::size_t>(i)]];
        for (int p = 0; p < 9; ++p) {
            for (int q = p + 1; q < 9; ++q) {
                for (int orient = 0; orient < 2; ++orient) {
                    const Mask atP = orient ? bit(9) : bit(1);
                    const Mask atQ = orient ? bit(1) : bit(9);
                    if (!(m[p] & atP) || !(m[q] & atQ))
                        continue;
                    int lo = 0, hi = 0;
                    bool ok = true;
                    for (int i = 0; i < 9 && ok; ++i) {
                        if (i == p || i == q)
                            continue;
                        const Mask mm = m[i] & mid;
                        if (!mm) {
                            ok = false;
                        } else if (i > p && i < q) {
                            lo += firstDigit(mm);
                            hi += highDigit(mm);
                        }
                    }
                    if (!ok || k.sum < lo || k.sum > hi)
                        continue;
                    support[p] |= atP;
                    support[q] |= atQ;
                    for (int i = 0; i < 9; ++i) {
                        if (i == p || i == q)
                            continue;
                        const Mask mm = m[i] & mid;
                        if (i < p || i > q) {
                            support[i] |= mm;
                            continue;
                        }
                        const int restLo = lo - firstDigit(mm), restHi = hi - highDigit(mm);
                        for (Mask w = mm; w; w &= w - 1) {
                            const int v = std::countr_zero(static_cast<unsigned>(w));
                            if (k.sum - v >= restLo && k.sum - v <= restHi)
                                support[i] |= bit(v);
                        }
                    }
                }
            }
        }
        for (int i = 0; i < 9; ++i) {
            if (support[i] != m[i]) {
                if (!support[i])
                    return false;
                s.m[k.cells[static_cast<std::size_t>(i)]] = support[i];
                changed = true;
            }
        }
        return true;
    }

    struct Count {
        int limit = 2;
        int found = 0;
        Grid solution {};
        calm::Rng* rng = nullptr;
        long nodes = 0;
        long nodeLimit = -1;
        bool gaveUp = false;
    };

    void search(State& s, Count& ctx) const
    {
        if (ctx.gaveUp || ctx.found >= ctx.limit)
            return;
        if (ctx.nodeLimit >= 0 && ++ctx.nodes > ctx.nodeLimit) {
            ctx.gaveUp = true;
            return;
        }
        if (!propagate(s, true))
            return;
        int best = -1, bestCount = 10;
        for (int c = 0; c < 81; ++c) {
            const int count = popcount(s.m[c]);
            if (count > 1 && count < bestCount) {
                best = c;
                bestCount = count;
                if (count == 2)
                    break;
            }
        }
        if (best < 0) {
            if (++ctx.found == 1)
                for (int c = 0; c < 81; ++c)
                    ctx.solution[c] = static_cast<std::uint8_t>(firstDigit(s.m[c]));
            return;
        }
        int digits[9];
        int n = 0;
        for (int d = 1; d <= 9; ++d)
            if (s.m[best] & bit(d))
                digits[n++] = d;
        if (ctx.rng)
            ctx.rng->shuffle(digits, digits + n);
        for (int i = 0; i < n && ctx.found < ctx.limit && !ctx.gaveUp; ++i) {
            State next = s;
            next.m[best] = bit(digits[i]);
            search(next, ctx);
        }
    }
};

VariantModel::VariantModel(const Rules& rules)
    : m(std::make_unique<Impl>(rules))
{
}

VariantModel::~VariantModel() = default;

const Rules& VariantModel::rules() const noexcept { return m->rules; }
const std::vector<int>& VariantModel::peers(int cell) const noexcept { return m->peers[static_cast<std::size_t>(cell)]; }
bool VariantModel::arePeers(int a, int b) const noexcept
{
    return detail::validCell(a) && detail::validCell(b) && m->peerSet[static_cast<std::size_t>(a)].test(b);
}
const std::vector<std::array<int, 9>>& VariantModel::units() const noexcept { return m->units; }
Mask VariantModel::allowed(int cell) const noexcept { return m->unary[static_cast<std::size_t>(cell)]; }

int VariantModel::countSolutions(const Grid& givens, int limit) const
{
    State s;
    if (!m->initial(givens, s))
        return 0;
    Impl::Count ctx;
    ctx.limit = limit;
    m->search(s, ctx);
    return ctx.found;
}

std::optional<Grid> VariantModel::solve(const Grid& givens) const
{
    State s;
    if (!m->initial(givens, s))
        return std::nullopt;
    Impl::Count ctx;
    ctx.limit = 1;
    m->search(s, ctx);
    if (!ctx.found)
        return std::nullopt;
    return ctx.solution;
}

std::optional<Grid> VariantModel::randomSolution(calm::Rng& rng, int nodeLimit) const
{
    State s;
    if (!m->initial(Grid {}, s))
        return std::nullopt;
    Impl::Count ctx;
    ctx.limit = 1;
    ctx.rng = &rng;
    ctx.nodeLimit = nodeLimit;
    m->search(s, ctx);
    if (!ctx.found)
        return std::nullopt;
    return ctx.solution;
}

VariantModel::Logic VariantModel::logicSolve(const Grid& givens) const
{
    Logic out;
    State s;
    if (!m->initial(givens, s) || !m->propagate(s, true)) {
        out.contradiction = true;
        return out;
    }
    out.candidates = s.m;
    out.solved = true;
    for (int c = 0; c < 81; ++c) {
        if (single(s.m[c]))
            out.grid[c] = static_cast<std::uint8_t>(firstDigit(s.m[c]));
        else
            out.solved = false;
    }
    return out;
}

std::optional<VariantModel::HintStep> VariantModel::hint(const Grid& position) const
{
    State s;
    if (!m->initial(position, s))
        return std::nullopt;

    // Simple steps first: candidates from the placed digits alone.
    std::array<Mask, 81> basic = s.m;
    for (int c = 0; c < 81; ++c)
        if (position[c])
            for (int p : m->peers[static_cast<std::size_t>(c)])
                basic[p] &= static_cast<Mask>(~bit(position[c]));
    for (int c = 0; c < 81; ++c)
        if (!position[c] && single(basic[c]))
            return HintStep { c, firstDigit(basic[c]) };
    for (const auto& unit : m->units) {
        for (int d = 1; d <= 9; ++d) {
            int at = -1, count = 0;
            for (int c : unit) {
                if (position[c] == d) {
                    count = -1;
                    break;
                }
                if (!position[c] && (basic[c] & bit(d))) {
                    at = c;
                    ++count;
                }
            }
            if (count == 1)
                return HintStep { at, d };
        }
    }

    // Then whatever full reasoning can fix.
    if (!m->propagate(s, true))
        return std::nullopt;
    for (int c = 0; c < 81; ++c)
        if (!position[c] && single(s.m[c]))
            return HintStep { c, firstDigit(s.m[c]) };
    return std::nullopt;
}

} // namespace sudoku
