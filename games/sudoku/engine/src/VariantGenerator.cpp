#include "sudoku/VariantGenerator.h"

#include "VariantDetail.h"
#include "sudoku/VariantSolver.h"

#include <calm/Rng.h>

#include <algorithm>
#include <cstdlib>
#include <numeric>

namespace sudoku {

namespace {

int rc(int r, int c) noexcept { return r * 9 + c; }

int randomIn(calm::Rng& rng, int lo, int hi) // inclusive
{
    return lo + static_cast<int>(rng.below(static_cast<std::uint32_t>(hi - lo + 1)));
}

std::vector<int> orthogonalNeighbours(int cell)
{
    std::vector<int> out;
    const int r = rowOf(cell), c = colOf(cell);
    if (r > 0)
        out.push_back(cell - 9);
    if (r < 8)
        out.push_back(cell + 9);
    if (c > 0)
        out.push_back(cell - 1);
    if (c < 8)
        out.push_back(cell + 1);
    return out;
}

std::vector<int> kingNeighbours(int cell)
{
    std::vector<int> out;
    const int r = rowOf(cell), c = colOf(cell);
    for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc)
            if ((dr || dc) && r + dr >= 0 && r + dr < 9 && c + dc >= 0 && c + dc < 9)
                out.push_back(rc(r + dr, c + dc));
    return out;
}

// Irregular regions: the 3x3 boxes, reshaped by swapping border cells
// between neighbouring regions while every region stays connected.
std::array<std::uint8_t, 81> randomRegions(calm::Rng& rng)
{
    auto region = standardRegions();
    int swaps = 0;
    for (int tries = 0; tries < 4000 && swaps < 70; ++tries) {
        const int a = static_cast<int>(rng.below(81));
        const auto near = orthogonalNeighbours(a);
        const int b = near[rng.below(static_cast<std::uint32_t>(near.size()))];
        const int from = region[a], to = region[b];
        if (from == to)
            continue;
        // a joins `to`; some cell of `to` touching `from` joins `from`.
        std::vector<int> options;
        for (int x = 0; x < 81; ++x) {
            if (region[x] != to || x == b)
                continue;
            for (int n : orthogonalNeighbours(x))
                if (n != a && region[n] == from) {
                    options.push_back(x);
                    break;
                }
        }
        if (options.empty())
            continue;
        const int x = options[rng.below(static_cast<std::uint32_t>(options.size()))];
        region[a] = static_cast<std::uint8_t>(to);
        region[x] = static_cast<std::uint8_t>(from);
        if (validRegions(region)) {
            ++swaps;
        } else {
            region[a] = static_cast<std::uint8_t>(from);
            region[x] = static_cast<std::uint8_t>(to);
        }
    }
    return region;
}

// Cells and diagonal crossings already used by lines, so lines never share a
// cell or cross each other between four cells.
struct Canvas {
    std::array<bool, 81> used {};
    std::array<std::uint8_t, 81> crossing {}; // per 2x2 block: 1 = '\', 2 = '/'

    static int blockOf(int a, int b) { return rc(std::min(rowOf(a), rowOf(b)), std::min(colOf(a), colOf(b))); }
    static std::uint8_t slantOf(int a, int b)
    {
        return (rowOf(b) - rowOf(a)) * (colOf(b) - colOf(a)) > 0 ? 1 : 2;
    }
    static bool diagonal(int a, int b) { return rowOf(a) != rowOf(b) && colOf(a) != colOf(b); }

    bool canStep(int a, int b) const
    {
        if (!diagonal(a, b))
            return true;
        return (crossing[blockOf(a, b)] & (3 - slantOf(a, b))) == 0;
    }
    void claim(const std::vector<int>& cells)
    {
        for (std::size_t i = 0; i < cells.size(); ++i) {
            used[cells[i]] = true;
            if (i + 1 < cells.size() && diagonal(cells[i], cells[i + 1]))
                crossing[blockOf(cells[i], cells[i + 1])] |= slantOf(cells[i], cells[i + 1]);
        }
    }
};

// A random walk through free, touching cells, each step allowed by `accept`.
template <typename Accept>
std::vector<int> walk(calm::Rng& rng, const Canvas& claimed, int start, int maxLen, Accept&& accept)
{
    // Steps so far count too, so a line never crosses itself.
    Canvas canvas = claimed;
    std::vector<int> path { start };
    while (static_cast<int>(path.size()) < maxLen) {
        std::vector<int> options;
        for (int n : kingNeighbours(path.back()))
            if (!canvas.used[n] && std::find(path.begin(), path.end(), n) == path.end() && canvas.canStep(path.back(), n)
                && accept(path, n))
                options.push_back(n);
        if (options.empty())
            break;
        const int next = options[rng.below(static_cast<std::uint32_t>(options.size()))];
        canvas.claim({ path.back(), next });
        path.push_back(next);
    }
    return path;
}

int freeCell(calm::Rng& rng, const Canvas& canvas)
{
    for (int tries = 0; tries < 200; ++tries) {
        const int c = static_cast<int>(rng.below(81));
        if (!canvas.used[c])
            return c;
    }
    return -1;
}

// Lines of `minLen`..`maxLen` cells whose steps `accept` allows and whose
// whole path `finish` approves, up to `count` of them.
template <typename Accept, typename Finish>
std::vector<std::vector<int>> lines(calm::Rng& rng, Canvas& canvas, int count, int minLen, int maxLen, Accept&& accept,
    Finish&& finish)
{
    std::vector<std::vector<int>> out;
    for (int tries = 0; tries < 400 && static_cast<int>(out.size()) < count; ++tries) {
        const int start = freeCell(rng, canvas);
        if (start < 0)
            break;
        const int target = randomIn(rng, minLen, maxLen);
        auto path = walk(rng, canvas, start, target, accept);
        if (static_cast<int>(path.size()) < minLen || !finish(path))
            continue;
        canvas.claim(path);
        out.push_back(path);
    }
    return out;
}

std::vector<Cage> killerCages(calm::Rng& rng, const Grid& solution)
{
    std::array<int, 81> cageOf;
    cageOf.fill(-1);
    std::vector<std::vector<int>> cages;
    std::vector<int> order(81);
    std::iota(order.begin(), order.end(), 0);
    rng.shuffle(order.begin(), order.end());
    for (int start : order) {
        if (cageOf[start] >= 0)
            continue;
        const int roll = static_cast<int>(rng.below(100));
        const int target = roll < 30 ? 2 : roll < 65 ? 3 : roll < 90 ? 4 : 5;
        std::vector<int> cage { start };
        Mask digits = bit(solution[start]);
        cageOf[start] = static_cast<int>(cages.size());
        while (static_cast<int>(cage.size()) < target) {
            std::vector<int> options;
            for (int c : cage)
                for (int n : orthogonalNeighbours(c))
                    if (cageOf[n] < 0 && !(digits & bit(solution[n])) && std::find(options.begin(), options.end(), n) == options.end())
                        options.push_back(n);
            if (options.empty())
                break;
            const int n = options[rng.below(static_cast<std::uint32_t>(options.size()))];
            cage.push_back(n);
            digits |= bit(solution[n]);
            cageOf[n] = static_cast<int>(cages.size());
        }
        cages.push_back(cage);
    }
    // Fold single cells into a neighbouring cage where the digit fits.
    for (std::size_t i = 0; i < cages.size(); ++i) {
        if (cages[i].size() != 1)
            continue;
        const int cell = cages[i][0];
        for (int n : orthogonalNeighbours(cell)) {
            auto& other = cages[static_cast<std::size_t>(cageOf[n])];
            if (&other == &cages[i] || other.size() >= 5)
                continue;
            const bool fits = std::none_of(other.begin(), other.end(), [&](int c) { return solution[c] == solution[cell]; });
            if (fits) {
                other.push_back(cell);
                cageOf[cell] = cageOf[n];
                cages[i].clear();
                break;
            }
        }
    }
    std::vector<Cage> out;
    for (auto& cells : cages) {
        if (cells.empty())
            continue;
        std::sort(cells.begin(), cells.end());
        int sum = 0;
        for (int c : cells)
            sum += solution[c];
        out.push_back({ cells, sum });
    }
    std::sort(out.begin(), out.end(), [](const Cage& a, const Cage& b) { return a.cells[0] < b.cells[0]; });
    return out;
}

std::vector<ArrowClue> arrowClues(calm::Rng& rng, Canvas& canvas, const Grid& solution, int count)
{
    std::vector<ArrowClue> out;
    for (int tries = 0; tries < 600 && static_cast<int>(out.size()) < count; ++tries) {
        const int circle = freeCell(rng, canvas);
        if (circle < 0)
            break;
        const int target = solution[circle];
        if (target < 3)
            continue;
        const int length = randomIn(rng, 2, std::min(3, target - 1));
        // Random depth-first search for a path of `length` cells summing to target.
        std::vector<int> path;
        std::vector<int> best;
        auto dfs = [&](auto&& self, int from, int sum) -> bool {
            if (static_cast<int>(path.size()) == length)
                return sum == target;
            auto options = kingNeighbours(from);
            rng.shuffle(options.begin(), options.end());
            for (int n : options) {
                if (canvas.used[n] || n == circle || std::find(path.begin(), path.end(), n) != path.end() || !canvas.canStep(from, n))
                    continue;
                if (sum + solution[n] + (length - 1 - static_cast<int>(path.size())) > target)
                    continue;
                path.push_back(n);
                if (self(self, n, sum + solution[n]))
                    return true;
                path.pop_back();
            }
            return false;
        };
        if (!dfs(dfs, circle, 0))
            continue;
        std::vector<int> all { circle };
        all.insert(all.end(), path.begin(), path.end());
        canvas.claim(all);
        out.push_back({ circle, path });
    }
    return out;
}

std::vector<std::vector<int>> palindromeLines(calm::Rng& rng, Canvas& canvas, const Grid& solution, int count)
{
    std::vector<std::vector<int>> out;
    for (int tries = 0; tries < 1500 && static_cast<int>(out.size()) < count; ++tries) {
        const int centre = freeCell(rng, canvas);
        if (centre < 0)
            break;
        const int target = randomIn(rng, 4, 8);
        std::vector<int> path { centre };
        // An even line starts from two diagonal neighbours holding the same digit.
        if (target % 2 == 0) {
            int partner = -1;
            for (int n : kingNeighbours(centre))
                if (!canvas.used[n] && solution[n] == solution[centre] && canvas.canStep(centre, n))
                    partner = n;
            if (partner < 0)
                continue;
            path.push_back(partner);
        }
        while (static_cast<int>(path.size()) + 2 <= target) {
            std::vector<std::pair<int, int>> options;
            for (int x : kingNeighbours(path.front()))
                for (int y : kingNeighbours(path.back())) {
                    if (x == y || canvas.used[x] || canvas.used[y] || solution[x] != solution[y])
                        continue;
                    if (std::find(path.begin(), path.end(), x) != path.end() || std::find(path.begin(), path.end(), y) != path.end())
                        continue;
                    if (!canvas.canStep(path.front(), x) || !canvas.canStep(path.back(), y))
                        continue;
                    // The two new steps must not cross each other.
                    if (Canvas::diagonal(path.front(), x) && Canvas::diagonal(path.back(), y)
                        && Canvas::blockOf(path.front(), x) == Canvas::blockOf(path.back(), y))
                        continue;
                    options.emplace_back(x, y);
                }
            if (options.empty())
                break;
            const auto [x, y] = options[rng.below(static_cast<std::uint32_t>(options.size()))];
            path.insert(path.begin(), x);
            path.push_back(y);
        }
        if (static_cast<int>(path.size()) < 4)
            continue;
        // Steps inside the path must not cross each other either.
        Canvas probe = canvas;
        bool clean = true;
        for (std::size_t i = 0; i + 1 < path.size() && clean; ++i) {
            clean = probe.canStep(path[i], path[i + 1]);
            probe.claim({ path[i], path[i + 1] });
        }
        if (!clean)
            continue;
        canvas.claim(path);
        out.push_back(path);
    }
    return out;
}

std::vector<LittleKillerClue> littleKillerClues(calm::Rng& rng, const Grid& solution, int count)
{
    std::vector<LittleKillerClue> options;
    for (int r = -1; r <= 9; ++r)
        for (int c = -1; c <= 9; ++c) {
            if (r >= 0 && r <= 8 && c >= 0 && c <= 8)
                continue;
            for (int dr : { -1, 1 })
                for (int dc : { -1, 1 }) {
                    const LittleKillerClue clue { r, c, dr, dc, 0 };
                    const auto cells = littleKillerCells(clue);
                    if (cells.size() >= 3 && cells.size() <= 8)
                        options.push_back(clue);
                }
        }
    rng.shuffle(options.begin(), options.end());
    std::vector<LittleKillerClue> out;
    std::vector<std::vector<int>> taken;
    std::vector<int> spots;
    for (auto clue : options) {
        if (static_cast<int>(out.size()) >= count)
            break;
        auto cells = littleKillerCells(clue);
        auto key = cells;
        std::sort(key.begin(), key.end());
        if (std::find(taken.begin(), taken.end(), key) != taken.end())
            continue;
        const int spot = (clue.row + 1) * 11 + (clue.col + 1);
        if (std::find(spots.begin(), spots.end(), spot) != spots.end())
            continue;
        clue.sum = 0;
        for (int cell : cells)
            clue.sum += solution[cell];
        taken.push_back(key);
        spots.push_back(spot);
        out.push_back(clue);
    }
    return out;
}

bool isGlobal(Variant v)
{
    switch (v) {
    case Variant::Diagonal:
    case Variant::AntiKnight:
    case Variant::AntiKing:
    case Variant::Windoku:
    case Variant::NonConsecutive:
    case Variant::Jigsaw:
        return true;
    default:
        return false;
    }
}

// Reads the variant's clues off a solved grid.
void addClues(Rules& rules, calm::Rng& rng, const Grid& solution)
{
    Canvas canvas;
    auto digitRange = [&solution](const std::vector<int>& cells) {
        int lo = 10, hi = 0;
        for (int c : cells) {
            lo = std::min<int>(lo, solution[c]);
            hi = std::max<int>(hi, solution[c]);
        }
        return hi - lo;
    };
    switch (rules.variant) {
    case Variant::Killer:
        rules.cages = killerCages(rng, solution);
        break;
    case Variant::Thermo:
        rules.thermos = lines(rng, canvas, 8, 3, 7,
            [&](const std::vector<int>& path, int n) { return solution[n] > solution[path.back()]; },
            [](const std::vector<int>&) { return true; });
        break;
    case Variant::Arrow:
        rules.arrows = arrowClues(rng, canvas, solution, 8);
        break;
    case Variant::Sandwich:
        for (int k = 0; k < 9; ++k) {
            for (int axis = 0; axis < 2; ++axis) {
                int one = -1, nine = -1;
                for (int i = 0; i < 9; ++i) {
                    const int cell = axis == 0 ? rc(k, i) : rc(i, k);
                    if (solution[cell] == 1)
                        one = i;
                    if (solution[cell] == 9)
                        nine = i;
                }
                int sum = 0;
                for (int i = std::min(one, nine) + 1; i < std::max(one, nine); ++i)
                    sum += solution[axis == 0 ? rc(k, i) : rc(i, k)];
                (axis == 0 ? rules.rowSandwich : rules.colSandwich)[static_cast<std::size_t>(k)] = static_cast<std::int8_t>(sum);
            }
        }
        break;
    case Variant::Kropki:
        for (const auto& [a, b] : detail::orthogonalPairs()) {
            const int x = solution[a], y = solution[b];
            if (x == 2 * y || y == 2 * x)
                rules.dots.push_back({ a, b, DotKind::Black });
            else if (std::abs(x - y) == 1)
                rules.dots.push_back({ a, b, DotKind::White });
        }
        break;
    case Variant::XV:
        for (const auto& [a, b] : detail::orthogonalPairs()) {
            const int sum = solution[a] + solution[b];
            if (sum == 10)
                rules.dots.push_back({ a, b, DotKind::X });
            else if (sum == 5)
                rules.dots.push_back({ a, b, DotKind::V });
        }
        break;
    case Variant::EvenOdd: {
        std::vector<int> order(81);
        std::iota(order.begin(), order.end(), 0);
        rng.shuffle(order.begin(), order.end());
        for (int i = 0; i < 26; ++i)
            rules.parity[order[static_cast<std::size_t>(i)]] = solution[order[static_cast<std::size_t>(i)]] % 2 == 0 ? 1 : 2;
        break;
    }
    case Variant::LittleKiller:
        rules.littleKillers = littleKillerClues(rng, solution, 10);
        break;
    case Variant::Whispers:
        rules.whispers = lines(rng, canvas, 7, 3, 8,
            [&](const std::vector<int>& path, int n) { return std::abs(solution[n] - solution[path.back()]) >= 5; },
            [](const std::vector<int>&) { return true; });
        break;
    case Variant::Renban:
        rules.renbans = lines(rng, canvas, 8, 3, 5,
            [&](const std::vector<int>& path, int n) {
                for (int c : path)
                    if (solution[c] == solution[n])
                        return false;
                auto with = path;
                with.push_back(n);
                return digitRange(with) <= 4;
            },
            [&](const std::vector<int>& path) { return digitRange(path) == static_cast<int>(path.size()) - 1; });
        break;
    case Variant::Palindrome:
        rules.palindromes = palindromeLines(rng, canvas, solution, 6);
        break;
    default:
        break;
    }
}

} // namespace

std::optional<VariantPuzzle> generateVariant(Variant variant, std::uint64_t seed, int maxAttempts)
{
    if (variant == Variant::Classic)
        return std::nullopt;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        calm::Rng rng(calm::Rng::mix(seed ^ (std::uint64_t { static_cast<std::uint8_t>(variant) } << 56), static_cast<std::uint64_t>(attempt)));
        Rules rules = baseRules(variant);
        if (variant == Variant::Jigsaw)
            rules.region = randomRegions(rng);

        std::optional<Grid> solution;
        {
            // The global rules only: "every dot is shown" means nothing until
            // the dots are read off the solution.
            Rules globalRules = rules;
            globalRules.dotsComplete = false;
            const VariantModel global(globalRules);
            // Short searches restarted often find a grid far sooner than one long
            // search stuck in a dead end (Non-Consecutive especially).
            const int restarts = variant == Variant::NonConsecutive ? 400 : 30;
            for (int restart = 0; restart < restarts && !solution; ++restart)
                solution = global.randomSolution(rng, 120);
        }
        if (!solution)
            continue;
        addClues(rules, rng, *solution);

        const VariantModel model(rules);
        // Keep only puzzles that reasoning can finish from the full clue set
        // plus some givens; dig givens out while it still can.
        std::vector<std::vector<int>> groups;
        if (isGlobal(variant)) {
            for (int i = 0; i <= 40; ++i)
                groups.push_back(i == 40 ? std::vector<int> { 40 } : std::vector<int> { i, 80 - i });
        } else {
            for (int i = 0; i < 81; ++i)
                groups.push_back({ i });
        }
        rng.shuffle(groups.begin(), groups.end());

        Grid puzzle = *solution;
        int clues = 81;
        for (const auto& group : groups) {
            for (int cell : group)
                puzzle[cell] = 0;
            if (model.logicSolve(puzzle).solved) {
                clues -= static_cast<int>(group.size());
            } else {
                for (int cell : group)
                    puzzle[cell] = (*solution)[cell];
            }
        }
        return VariantPuzzle { rules, puzzle, *solution, seed, clues };
    }
    return std::nullopt;
}

} // namespace sudoku
