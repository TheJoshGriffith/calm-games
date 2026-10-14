#include "sudoku/Variant.h"

#include "VariantDetail.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>

namespace sudoku {

namespace {

struct VariantInfo {
    std::string_view name;
    std::string_view id;
    std::string_view code;
};

constexpr std::array<VariantInfo, VariantCount> kVariants { {
    { "Classic", "classic", "" },
    { "Diagonal", "diagonal", "DG" },
    { "Anti-Knight", "anti-knight", "AN" },
    { "Anti-King", "anti-king", "AK" },
    { "Windoku", "windoku", "WD" },
    { "Non-Consecutive", "non-consecutive", "NC" },
    { "Jigsaw", "jigsaw", "JG" },
    { "Killer", "killer", "KL" },
    { "Thermo", "thermo", "TH" },
    { "Arrow", "arrow", "AR" },
    { "Sandwich", "sandwich", "SW" },
    { "Kropki", "kropki", "KR" },
    { "XV", "xv", "XV" },
    { "Even-Odd", "even-odd", "EO" },
    { "Little Killer", "little-killer", "LK" },
    { "German Whispers", "whispers", "GW" },
    { "Renban", "renban", "RB" },
    { "Palindrome", "palindrome", "PL" },
} };

bool equalsIgnoringCase(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        if (lower(a[i]) != lower(b[i]))
            return false;
    }
    return true;
}

int rc(int r, int c) noexcept { return r * 9 + c; }

} // namespace

std::string_view variantName(Variant variant) noexcept
{
    return kVariants[static_cast<std::size_t>(variant)].name;
}

std::string_view variantId(Variant variant) noexcept
{
    return kVariants[static_cast<std::size_t>(variant)].id;
}

std::string_view variantCode(Variant variant) noexcept
{
    return kVariants[static_cast<std::size_t>(variant)].code;
}

std::optional<Variant> parseVariant(std::string_view text) noexcept
{
    if (text.empty())
        return std::nullopt;
    for (int v = 0; v < VariantCount; ++v) {
        const auto& info = kVariants[static_cast<std::size_t>(v)];
        if (equalsIgnoringCase(text, info.name) || equalsIgnoringCase(text, info.id)
            || (!info.code.empty() && equalsIgnoringCase(text, info.code)))
            return static_cast<Variant>(v);
    }
    return std::nullopt;
}

std::vector<int> littleKillerCells(const LittleKillerClue& clue)
{
    std::vector<int> cells;
    if ((clue.dr != 1 && clue.dr != -1) || (clue.dc != 1 && clue.dc != -1))
        return cells;
    int r = clue.row + clue.dr, c = clue.col + clue.dc;
    while (r >= 0 && r < 9 && c >= 0 && c < 9) {
        cells.push_back(rc(r, c));
        r += clue.dr;
        c += clue.dc;
    }
    return cells;
}

std::array<std::uint8_t, 81> standardRegions() noexcept
{
    std::array<std::uint8_t, 81> region {};
    for (int i = 0; i < 81; ++i)
        region[i] = static_cast<std::uint8_t>(boxOf(i));
    return region;
}

Rules baseRules(Variant variant)
{
    Rules rules;
    rules.variant = variant;
    rules.region = standardRegions();
    rules.rowSandwich.fill(-1);
    rules.colSandwich.fill(-1);
    rules.diagonals = variant == Variant::Diagonal;
    rules.antiKnight = variant == Variant::AntiKnight;
    rules.antiKing = variant == Variant::AntiKing;
    rules.windoku = variant == Variant::Windoku;
    rules.nonConsecutive = variant == Variant::NonConsecutive;
    rules.dotsComplete = variant == Variant::Kropki || variant == Variant::XV;
    return rules;
}

bool validRegions(const std::array<std::uint8_t, 81>& region)
{
    std::array<int, 9> size {};
    for (auto r : region) {
        if (r > 8)
            return false;
        ++size[r];
    }
    for (int s : size)
        if (s != 9)
            return false;
    // Each region must be connected.
    for (int r = 0; r < 9; ++r) {
        int start = -1;
        for (int i = 0; i < 81 && start < 0; ++i)
            if (region[i] == r)
                start = i;
        std::array<bool, 81> seen {};
        std::vector<int> stack { start };
        seen[start] = true;
        int count = 0;
        while (!stack.empty()) {
            const int cell = stack.back();
            stack.pop_back();
            ++count;
            const int cr = rowOf(cell), cc = colOf(cell);
            const int next[4][2] = { { cr - 1, cc }, { cr + 1, cc }, { cr, cc - 1 }, { cr, cc + 1 } };
            for (const auto& n : next) {
                if (n[0] < 0 || n[0] > 8 || n[1] < 0 || n[1] > 8)
                    continue;
                const int o = rc(n[0], n[1]);
                if (!seen[o] && region[o] == r) {
                    seen[o] = true;
                    stack.push_back(o);
                }
            }
        }
        if (count != 9)
            return false;
    }
    return true;
}

namespace detail {

bool validCell(int cell) noexcept { return cell >= 0 && cell < 81; }

bool kingAdjacent(int a, int b) noexcept
{
    return a != b && std::abs(rowOf(a) - rowOf(b)) <= 1 && std::abs(colOf(a) - colOf(b)) <= 1;
}

bool holds(Relation relation, int a, int b) noexcept
{
    const int diff = a > b ? a - b : b - a;
    switch (relation) {
    case Relation::Less: return a < b;
    case Relation::Whisper: return diff >= 5;
    case Relation::Equal: return a == b;
    case Relation::Consecutive: return diff == 1;
    case Relation::Double: return a == 2 * b || b == 2 * a;
    case Relation::NoKropki: return diff != 1 && a != 2 * b && b != 2 * a;
    case Relation::Ten: return a + b == 10;
    case Relation::Five: return a + b == 5;
    case Relation::NoXV: return a + b != 10 && a + b != 5;
    case Relation::NotConsecutive: return diff != 1;
    }
    return false;
}

const std::vector<std::pair<int, int>>& orthogonalPairs()
{
    static const std::vector<std::pair<int, int>> pairs = [] {
        std::vector<std::pair<int, int>> out;
        for (int r = 0; r < 9; ++r)
            for (int c = 0; c < 8; ++c)
                out.emplace_back(rc(r, c), rc(r, c + 1));
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 9; ++c)
                out.emplace_back(rc(r, c), rc(r + 1, c));
        return out;
    }();
    return pairs;
}

std::vector<std::array<int, 9>> unitsOf(const Rules& rules)
{
    std::vector<std::array<int, 9>> out;
    for (int i = 0; i < 18; ++i)
        out.push_back(units()[static_cast<std::size_t>(i)]);
    std::array<int, 9> fill {};
    for (int r = 0; r < 9; ++r) {
        int n = 0;
        for (int i = 0; i < 81 && n < 9; ++i)
            if (rules.region[i] == r)
                fill[n++] = i;
        out.push_back(fill);
    }
    if (rules.diagonals) {
        for (int i = 0; i < 9; ++i)
            fill[i] = rc(i, i);
        out.push_back(fill);
        for (int i = 0; i < 9; ++i)
            fill[i] = rc(i, 8 - i);
        out.push_back(fill);
    }
    if (rules.windoku) {
        for (int br : { 1, 5 })
            for (int bc : { 1, 5 }) {
                int n = 0;
                for (int r = br; r < br + 3; ++r)
                    for (int c = bc; c < bc + 3; ++c)
                        fill[n++] = rc(r, c);
                out.push_back(fill);
            }
    }
    return out;
}

std::array<std::vector<int>, 81> peersOf(const Rules& rules)
{
    std::array<std::array<bool, 81>, 81> peer {};
    auto link = [&peer](int a, int b) {
        if (a != b && validCell(a) && validCell(b))
            peer[a][b] = peer[b][a] = true;
    };
    auto linkAll = [&link](const auto& cells) {
        for (std::size_t i = 0; i < cells.size(); ++i)
            for (std::size_t j = i + 1; j < cells.size(); ++j)
                link(cells[i], cells[j]);
    };
    for (const auto& unit : unitsOf(rules))
        linkAll(unit);
    for (int a = 0; a < 81; ++a) {
        for (int b = a + 1; b < 81; ++b) {
            const int dr = std::abs(rowOf(a) - rowOf(b)), dc = std::abs(colOf(a) - colOf(b));
            if (rules.antiKnight && ((dr == 1 && dc == 2) || (dr == 2 && dc == 1)))
                link(a, b);
            if (rules.antiKing && dr == 1 && dc == 1)
                link(a, b);
        }
    }
    for (const auto& cage : rules.cages)
        linkAll(cage.cells);
    for (const auto& line : rules.renbans)
        linkAll(line);
    // Thermometers strictly increase, so their cells all differ too.
    for (const auto& line : rules.thermos)
        linkAll(line);

    std::array<std::vector<int>, 81> out;
    for (int a = 0; a < 81; ++a)
        for (int b = 0; b < 81; ++b)
            if (peer[a][b])
                out[a].push_back(b);
    return out;
}

std::vector<Edge> edgesOf(const Rules& rules)
{
    std::vector<Edge> out;
    for (const auto& line : rules.thermos)
        for (std::size_t i = 0; i + 1 < line.size(); ++i)
            out.push_back({ line[i], line[i + 1], Relation::Less });
    for (const auto& line : rules.whispers)
        for (std::size_t i = 0; i + 1 < line.size(); ++i)
            out.push_back({ line[i], line[i + 1], Relation::Whisper });
    for (const auto& line : rules.palindromes)
        for (std::size_t i = 0; i < line.size() / 2; ++i)
            out.push_back({ line[i], line[line.size() - 1 - i], Relation::Equal });
    if (rules.nonConsecutive)
        for (const auto& [a, b] : orthogonalPairs())
            out.push_back({ a, b, Relation::NotConsecutive });

    auto relationOf = [](DotKind kind) {
        switch (kind) {
        case DotKind::White: return Relation::Consecutive;
        case DotKind::Black: return Relation::Double;
        case DotKind::X: return Relation::Ten;
        case DotKind::V: return Relation::Five;
        }
        return Relation::Consecutive;
    };
    for (const auto& dot : rules.dots)
        out.push_back({ dot.a, dot.b, relationOf(dot.kind) });
    if (rules.dotsComplete) {
        const Relation none = rules.variant == Variant::XV ? Relation::NoXV : Relation::NoKropki;
        for (const auto& [a, b] : orthogonalPairs()) {
            const bool marked = std::any_of(rules.dots.begin(), rules.dots.end(), [a = a, b = b](const Dot& d) {
                return (d.a == a && d.b == b) || (d.a == b && d.b == a);
            });
            if (!marked)
                out.push_back({ a, b, none });
        }
    }
    return out;
}

} // namespace detail

std::array<bool, 81> conflicts(const Rules& rules, const Grid& g)
{
    std::array<bool, 81> bad {};
    auto markFilled = [&bad, &g](const auto& cells) {
        for (int c : cells)
            if (detail::validCell(c) && g[c])
                bad[c] = true;
    };

    const auto peers = detail::peersOf(rules);
    for (int c = 0; c < 81; ++c) {
        if (!g[c])
            continue;
        if (g[c] > 9) {
            bad[c] = true;
            continue;
        }
        for (int p : peers[c])
            if (g[p] == g[c])
                bad[c] = true;
        if ((rules.parity[c] == 1 && g[c] % 2 != 0) || (rules.parity[c] == 2 && g[c] % 2 == 0))
            bad[c] = true;
    }
    for (const auto& e : detail::edgesOf(rules)) {
        if (g[e.a] && g[e.b] && !detail::holds(e.relation, g[e.a], g[e.b]))
            bad[e.a] = bad[e.b] = true;
    }
    for (const auto& line : rules.thermos) {
        const int n = static_cast<int>(line.size());
        for (int i = 0; i < n; ++i) {
            const int v = g[line[static_cast<std::size_t>(i)]];
            if (!v)
                continue;
            if (v < i + 1 || v > 9 - (n - 1 - i))
                bad[line[static_cast<std::size_t>(i)]] = true;
            for (int j = i + 1; j < n; ++j) {
                const int w = g[line[static_cast<std::size_t>(j)]];
                if (w && w - v < j - i)
                    bad[line[static_cast<std::size_t>(i)]] = bad[line[static_cast<std::size_t>(j)]] = true;
            }
        }
    }

    // A sum over `cells` that can no longer reach `target`; distinct cells
    // need different digits, others at least 1 each.
    auto checkSum = [&](const std::vector<int>& cells, int target, bool distinct) {
        int filled = 0, empty = 0;
        Mask used = 0;
        for (int c : cells) {
            if (g[c]) {
                filled += g[c];
                used |= bit(g[c]);
            } else {
                ++empty;
            }
        }
        int lo = 0, hi = 0;
        if (distinct) {
            int need = empty;
            for (int d = 1; d <= 9 && need > 0; ++d)
                if (!(used & bit(d))) {
                    lo += d;
                    --need;
                }
            need = empty;
            for (int d = 9; d >= 1 && need > 0; --d)
                if (!(used & bit(d))) {
                    hi += d;
                    --need;
                }
        } else {
            lo = empty;
            hi = 9 * empty;
        }
        if (filled + lo > target || filled + hi < target)
            markFilled(cells);
    };
    for (const auto& cage : rules.cages)
        checkSum(cage.cells, cage.sum, true);
    for (const auto& clue : rules.littleKillers)
        checkSum(littleKillerCells(clue), clue.sum, false);
    for (const auto& arrow : rules.arrows) {
        const int target = g[arrow.circle];
        int filled = 0, empty = 0;
        for (int c : arrow.path)
            g[c] ? filled += g[c] : ++empty;
        const bool broken = target ? (filled + empty > target || (empty == 0 && filled != target)) : filled + empty > 9;
        if (broken) {
            markFilled(arrow.path);
            if (target)
                bad[arrow.circle] = true;
        }
    }
    for (const auto& line : rules.renbans) {
        int lo = 10, hi = 0;
        for (int c : line)
            if (g[c]) {
                lo = std::min<int>(lo, g[c]);
                hi = std::max<int>(hi, g[c]);
            }
        if (hi > 0 && hi - lo > static_cast<int>(line.size()) - 1)
            markFilled(line);
    }
    for (int k = 0; k < 18; ++k) {
        const int clue = k < 9 ? rules.rowSandwich[static_cast<std::size_t>(k)] : rules.colSandwich[static_cast<std::size_t>(k - 9)];
        if (clue < 0)
            continue;
        std::vector<int> cells;
        for (int i = 0; i < 9; ++i)
            cells.push_back(k < 9 ? rc(k, i) : rc(i, k - 9));
        int one = -1, nine = -1;
        for (int i = 0; i < 9; ++i) {
            if (g[cells[static_cast<std::size_t>(i)]] == 1)
                one = i;
            if (g[cells[static_cast<std::size_t>(i)]] == 9)
                nine = i;
        }
        if (one < 0 || nine < 0)
            continue;
        const int from = std::min(one, nine), to = std::max(one, nine);
        int filled = 0, empty = 0;
        std::vector<int> span;
        for (int i = from; i <= to; ++i) {
            const int c = cells[static_cast<std::size_t>(i)];
            span.push_back(c);
            if (i == from || i == to)
                continue;
            g[c] ? filled += g[c] : ++empty;
        }
        if (filled + 2 * empty > clue || filled + 8 * empty < clue)
            markFilled(span);
    }
    return bad;
}

bool satisfies(const Rules& rules, const Grid& grid)
{
    if (clueCount(grid) != 81)
        return false;
    const auto bad = conflicts(rules, grid);
    return std::none_of(bad.begin(), bad.end(), [](bool b) { return b; });
}

// Text form: fields separated by ';', each a letter, ':' and a value.
// Lists use '/' between items and ',' between numbers.
//   v:<id>  r:<81 region digits>  f:<flags>  c:<sum>=<cells>/...
//   t:<cells>/...  a:<circle>=<path>/...  s:<18 sandwich sums, -1 none>
//   d:<a>,<b>,<w|b|x|v>/...  p:<81 parity digits>  l:<row>,<col>,<dr>,<dc>,<sum>/...
//   h:<whispers>  e:<renbans>  m:<palindromes>
namespace {

void appendCells(std::string& out, const std::vector<int>& cells)
{
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i)
            out += ',';
        out += std::to_string(cells[i]);
    }
}

void appendLines(std::string& out, char key, const std::vector<std::vector<int>>& lines)
{
    if (lines.empty())
        return;
    out += ';';
    out += key;
    out += ':';
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i)
            out += '/';
        appendCells(out, lines[i]);
    }
}

std::vector<std::string_view> split(std::string_view text, char sep)
{
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        const std::size_t at = text.find(sep, start);
        out.push_back(text.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start));
        if (at == std::string_view::npos)
            break;
        start = at + 1;
    }
    return out;
}

bool parseInt(std::string_view text, int& value)
{
    if (text.empty() || text.size() > 4)
        return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc() && result.ptr == text.data() + text.size();
}

bool parseCells(std::string_view text, std::vector<int>& cells, std::size_t minSize = 1)
{
    cells.clear();
    for (auto part : split(text, ',')) {
        int v = 0;
        if (!parseInt(part, v) || !detail::validCell(v))
            return false;
        if (std::find(cells.begin(), cells.end(), v) != cells.end())
            return false;
        cells.push_back(v);
    }
    return cells.size() >= minSize && cells.size() <= 81;
}

bool parseLines(std::string_view text, std::vector<std::vector<int>>& lines)
{
    for (auto part : split(text, '/')) {
        std::vector<int> cells;
        if (!parseCells(part, cells, 2))
            return false;
        // Lines step between touching cells.
        for (std::size_t i = 0; i + 1 < cells.size(); ++i)
            if (!detail::kingAdjacent(cells[i], cells[i + 1]))
                return false;
        lines.push_back(cells);
    }
    return true;
}

bool parseDigits81(std::string_view text, std::array<std::uint8_t, 81>& out, int max)
{
    if (text.size() != 81)
        return false;
    for (int i = 0; i < 81; ++i) {
        const int v = text[static_cast<std::size_t>(i)] - '0';
        if (v < 0 || v > max)
            return false;
        out[i] = static_cast<std::uint8_t>(v);
    }
    return true;
}

} // namespace

std::string encodeRules(const Rules& rules)
{
    std::string out = "v:";
    out += variantId(rules.variant);
    if (rules.region != standardRegions()) {
        out += ";r:";
        for (auto r : rules.region)
            out += static_cast<char>('0' + r);
    }
    std::string flags;
    if (rules.diagonals)
        flags += 'd';
    if (rules.antiKnight)
        flags += 'n';
    if (rules.antiKing)
        flags += 'k';
    if (rules.windoku)
        flags += 'w';
    if (rules.nonConsecutive)
        flags += 'c';
    if (rules.dotsComplete)
        flags += 'z';
    if (!flags.empty())
        out += ";f:" + flags;
    if (!rules.cages.empty()) {
        out += ";c:";
        for (std::size_t i = 0; i < rules.cages.size(); ++i) {
            if (i)
                out += '/';
            out += std::to_string(rules.cages[i].sum) + '=';
            appendCells(out, rules.cages[i].cells);
        }
    }
    appendLines(out, 't', rules.thermos);
    if (!rules.arrows.empty()) {
        out += ";a:";
        for (std::size_t i = 0; i < rules.arrows.size(); ++i) {
            if (i)
                out += '/';
            out += std::to_string(rules.arrows[i].circle) + '=';
            appendCells(out, rules.arrows[i].path);
        }
    }
    const bool anySandwich = std::any_of(rules.rowSandwich.begin(), rules.rowSandwich.end(), [](auto v) { return v >= 0; })
        || std::any_of(rules.colSandwich.begin(), rules.colSandwich.end(), [](auto v) { return v >= 0; });
    if (anySandwich) {
        out += ";s:";
        for (int i = 0; i < 18; ++i) {
            if (i)
                out += ',';
            out += std::to_string(i < 9 ? rules.rowSandwich[static_cast<std::size_t>(i)] : rules.colSandwich[static_cast<std::size_t>(i - 9)]);
        }
    }
    if (!rules.dots.empty()) {
        out += ";d:";
        for (std::size_t i = 0; i < rules.dots.size(); ++i) {
            if (i)
                out += '/';
            const auto& d = rules.dots[i];
            const char kind[] = { 'w', 'b', 'x', 'v' };
            out += std::to_string(d.a) + ',' + std::to_string(d.b) + ',' + kind[static_cast<int>(d.kind)];
        }
    }
    if (std::any_of(rules.parity.begin(), rules.parity.end(), [](auto v) { return v != 0; })) {
        out += ";p:";
        for (auto p : rules.parity)
            out += static_cast<char>('0' + p);
    }
    if (!rules.littleKillers.empty()) {
        out += ";l:";
        for (std::size_t i = 0; i < rules.littleKillers.size(); ++i) {
            if (i)
                out += '/';
            const auto& k = rules.littleKillers[i];
            out += std::to_string(k.row) + ',' + std::to_string(k.col) + ',' + std::to_string(k.dr) + ','
                + std::to_string(k.dc) + ',' + std::to_string(k.sum);
        }
    }
    appendLines(out, 'h', rules.whispers);
    appendLines(out, 'e', rules.renbans);
    appendLines(out, 'm', rules.palindromes);
    return out;
}

std::optional<Rules> decodeRules(std::string_view text)
{
    if (text.size() > 8192)
        return std::nullopt;
    const auto fields = split(text, ';');
    if (fields.empty() || fields[0].substr(0, 2) != "v:")
        return std::nullopt;
    const auto variant = parseVariant(fields[0].substr(2));
    if (!variant || variantId(*variant) != fields[0].substr(2))
        return std::nullopt;
    Rules rules = baseRules(*variant);
    rules.diagonals = rules.antiKnight = rules.antiKing = rules.windoku = rules.nonConsecutive = rules.dotsComplete = false;

    std::string seen;
    for (std::size_t f = 1; f < fields.size(); ++f) {
        const auto field = fields[f];
        if (field.size() < 3 || field[1] != ':' || seen.find(field[0]) != std::string::npos)
            return std::nullopt;
        seen += field[0];
        const auto value = field.substr(2);
        switch (field[0]) {
        case 'r':
            if (!parseDigits81(value, rules.region, 8) || !validRegions(rules.region))
                return std::nullopt;
            break;
        case 'f':
            for (char c : value) {
                bool* flag = c == 'd' ? &rules.diagonals : c == 'n' ? &rules.antiKnight : c == 'k' ? &rules.antiKing
                    : c == 'w' ? &rules.windoku : c == 'c' ? &rules.nonConsecutive : c == 'z' ? &rules.dotsComplete : nullptr;
                if (!flag || *flag)
                    return std::nullopt;
                *flag = true;
            }
            break;
        case 'c':
        case 'a':
            for (auto part : split(value, '/')) {
                const auto eq = part.find('=');
                if (eq == std::string_view::npos)
                    return std::nullopt;
                int head = 0;
                std::vector<int> cells;
                if (!parseInt(part.substr(0, eq), head) || !parseCells(part.substr(eq + 1), cells))
                    return std::nullopt;
                if (field[0] == 'c') {
                    if (head < 1 || head > 45 || cells.size() > 9)
                        return std::nullopt;
                    rules.cages.push_back({ cells, head });
                } else {
                    if (!detail::validCell(head) || std::find(cells.begin(), cells.end(), head) != cells.end())
                        return std::nullopt;
                    if (!detail::kingAdjacent(head, cells[0]))
                        return std::nullopt;
                    for (std::size_t i = 0; i + 1 < cells.size(); ++i)
                        if (!detail::kingAdjacent(cells[i], cells[i + 1]))
                            return std::nullopt;
                    rules.arrows.push_back({ head, cells });
                }
            }
            break;
        case 't':
            if (!parseLines(value, rules.thermos))
                return std::nullopt;
            break;
        case 'h':
            if (!parseLines(value, rules.whispers))
                return std::nullopt;
            break;
        case 'e':
            if (!parseLines(value, rules.renbans))
                return std::nullopt;
            break;
        case 'm':
            if (!parseLines(value, rules.palindromes))
                return std::nullopt;
            break;
        case 's': {
            const auto parts = split(value, ',');
            if (parts.size() != 18)
                return std::nullopt;
            for (int i = 0; i < 18; ++i) {
                int v = 0;
                if (!parseInt(parts[static_cast<std::size_t>(i)], v) || v < -1 || v > 35)
                    return std::nullopt;
                (i < 9 ? rules.rowSandwich[static_cast<std::size_t>(i)] : rules.colSandwich[static_cast<std::size_t>(i - 9)])
                    = static_cast<std::int8_t>(v);
            }
            break;
        }
        case 'd':
            for (auto part : split(value, '/')) {
                const auto parts = split(part, ',');
                int a = 0, b = 0;
                if (parts.size() != 3 || parts[2].size() != 1 || !parseInt(parts[0], a) || !parseInt(parts[1], b))
                    return std::nullopt;
                if (!detail::validCell(a) || !detail::validCell(b) || a >= b
                    || !((b == a + 1 && rowOf(a) == rowOf(b)) || b == a + 9))
                    return std::nullopt;
                const auto k = std::string_view("wbxv").find(parts[2][0]);
                if (k == std::string_view::npos)
                    return std::nullopt;
                rules.dots.push_back({ a, b, static_cast<DotKind>(k) });
            }
            break;
        case 'p':
            if (!parseDigits81(value, rules.parity, 2))
                return std::nullopt;
            break;
        case 'l':
            for (auto part : split(value, '/')) {
                const auto parts = split(part, ',');
                int v[5] {};
                if (parts.size() != 5)
                    return std::nullopt;
                for (int i = 0; i < 5; ++i)
                    if (!parseInt(parts[static_cast<std::size_t>(i)], v[i]))
                        return std::nullopt;
                const LittleKillerClue clue { v[0], v[1], v[2], v[3], v[4] };
                const bool outside = clue.row < 0 || clue.row > 8 || clue.col < 0 || clue.col > 8;
                if (!outside || clue.row < -1 || clue.row > 9 || clue.col < -1 || clue.col > 9 || littleKillerCells(clue).empty()
                    || clue.sum < 1 || clue.sum > 81)
                    return std::nullopt;
                rules.littleKillers.push_back(clue);
            }
            break;
        default:
            return std::nullopt;
        }
    }
    return rules;
}

} // namespace sudoku
