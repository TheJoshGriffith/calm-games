// Tests for share-link payloads and typed puzzle codes, run by CTest.

#include <calm/Rng.h>
#include <sudoku/Generator.h>
#include <sudoku/Share.h>

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(expr)                                                                   \
    do {                                                                              \
        if (!(expr)) {                                                                \
            std::fprintf(stderr, "  FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

const char* kClassic = "530070000600195000098000060800060003400803001700020006060000280000419005000080079";
const char* kClassicSolution = "534678912672195348198342567859761423426853791713924856961537284287419635345286179";
const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

sudoku::SharedBoard startingBoard(sudoku::Difficulty difficulty, std::uint64_t seed)
{
    sudoku::SharedBoard board;
    const auto p = sudoku::generate(difficulty, seed);
    if (p) {
        board.givens = p->puzzle;
        board.level = difficulty;
    }
    return board;
}

// A mid-game Hard board: half the open cells filled from the solution and,
// optionally, corner marks on the rest, centre marks on a third of those and
// colours on every 7th cell.
sudoku::SharedBoard midGameBoard(bool withMarks)
{
    const auto p = *sudoku::generate(sudoku::Difficulty::Hard, 4);
    sudoku::SharedBoard board;
    board.givens = p.puzzle;
    board.level = sudoku::Difficulty::Hard;
    int open = 0;
    for (int i = 0; i < 81; ++i) {
        if (p.puzzle[i] != 0)
            continue;
        if (open++ % 2 == 0) {
            board.entries[i] = p.solution[i];
        } else if (withMarks) {
            board.corner[i] = static_cast<sudoku::Mask>(sudoku::bit(p.solution[i]) | sudoku::bit(p.solution[i] % 9 + 1));
            if (open % 3 == 0)
                board.centre[i] = static_cast<sudoku::Mask>(sudoku::bit(p.solution[i]) | sudoku::bit(1) | sudoku::bit(9));
        }
    }
    if (withMarks)
        for (int i = 0; i < 81; i += 7)
            board.colour[i] = static_cast<std::uint8_t>(i % 9 + 1);
    return board;
}

bool roundTrips(const sudoku::SharedBoard& board)
{
    const auto decoded = sudoku::decodeBoard(sudoku::encodeBoard(board));
    return decoded.has_value() && *decoded == board;
}

bool isUrlSafe(const std::string& s)
{
    for (char c : s)
        if (std::string_view(kAlphabet).find(c) == std::string_view::npos)
            return false;
    return true;
}

std::string bin(unsigned value, int width)
{
    std::string s;
    for (int i = width - 1; i >= 0; --i)
        s.push_back((value >> i) & 1 ? '1' : '0');
    return s;
}

// Builds a version-1 payload from a string of '0'/'1' bits.
std::string fromBits(const std::string& bits)
{
    std::string out = "1";
    for (std::size_t i = 0; i < bits.size(); i += 6) {
        int v = 0;
        for (std::size_t j = 0; j < 6; ++j)
            v = v * 2 + (i + j < bits.size() && bits[i + j] == '1');
        out.push_back(kAlphabet[v]);
    }
    return out;
}

std::string toBits(const std::string& payload)
{
    std::string bits;
    for (std::size_t i = 1; i < payload.size(); ++i)
        bits += bin(static_cast<unsigned>(std::string_view(kAlphabet).find(payload[i])), 6);
    return bits;
}

void shareRoundTrips()
{
    for (int level = 0; level < sudoku::DifficultyCount; ++level) {
        for (std::uint64_t seed = 1; seed <= 3; ++seed) {
            const auto board = startingBoard(static_cast<sudoku::Difficulty>(level), seed);
            const std::string payload = sudoku::encodeBoard(board);
            CHECK(payload[0] == sudoku::ShareFormatVersion);
            CHECK(isUrlSafe(payload.substr(1)));
            CHECK(roundTrips(board));
            std::printf("  %-6s start, seed %d: %2zu chars  %s\n", std::string(sudoku::difficultyName(*board.level)).c_str(),
                static_cast<int>(seed), payload.size(), payload.c_str());
        }
    }
    const auto mid = midGameBoard(false);
    const auto marked = midGameBoard(true);
    CHECK(roundTrips(mid));
    CHECK(roundTrips(marked));
    std::printf("  Hard mid-game, entries: %zu chars\n", sudoku::encodeBoard(mid).size());
    std::printf("  Hard mid-game, entries + marks + colours: %zu chars\n", sudoku::encodeBoard(marked).size());

    // No level, asymmetric givens.
    sudoku::SharedBoard classic;
    classic.givens = *sudoku::parseGrid(kClassic);
    CHECK(roundTrips(classic));
    CHECK(!sudoku::decodeBoard(sudoku::encodeBoard(classic))->level.has_value());
    std::printf("  classic (asymmetric, no level): %zu chars\n", sudoku::encodeBoard(classic).size());

    // Edge cases: empty grid, full grid, everything filled in.
    CHECK(roundTrips(sudoku::SharedBoard {}));
    sudoku::SharedBoard full;
    full.givens = *sudoku::parseGrid(kClassicSolution);
    full.colour.fill(9);
    CHECK(roundTrips(full));
    sudoku::SharedBoard busy;
    busy.level = sudoku::Difficulty::Expert;
    busy.corner.fill(sudoku::AllDigits);
    busy.centre.fill(sudoku::bit(5));
    for (int i = 0; i < 81; ++i) {
        busy.entries[i] = static_cast<std::uint8_t>(i % 9 + 1);
        busy.colour[i] = static_cast<std::uint8_t>(9 - i % 9);
    }
    CHECK(roundTrips(busy));
    std::printf("  worst case (all sections full): %zu chars\n", sudoku::encodeBoard(busy).size());

    // Every remainder of the digit packing (1, 2, 3 and 4 values).
    for (int n = 1; n <= 4; ++n) {
        sudoku::SharedBoard few;
        for (int i = 0; i < n; ++i)
            few.givens[i * 10] = static_cast<std::uint8_t>(9 - i);
        CHECK(roundTrips(few));
    }
}

void shareNormalisesWhatItCannotRepresent()
{
    const auto board = startingBoard(sudoku::Difficulty::Easy, 1);
    int givenCell = 0;
    while (board.givens[givenCell] == 0)
        ++givenCell;
    int openCell = 0;
    while (board.givens[openCell] != 0)
        ++openCell;

    auto dirty = board;
    dirty.entries[givenCell] = 3; // entries and marks on givens are dropped
    dirty.corner[givenCell] = sudoku::bit(2);
    dirty.centre[givenCell] = sudoku::bit(4);
    CHECK(sudoku::encodeBoard(dirty) == sudoku::encodeBoard(board));

    dirty = board;
    dirty.corner[openCell] = static_cast<sudoku::Mask>(sudoku::bit(3) | 0x8001); // bits 0 and 15 dropped
    dirty.entries[openCell] = 12; // out of range: dropped
    dirty.colour[openCell] = 200;
    const auto decoded = sudoku::decodeBoard(sudoku::encodeBoard(dirty));
    CHECK(decoded.has_value());
    if (decoded) {
        CHECK(decoded->corner[openCell] == sudoku::bit(3));
        CHECK(decoded->entries[openCell] == 0);
        CHECK(decoded->colour[openCell] == 0);
    }
}

void shareRejectsBadPayloads()
{
    const std::string good = sudoku::encodeBoard(midGameBoard(true));
    CHECK(sudoku::decodeBoard(good).has_value());

    CHECK(!sudoku::decodeBoard("").has_value());
    CHECK(!sudoku::decodeBoard("1").has_value());
    CHECK(!sudoku::decodeBoard("hello world").has_value());
    CHECK(!sudoku::decodeBoard("2" + good.substr(1)).has_value()); // unknown version
    CHECK(!sudoku::decodeBoard(good + "A").has_value()); // trailing data
    CHECK(!sudoku::decodeBoard(good + good).has_value());
    for (std::size_t n = 0; n < good.size(); ++n)
        CHECK(!sudoku::decodeBoard(good.substr(0, n)).has_value()); // every truncation
    std::string bad = good;
    bad[5] = '=';
    CHECK(!sudoku::decodeBoard(bad).has_value()); // outside the alphabet
    bad[5] = '+';
    CHECK(!sudoku::decodeBoard(bad).has_value()); // standard base64, not url-safe
    CHECK(!sudoku::decodeBoard(std::string(100000, 'A')).has_value()); // oversized

    const std::string emptySym = bin(0, 3) + "1" + "0000" + std::string(41, '0');
    CHECK(sudoku::decodeBoard(fromBits(emptySym)).has_value()); // sanity check of the helper
    // Level out of range (5 = past Expert).
    CHECK(!sudoku::decodeBoard(fromBits(bin(5, 3) + "1" + "0000" + std::string(41, '0'))).has_value());
    CHECK(!sudoku::decodeBoard(fromBits(bin(7, 3) + "1" + "0000" + std::string(41, '0'))).has_value());
    // One given (asymmetric), digit stored as 9: out of range. 8 (= digit 9) is fine.
    const std::string oneGiven = bin(0, 3) + "0" + "0000" + "1" + std::string(80, '0');
    CHECK(!sudoku::decodeBoard(fromBits(oneGiven + bin(9, 4))).has_value());
    CHECK(sudoku::decodeBoard(fromBits(oneGiven + bin(8, 4))).has_value());
    // Two givens packed as a pair >= 81.
    const std::string twoGivens = bin(0, 3) + "0" + "0000" + "11" + std::string(79, '0');
    CHECK(!sudoku::decodeBoard(fromBits(twoGivens + bin(81, 7))).has_value());
    // Inconsistent givens: two 5s in row 1. 5 and 6 is fine.
    CHECK(!sudoku::decodeBoard(fromBits(twoGivens + bin(4 * 9 + 4, 7))).has_value());
    CHECK(sudoku::decodeBoard(fromBits(twoGivens + bin(4 * 9 + 5, 7))).has_value());
    // Three givens packed as a triple >= 729.
    const std::string threeGivens = bin(0, 3) + "0" + "0000" + "111" + std::string(78, '0');
    CHECK(!sudoku::decodeBoard(fromBits(threeGivens + bin(729, 10))).has_value());
    CHECK(sudoku::decodeBoard(fromBits(threeGivens + bin(0 * 81 + 1 * 9 + 2, 10))).has_value());
    // Non-canonical: symmetric givens sent with the asymmetric bitmap.
    CHECK(!sudoku::decodeBoard(fromBits(bin(0, 3) + "0" + "0000" + std::string(81, '0'))).has_value());
    // A flagged section that turns out empty.
    CHECK(!sudoku::decodeBoard(fromBits(bin(0, 3) + "1" + "1000" + std::string(41, '0') + std::string(81, '0'))).has_value());
    // A corner mark present but with no digits.
    CHECK(!sudoku::decodeBoard(fromBits(bin(0, 3) + "1" + "0100" + std::string(41, '0') + "1" + std::string(80, '0') + bin(0, 9))).has_value());
    // Non-zero padding bits.
    {
        std::string payload = fromBits(emptySym);
        CHECK((emptySym.size() % 6) != 0);
        payload.back() = 'B';
        CHECK(!sudoku::decodeBoard(payload).has_value());
    }
    // Entries on given cells: entries are indexed over open cells, so the only
    // way to claim one is a stream with the entries flag and no open cells.
    {
        sudoku::SharedBoard full;
        full.givens = *sudoku::parseGrid(kClassicSolution);
        std::string bits = toBits(sudoku::encodeBoard(full));
        bits[4] = '1'; // entries flag
        CHECK(!sudoku::decodeBoard(fromBits(bits)).has_value());
        CHECK(!sudoku::decodeBoard(fromBits(bits + bin(1, 6))).has_value());
    }
}

void shareSurvivesFuzzing()
{
    calm::Rng rng(20261013);
    const std::string junk = std::string(kAlphabet) + "=+/ .%~\x01\x7f\xff";
    int accepted = 0;
    auto validate = [&](const std::string& input) {
        const auto board = sudoku::decodeBoard(input);
        if (!board)
            return;
        ++accepted;
        // Anything accepted must be well formed and canonical.
        CHECK(sudoku::isConsistent(board->givens));
        for (int i = 0; i < 81; ++i) {
            CHECK(board->givens[i] <= 9 && board->entries[i] <= 9 && board->colour[i] <= 9);
            CHECK((board->corner[i] & ~sudoku::AllDigits) == 0 && (board->centre[i] & ~sudoku::AllDigits) == 0);
            if (board->givens[i])
                CHECK(board->entries[i] == 0 && board->corner[i] == 0 && board->centre[i] == 0);
        }
        CHECK(sudoku::encodeBoard(*board) == input);
    };

    for (int i = 0; i < 20000; ++i) {
        std::string s;
        const auto length = rng.below(200);
        const auto range = static_cast<std::uint32_t>(i % 2 ? 64 : junk.size());
        for (std::uint32_t j = 0; j < length; ++j)
            s.push_back(junk[rng.below(range)]);
        if (i % 3 == 0 && !s.empty())
            s[0] = '1';
        validate(s);
    }

    std::vector<std::string> seeds;
    for (int level = 0; level < sudoku::DifficultyCount; ++level)
        seeds.push_back(sudoku::encodeBoard(startingBoard(static_cast<sudoku::Difficulty>(level), 2)));
    seeds.push_back(sudoku::encodeBoard(midGameBoard(false)));
    seeds.push_back(sudoku::encodeBoard(midGameBoard(true)));
    for (int i = 0; i < 40000; ++i) {
        std::string s = seeds[rng.below(static_cast<std::uint32_t>(seeds.size()))];
        const int edits = 1 + static_cast<int>(rng.below(4));
        for (int e = 0; e < edits && !s.empty(); ++e) {
            const auto pos = rng.below(static_cast<std::uint32_t>(s.size()));
            switch (rng.below(5)) {
            case 0: s[pos] = kAlphabet[rng.below(64)]; break;
            case 1: s[pos] = static_cast<char>(s[pos] ^ (1 << rng.below(7))); break;
            case 2: s.erase(pos, 1); break;
            case 3: s.insert(pos, 1, kAlphabet[rng.below(64)]); break;
            default: s.resize(pos); break;
            }
        }
        validate(s);
    }
    std::printf("  fuzz: %d of 60000 inputs decoded, all canonical\n", accepted);
}

void puzzleCodesRoundTrip()
{
    using sudoku::Difficulty;
    CHECK(sudoku::puzzleCode(Difficulty::Easy, 0) == "E1-00000000");
    CHECK(sudoku::puzzleCode(Difficulty::Expert, 31) == "X1-0000000Z");
    CHECK(sudoku::puzzleCode(Difficulty::Medium, sudoku::PuzzleSeedMask) == "M1-ZZZZZZZZ");
    CHECK(sudoku::puzzleCode(Difficulty::Hard, sudoku::PuzzleSeedMask + 1) == "H1-0000100000000");

    calm::Rng rng(99);
    for (int i = 0; i < 2000; ++i) {
        const auto d = static_cast<Difficulty>(i % sudoku::DifficultyCount);
        const std::uint64_t seed = i % 2 ? rng.next() : rng.next() & sudoku::PuzzleSeedMask;
        const std::string code = sudoku::puzzleCode(d, seed);
        CHECK(code.size() == (seed <= sudoku::PuzzleSeedMask ? 11u : 16u));
        const auto parsed = sudoku::parsePuzzleCode(code);
        CHECK(parsed.has_value() && parsed->first == d && parsed->second == seed);
    }
    const auto maxCode = sudoku::puzzleCode(Difficulty::Hard, ~std::uint64_t { 0 });
    CHECK(maxCode == "H1-FZZZZZZZZZZZZ");
    CHECK(sudoku::parsePuzzleCode(maxCode)->second == ~std::uint64_t { 0 });

    // Lenient input: lowercase, I/L/O look-alikes, extra dashes, spaces.
    const auto a = sudoku::parsePuzzleCode("h1-0123abyz");
    const auto b = sudoku::parsePuzzleCode("  H1-oi23-ABYZ ");
    const auto c = sudoku::parsePuzzleCode("H1-Ol23aByZ");
    CHECK(a.has_value() && b.has_value() && c.has_value());
    if (a && b && c) {
        CHECK(a->first == Difficulty::Hard);
        CHECK(a->second == b->second && b->second == c->second);
        CHECK(sudoku::puzzleCode(a->first, a->second) == "H1-0123ABYZ");
    }
    CHECK(sudoku::parsePuzzleCode("e1-00000000")->first == Difficulty::Easy);
    CHECK(sudoku::parsePuzzleCode("m1-00000000")->first == Difficulty::Medium);
    CHECK(sudoku::parsePuzzleCode("x1-00000000")->first == Difficulty::Expert);

    CHECK(!sudoku::parsePuzzleCode("").has_value());
    CHECK(!sudoku::parsePuzzleCode("H1").has_value());
    CHECK(!sudoku::parsePuzzleCode("H1-").has_value());
    CHECK(!sudoku::parsePuzzleCode("Q1-00000000").has_value()); // unknown level
    CHECK(!sudoku::parsePuzzleCode("H2-00000000").has_value()); // other generator version
    CHECK(!sudoku::parsePuzzleCode("H100000000").has_value()); // missing dash
    CHECK(!sudoku::parsePuzzleCode("H1-0000000").has_value()); // 7 characters
    CHECK(!sudoku::parsePuzzleCode("H1-000000000").has_value()); // 9 characters
    CHECK(!sudoku::parsePuzzleCode("H1-0000000U").has_value()); // U is not Crockford
    CHECK(!sudoku::parsePuzzleCode("H1-G000000000000").has_value()); // over 64 bits
    CHECK(!sudoku::parsePuzzleCode("H1-00000000000000").has_value()); // 14 characters
    CHECK(!sudoku::parsePuzzleCode("H1-0000*000").has_value());
    CHECK(!sudoku::parsePuzzleCode(std::string(1000, '-')).has_value());
}

// Puzzle codes only reproduce a puzzle while generate() stays deterministic.
// If this fails, the generator's output changed: bump GeneratorVersion in
// Generator.h and re-record these.
void generatorGoldenPuzzles()
{
    struct Golden {
        sudoku::Difficulty difficulty;
        std::uint64_t seed;
        const char* puzzle;
    };
    const Golden goldens[] = {
        { sudoku::Difficulty::Easy, 1, "187000354600300270000701089000000960048020710096000000870403000013005007465000123" },
        { sudoku::Difficulty::Medium, 4, "100000504006391007000002003600400000080603050000008006500800000800125300207000005" },
        { sudoku::Difficulty::Hard, 4, "018006020302100000060980000100003060006000900020600007000025080000001609030400170" },
        { sudoku::Difficulty::Expert, 5, "700230000034009000209140000070400038300000009150003020000081905000700160000062003" },
    };
    CHECK(sudoku::GeneratorVersion == 1);
    for (const auto& g : goldens) {
        const std::string code = sudoku::puzzleCode(g.difficulty, g.seed);
        const auto parsed = sudoku::parsePuzzleCode(code);
        CHECK(parsed.has_value());
        if (!parsed)
            continue;
        const auto p = sudoku::generate(parsed->first, parsed->second);
        CHECK(p.has_value());
        if (p && sudoku::formatGrid(p->puzzle) != g.puzzle) {
            std::fprintf(stderr, "  %s now generates %s\n", code.c_str(), sudoku::formatGrid(p->puzzle).c_str());
            CHECK(sudoku::formatGrid(p->puzzle) == g.puzzle);
        }
    }
}

} // namespace

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        { "share payload round trips", shareRoundTrips },
        { "share payload normalises what it cannot represent", shareNormalisesWhatItCannotRepresent },
        { "share payload rejects bad input", shareRejectsBadPayloads },
        { "share payload survives fuzzing", shareSurvivesFuzzing },
        { "puzzle codes round trip", puzzleCodesRoundTrip },
        { "generator golden puzzles", generatorGoldenPuzzles },
    };
    for (const auto& [name, fn] : tests) {
        const int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", name);
    }
    std::printf("%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
