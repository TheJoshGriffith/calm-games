// Dependency-free tests for variant Sudoku: rules, solver, generator, codec.
// Run with CALM_VARIANT_BENCH=<n> to print generation times over n seeds.

#include <calm/Rng.h>
#include <sudoku/Share.h>
#include <sudoku/Solver.h>
#include <sudoku/Variant.h>
#include <sudoku/VariantGenerator.h>
#include <sudoku/VariantSolver.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
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

using sudoku::Variant;

const char* kSolution = "534678912672195348198342567859761423426853791713924856961537284287419635345286179";

std::vector<Variant> allVariants()
{
    std::vector<Variant> out;
    for (int v = 1; v < sudoku::VariantCount; ++v)
        out.push_back(static_cast<Variant>(v));
    return out;
}

void names()
{
    for (int v = 0; v < sudoku::VariantCount; ++v) {
        const auto variant = static_cast<Variant>(v);
        CHECK(sudoku::parseVariant(sudoku::variantName(variant)) == variant);
        CHECK(sudoku::parseVariant(sudoku::variantId(variant)) == variant);
        if (variant != Variant::Classic) {
            CHECK(sudoku::variantCode(variant).size() == 2);
            CHECK(sudoku::parseVariant(sudoku::variantCode(variant)) == variant);
        }
        for (int w = 0; w < v; ++w)
            CHECK(sudoku::variantCode(variant) != sudoku::variantCode(static_cast<Variant>(w)));
    }
    CHECK(sudoku::parseVariant("KILLER") == Variant::Killer);
    CHECK(!sudoku::parseVariant("Easy"));
    CHECK(!sudoku::parseVariant(""));
}

void globalRules()
{
    const auto grid = *sudoku::parseGrid(kSolution);
    CHECK(sudoku::satisfies(sudoku::baseRules(Variant::Classic), grid));
    // This classic grid has a repeated digit on its main diagonal (5, 7, ...).
    auto diagonal = sudoku::baseRules(Variant::Diagonal);
    CHECK(!sudoku::satisfies(diagonal, grid));

    // Two 5s a knight's move apart are flagged under anti-knight only.
    sudoku::Grid g {};
    g[2] = 5;
    g[13] = 5; // row 1, column 4: another box
    CHECK(!sudoku::conflicts(sudoku::baseRules(Variant::Classic), g)[2]);
    const auto knight = sudoku::conflicts(sudoku::baseRules(Variant::AntiKnight), g);
    CHECK(knight[2] && knight[13]);
    sudoku::Grid k {};
    k[20] = 3;
    k[30] = 3; // diagonal neighbours across a box corner
    CHECK(!sudoku::conflicts(sudoku::baseRules(Variant::Classic), k)[20]);
    CHECK(sudoku::conflicts(sudoku::baseRules(Variant::AntiKing), k)[30]);

    sudoku::Grid n {};
    n[0] = 4;
    n[1] = 5;
    CHECK(sudoku::conflicts(sudoku::baseRules(Variant::NonConsecutive), n)[0]);
    n[1] = 6;
    CHECK(!sudoku::conflicts(sudoku::baseRules(Variant::NonConsecutive), n)[0]);

    sudoku::Grid w {};
    w[10] = 7;
    w[30] = 7; // both in the top-left windoku box (rows 1-3, columns 1-3)
    CHECK(sudoku::conflicts(sudoku::baseRules(Variant::Windoku), w)[30]);
    CHECK(!sudoku::conflicts(sudoku::baseRules(Variant::Classic), w)[30]);
}

void clueRules()
{
    auto rules = sudoku::baseRules(Variant::Killer);
    rules.cages.push_back({ { 0, 1 }, 10 });
    sudoku::Grid g {};
    g[0] = 3;
    g[1] = 6;
    CHECK(sudoku::conflicts(rules, g)[0]);
    g[1] = 7;
    CHECK(!sudoku::conflicts(rules, g)[0]);
    g[1] = 0;
    g[0] = 9; // 9 + at least 1 > 10? no: 9 + 1 = 10 is fine
    CHECK(!sudoku::conflicts(rules, g)[0]);

    auto thermo = sudoku::baseRules(Variant::Thermo);
    thermo.thermos.push_back({ 0, 1, 2 });
    sudoku::Grid t {};
    t[0] = 5;
    t[2] = 6; // 5 _ 6 leaves nothing for the middle
    CHECK(sudoku::conflicts(thermo, t)[0] && sudoku::conflicts(thermo, t)[2]);
    t[2] = 7;
    CHECK(!sudoku::conflicts(thermo, t)[2]);
    sudoku::Grid tb {};
    tb[2] = 2; // the tip of a 3-cell thermo is at least 3
    CHECK(sudoku::conflicts(thermo, tb)[2]);

    auto arrow = sudoku::baseRules(Variant::Arrow);
    arrow.arrows.push_back({ 0, { 1, 2 } });
    sudoku::Grid a {};
    a[0] = 5;
    a[1] = 2;
    a[2] = 3;
    CHECK(!sudoku::conflicts(arrow, a)[0]);
    a[2] = 4;
    CHECK(sudoku::conflicts(arrow, a)[0]);

    auto sandwich = sudoku::baseRules(Variant::Sandwich);
    sandwich.rowSandwich[0] = 5;
    sudoku::Grid s {};
    s[0] = 1;
    s[1] = 2;
    s[2] = 3;
    s[3] = 9;
    CHECK(!sudoku::conflicts(sandwich, s)[1]);
    s[2] = 4;
    CHECK(sudoku::conflicts(sandwich, s)[1]);

    auto kropki = sudoku::baseRules(Variant::Kropki);
    kropki.dots.push_back({ 0, 1, sudoku::DotKind::Black });
    sudoku::Grid kr {};
    kr[0] = 3;
    kr[1] = 6;
    CHECK(!sudoku::conflicts(kropki, kr)[0]);
    kr[1] = 4;
    CHECK(sudoku::conflicts(kropki, kr)[0]);
    kr[9] = 5; // no dot between 0 and 9: 3 and 5 is fine
    CHECK(!sudoku::conflicts(kropki, kr)[9]);
    kr[9] = 4; // consecutive without a dot breaks the all-dots-shown rule
    CHECK(sudoku::conflicts(kropki, kr)[9]);

    auto xv = sudoku::baseRules(Variant::XV);
    xv.dots.push_back({ 0, 1, sudoku::DotKind::X });
    sudoku::Grid x {};
    x[0] = 3;
    x[1] = 7;
    CHECK(!sudoku::conflicts(xv, x)[0]);
    x[9] = 2; // 3 + 2 = 5 without a V
    CHECK(sudoku::conflicts(xv, x)[9]);

    auto parity = sudoku::baseRules(Variant::EvenOdd);
    parity.parity[0] = 1;
    sudoku::Grid p {};
    p[0] = 3;
    CHECK(sudoku::conflicts(parity, p)[0]);
    p[0] = 4;
    CHECK(!sudoku::conflicts(parity, p)[0]);

    auto whisper = sudoku::baseRules(Variant::Whispers);
    whisper.whispers.push_back({ 0, 10 });
    sudoku::Grid wh {};
    wh[0] = 2;
    wh[10] = 6;
    CHECK(sudoku::conflicts(whisper, wh)[10]);
    wh[10] = 7;
    CHECK(!sudoku::conflicts(whisper, wh)[10]);

    auto renban = sudoku::baseRules(Variant::Renban);
    renban.renbans.push_back({ 0, 1, 2 });
    sudoku::Grid r {};
    r[0] = 4;
    r[2] = 7;
    CHECK(sudoku::conflicts(renban, r)[0]);
    r[2] = 6;
    CHECK(!sudoku::conflicts(renban, r)[0]);

    auto palindrome = sudoku::baseRules(Variant::Palindrome);
    palindrome.palindromes.push_back({ 0, 10, 20, 30 });
    sudoku::Grid pl {};
    pl[0] = 4;
    pl[30] = 5;
    CHECK(sudoku::conflicts(palindrome, pl)[30]);

    auto little = sudoku::baseRules(Variant::LittleKiller);
    little.littleKillers.push_back({ -1, 5, 1, 1, 6 }); // cells (0,6) (1,7) (2,8)
    CHECK(sudoku::littleKillerCells(little.littleKillers[0]) == (std::vector<int> { 6, 16, 26 }));
    sudoku::Grid lk {};
    lk[6] = 5;
    lk[16] = 2;
    CHECK(sudoku::conflicts(little, lk)[6]);
}

void solverRespectsRules()
{
    // A classic puzzle solved by the variant model agrees with the classic solver.
    const auto puzzle = *sudoku::parseGrid("530070000600195000098000060800060003400803001700020006060000280000419005000080079");
    const sudoku::VariantModel classic(sudoku::baseRules(Variant::Classic));
    CHECK(classic.countSolutions(puzzle) == 1);
    CHECK(classic.solve(puzzle) == sudoku::solve(puzzle));
    CHECK(classic.logicSolve(puzzle).solved);

    // Every global variant has solutions from an empty grid.
    calm::Rng rng(7);
    for (auto v : { Variant::Diagonal, Variant::AntiKnight, Variant::AntiKing, Variant::Windoku, Variant::NonConsecutive }) {
        const auto rules = sudoku::baseRules(v);
        const sudoku::VariantModel model(rules);
        const auto grid = model.randomSolution(rng);
        CHECK(grid.has_value());
        if (grid)
            CHECK(sudoku::satisfies(rules, *grid));
    }
}

void codec()
{
    for (auto v : allVariants()) {
        const auto puzzle = sudoku::generateVariant(v, 12345);
        CHECK(puzzle.has_value());
        if (!puzzle)
            continue;
        const auto text = sudoku::encodeRules(puzzle->rules);
        const auto back = sudoku::decodeRules(text);
        CHECK(back.has_value());
        if (back)
            CHECK(*back == puzzle->rules);
    }
    CHECK(!sudoku::decodeRules(""));
    CHECK(!sudoku::decodeRules("v:nope"));
    CHECK(!sudoku::decodeRules("v:killer;c:10=0,81"));
    CHECK(!sudoku::decodeRules("v:thermo;t:0,40"));
    CHECK(!sudoku::decodeRules("v:killer;f:zz"));
    CHECK(sudoku::decodeRules("v:killer;c:10=0,1"));
}

void generatorMakesUniquePuzzles()
{
    for (auto v : allVariants()) {
        for (std::uint64_t seed : { 1ull, 2ull, 3ull }) {
            const auto puzzle = sudoku::generateVariant(v, seed);
            CHECK(puzzle.has_value());
            if (!puzzle)
                continue;
            CHECK(sudoku::satisfies(puzzle->rules, puzzle->solution));
            const sudoku::VariantModel model(puzzle->rules);
            CHECK(model.countSolutions(puzzle->puzzle, 2) == 1);
            CHECK(model.solve(puzzle->puzzle) == puzzle->solution);
            CHECK(sudoku::clueCount(puzzle->puzzle) == puzzle->clues);
            for (int i = 0; i < 81; ++i)
                CHECK(puzzle->puzzle[i] == 0 || puzzle->puzzle[i] == puzzle->solution[i]);
            // Same seed, same puzzle.
            const auto again = sudoku::generateVariant(v, seed);
            CHECK(again && again->rules == puzzle->rules && again->puzzle == puzzle->puzzle);
            // Clue variants really have their clues (several of each).
            const auto& r = puzzle->rules;
            int sandwiches = 0, shaded = 0;
            for (int i = 0; i < 9; ++i)
                sandwiches += (r.rowSandwich[static_cast<std::size_t>(i)] >= 0) + (r.colSandwich[static_cast<std::size_t>(i)] >= 0);
            for (auto p : r.parity)
                shaded += p != 0;
            const std::size_t clueItems = r.cages.size() + r.thermos.size() + r.arrows.size() + static_cast<std::size_t>(sandwiches)
                + r.dots.size() + static_cast<std::size_t>(shaded) + r.littleKillers.size() + r.whispers.size()
                + r.renbans.size() + r.palindromes.size();
            switch (v) {
            case Variant::Diagonal:
            case Variant::AntiKnight:
            case Variant::AntiKing:
            case Variant::Windoku:
            case Variant::NonConsecutive:
            case Variant::Jigsaw:
                CHECK(clueItems == 0);
                break;
            default:
                CHECK(clueItems >= 3);
                break;
            }
            if (v == Variant::Kropki || v == Variant::XV)
                CHECK(r.dotsComplete && r.dots.size() >= 10);
            if (v == Variant::Jigsaw)
                CHECK(puzzle->rules.region != sudoku::standardRegions() && sudoku::validRegions(puzzle->rules.region));
        }
        // Different seeds, different puzzles.
        CHECK(sudoku::generateVariant(v, 1)->solution != sudoku::generateVariant(v, 2)->solution);
    }
}

void hintsFollowTheSolution()
{
    for (auto v : { Variant::Killer, Variant::Jigsaw, Variant::Sandwich, Variant::Thermo }) {
        auto puzzle = *sudoku::generateVariant(v, 99);
        const sudoku::VariantModel model(puzzle.rules);
        sudoku::Grid position = puzzle.puzzle;
        int steps = 0;
        while (auto h = model.hint(position)) {
            CHECK(position[h->cell] == 0);
            CHECK(h->digit == puzzle.solution[h->cell]);
            position[h->cell] = static_cast<std::uint8_t>(h->digit);
            if (++steps > 81)
                break;
        }
        CHECK(position == puzzle.solution);
    }
}

void variantCodes()
{
    const std::string code = sudoku::variantPuzzleCode(Variant::Killer, 0x3ABCDEF12ull);
    CHECK(code.substr(0, 4) == "KL1-");
    const auto parsed = sudoku::parseVariantPuzzleCode(code);
    CHECK(parsed && parsed->first == Variant::Killer && parsed->second == 0x3ABCDEF12ull);
    CHECK(sudoku::parseVariantPuzzleCode(" kl1-" + code.substr(4) + " "));
    // Classic codes are not variant codes, and the other way round.
    CHECK(!sudoku::parseVariantPuzzleCode(sudoku::puzzleCode(sudoku::Difficulty::Hard, 5)));
    CHECK(!sudoku::parsePuzzleCode(code));
    CHECK(!sudoku::parseVariantPuzzleCode("KL9-00000001"));
    CHECK(!sudoku::parseVariantPuzzleCode("QQ1-00000001"));
    // XV starts with the Expert letter but is a variant.
    const auto xv = sudoku::parseVariantPuzzleCode(sudoku::variantPuzzleCode(Variant::XV, 77));
    CHECK(xv && xv->first == Variant::XV && xv->second == 77);

    sudoku::SharedBoard progress;
    progress.entries[5] = 3;
    progress.colour[7] = 2;
    const std::string payload = sudoku::encodeVariantBoard(Variant::Thermo, 42, progress);
    const auto board = sudoku::decodeVariantBoard(payload);
    CHECK(board && board->variant == Variant::Thermo && board->seed == 42 && board->board == progress);
    const auto bare = sudoku::decodeVariantBoard(sudoku::variantPuzzleCode(Variant::Thermo, 42));
    CHECK(bare && bare->seed == 42 && bare->board == sudoku::SharedBoard {});
    CHECK(!sudoku::decodeVariantBoard(sudoku::encodeBoard(progress)));
    CHECK(!sudoku::decodeVariantBoard(payload + "x"));
    CHECK(!sudoku::decodeBoard(payload));
}

// A fingerprint of every variant's puzzle for seed 4 (rules and givens).
// Pins generateVariant's output: if this fails, either revert the change or
// bump VariantGeneratorVersion and update the list (CALM_PRINT_GOLDEN=1
// prints it).
const char* kGolden[] = {
    "7cdf13bfa8fb03bf", // Diagonal
    "d60c50bf25b66777", // Anti-Knight
    "0e2f0823974a0b0f", // Anti-King
    "3135311a3cb7b021", // Windoku
    "4839398838aa0391", // Non-Consecutive
    "c07c02ed5f08ff15", // Jigsaw
    "4f793a1459b31c9f", // Killer
    "e64b7f7b2f3d5744", // Thermo
    "9e66b0415eb645a5", // Arrow
    "373db3680fd0e9a9", // Sandwich
    "10061aa2870d3f1e", // Kropki
    "2360e0d96290d808", // XV
    "7baa6c7fabc545b2", // Even-Odd
    "e4f962e9b0cb388d", // Little Killer
    "ef9db6d2b4b6134d", // German Whispers
    "88fc73f78b75f035", // Renban
    "0019bd4de30784e5", // Palindrome
};

std::uint64_t fingerprint(const std::string& text)
{
    std::uint64_t h = 14695981039346656037ull; // FNV-1a
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

void golden()
{
    const auto variants = allVariants();
    for (std::size_t i = 0; i < variants.size(); ++i) {
        const auto puzzle = sudoku::generateVariant(variants[i], 4);
        CHECK(puzzle.has_value());
        if (!puzzle)
            continue;
        char hex[17];
        std::snprintf(hex, sizeof hex, "%016llx",
            static_cast<unsigned long long>(fingerprint(sudoku::encodeRules(puzzle->rules) + sudoku::formatGrid(puzzle->puzzle))));
        if (std::getenv("CALM_PRINT_GOLDEN"))
            std::printf("    \"%s\", // %s\n", hex, std::string(sudoku::variantName(variants[i])).c_str());
        CHECK(i < std::size(kGolden) && std::string(hex) == kGolden[i]);
    }
}

void bench(int seeds)
{
    for (auto v : allVariants()) {
        double total = 0, worst = 0;
        int clues = 0, failed = 0;
        for (int s = 0; s < seeds; ++s) {
            const auto start = std::chrono::steady_clock::now();
            const auto puzzle = sudoku::generateVariant(v, static_cast<std::uint64_t>(1000 + s));
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            total += ms;
            worst = std::max(worst, ms);
            if (puzzle)
                clues += puzzle->clues;
            else
                ++failed;
        }
        std::printf("%-16s avg %7.1f ms  worst %7.1f ms  givens %5.1f  failed %d\n",
            std::string(sudoku::variantName(v)).c_str(), total / seeds, worst, double(clues) / seeds, failed);
    }
}

} // namespace

int main()
{
    if (const char* n = std::getenv("CALM_VARIANT_BENCH")) {
        bench(std::atoi(n));
        return 0;
    }
    const std::vector<std::pair<const char*, void (*)()>> tests {
        { "names", names },
        { "globalRules", globalRules },
        { "clueRules", clueRules },
        { "solverRespectsRules", solverRespectsRules },
        { "codec", codec },
        { "generatorMakesUniquePuzzles", generatorMakesUniquePuzzles },
        { "hintsFollowTheSolution", hintsFollowTheSolution },
        { "variantCodes", variantCodes },
        { "golden", golden },
    };
    for (const auto& [name, fn] : tests) {
        const int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", name);
    }
    if (g_failures)
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return g_failures ? 1 : 0;
}
