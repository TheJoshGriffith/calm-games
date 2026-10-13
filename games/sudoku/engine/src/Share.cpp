#include "sudoku/Share.h"

#include "sudoku/Generator.h"

#include <vector>

namespace sudoku {

namespace {

constexpr std::string_view kBase64Url = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr std::string_view kCrockford = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

// Longest payload worth looking at: a board with every section full is
// 397 characters.
constexpr std::size_t kMaxPayload = 512;

int base64Value(char c) noexcept
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '-')
        return 62;
    if (c == '_')
        return 63;
    return -1;
}

class BitWriter {
public:
    void write(std::uint32_t value, int bits)
    {
        for (int i = bits - 1; i >= 0; --i)
            m_bits.push_back(static_cast<std::uint8_t>((value >> i) & 1u));
    }

    std::string toBase64Url() const
    {
        std::string out;
        out.reserve((m_bits.size() + 5) / 6);
        for (std::size_t i = 0; i < m_bits.size(); i += 6) {
            int v = 0;
            for (std::size_t j = 0; j < 6; ++j)
                v = (v << 1) | (i + j < m_bits.size() ? m_bits[i + j] : 0);
            out.push_back(kBase64Url[static_cast<std::size_t>(v)]);
        }
        return out;
    }

private:
    std::vector<std::uint8_t> m_bits;
};

class BitReader {
public:
    // Fails (returns false) on any character outside the alphabet.
    bool load(std::string_view text)
    {
        m_bits.clear();
        m_bits.reserve(text.size() * 6);
        for (char c : text) {
            const int v = base64Value(c);
            if (v < 0)
                return false;
            for (int i = 5; i >= 0; --i)
                m_bits.push_back(static_cast<std::uint8_t>((v >> i) & 1));
        }
        return true;
    }

    // Reads `bits` bits into `value`; false if the stream is too short.
    bool read(int bits, std::uint32_t& value) noexcept
    {
        if (bits < 0 || static_cast<std::size_t>(bits) > m_bits.size() - m_pos)
            return false;
        value = 0;
        for (int i = 0; i < bits; ++i)
            value = (value << 1) | m_bits[m_pos++];
        return true;
    }

private:
    std::vector<std::uint8_t> m_bits;
    std::size_t m_pos = 0;
};

// Values 0..8, three to 10 bits, then a pair in 7 bits or a single in 4.
void writeNonary(BitWriter& w, const std::vector<std::uint32_t>& values)
{
    std::size_t i = 0;
    for (; i + 3 <= values.size(); i += 3)
        w.write(values[i] * 81 + values[i + 1] * 9 + values[i + 2], 10);
    const std::size_t rest = values.size() - i;
    if (rest == 2)
        w.write(values[i] * 9 + values[i + 1], 7);
    else if (rest == 1)
        w.write(values[i], 4);
}

bool readNonary(BitReader& r, std::size_t count, std::vector<std::uint32_t>& values)
{
    values.clear();
    std::uint32_t v = 0;
    while (count - values.size() >= 3) {
        if (!r.read(10, v) || v >= 729)
            return false;
        values.push_back(v / 81);
        values.push_back(v / 9 % 9);
        values.push_back(v % 9);
    }
    const std::size_t rest = count - values.size();
    if (rest == 2) {
        if (!r.read(7, v) || v >= 81)
            return false;
        values.push_back(v / 9);
        values.push_back(v % 9);
    } else if (rest == 1) {
        if (!r.read(4, v) || v >= 9)
            return false;
        values.push_back(v);
    }
    return true;
}

bool inDigitRange(int v) noexcept { return v >= 1 && v <= 9; }

// The non-given cells, in order: the domain for entries and marks.
std::vector<int> openCells(const Grid& givens)
{
    std::vector<int> cells;
    for (int i = 0; i < 81; ++i)
        if (givens[i] == 0)
            cells.push_back(i);
    return cells;
}

// Writes a digit-valued section (entries or colours) over `cells`.
template <typename Array>
void writeDigitSection(BitWriter& w, const Array& values, const std::vector<int>& cells)
{
    std::vector<std::uint32_t> digits;
    for (int cell : cells) {
        const bool present = inDigitRange(values[cell]);
        w.write(present ? 1 : 0, 1);
        if (present)
            digits.push_back(values[cell] - 1u);
    }
    writeNonary(w, digits);
}

template <typename Array>
bool readDigitSection(BitReader& r, Array& values, const std::vector<int>& cells)
{
    std::vector<int> present;
    std::uint32_t bitValue = 0;
    for (int cell : cells) {
        if (!r.read(1, bitValue))
            return false;
        if (bitValue)
            present.push_back(cell);
    }
    if (present.empty())
        return false; // a flagged section must hold something
    std::vector<std::uint32_t> digits;
    if (!readNonary(r, present.size(), digits))
        return false;
    for (std::size_t i = 0; i < present.size(); ++i)
        values[present[i]] = static_cast<typename Array::value_type>(digits[i] + 1);
    return true;
}

void writeMaskSection(BitWriter& w, const std::array<Mask, 81>& masks, const std::vector<int>& cells)
{
    for (int cell : cells)
        w.write((masks[cell] & AllDigits) ? 1 : 0, 1);
    for (int cell : cells) {
        const Mask m = masks[cell] & AllDigits;
        if (m)
            w.write(m >> 1, 9);
    }
}

bool readMaskSection(BitReader& r, std::array<Mask, 81>& masks, const std::vector<int>& cells)
{
    std::vector<int> present;
    std::uint32_t v = 0;
    for (int cell : cells) {
        if (!r.read(1, v))
            return false;
        if (v)
            present.push_back(cell);
    }
    if (present.empty())
        return false;
    for (int cell : present) {
        if (!r.read(9, v) || v == 0)
            return false;
        masks[cell] = static_cast<Mask>(v << 1);
    }
    return true;
}

bool anyDigit(const std::array<std::uint8_t, 81>& values, const std::vector<int>& cells)
{
    for (int cell : cells)
        if (inDigitRange(values[cell]))
            return true;
    return false;
}

bool anyMask(const std::array<Mask, 81>& masks, const std::vector<int>& cells)
{
    for (int cell : cells)
        if (masks[cell] & AllDigits)
            return true;
    return false;
}

char levelLetter(Difficulty d) noexcept
{
    switch (d) {
    case Difficulty::Easy: return 'E';
    case Difficulty::Medium: return 'M';
    case Difficulty::Hard: return 'H';
    case Difficulty::Expert: return 'X';
    }
    return '?';
}

int crockfordValue(char c) noexcept
{
    if (c >= 'a' && c <= 'z')
        c = static_cast<char>(c - 'a' + 'A');
    if (c == 'O')
        return 0;
    if (c == 'I' || c == 'L')
        return 1;
    for (std::size_t i = 0; i < kCrockford.size(); ++i)
        if (kCrockford[i] == c)
            return static_cast<int>(i);
    return -1;
}

} // namespace

std::string encodeBoard(const SharedBoard& board)
{
    // Normalise givens: anything outside 1..9 is treated as empty.
    Grid givens {};
    for (int i = 0; i < 81; ++i)
        givens[i] = inDigitRange(board.givens[i]) ? board.givens[i] : 0;
    const std::vector<int> open = openCells(givens);
    std::vector<int> all(81);
    for (int i = 0; i < 81; ++i)
        all[i] = i;

    bool symmetric = true;
    for (int i = 0; i < 81 && symmetric; ++i)
        symmetric = (givens[i] != 0) == (givens[80 - i] != 0);

    const bool hasEntries = anyDigit(board.entries, open);
    const bool hasCorner = anyMask(board.corner, open);
    const bool hasCentre = anyMask(board.centre, open);
    const bool hasColour = anyDigit(board.colour, all);

    BitWriter w;
    int level = 0;
    if (board.level && static_cast<int>(*board.level) < DifficultyCount)
        level = static_cast<int>(*board.level) + 1;
    w.write(static_cast<std::uint32_t>(level), 3);
    w.write(symmetric, 1);
    w.write(hasEntries, 1);
    w.write(hasCorner, 1);
    w.write(hasCentre, 1);
    w.write(hasColour, 1);

    const int bitmapCells = symmetric ? 41 : 81;
    for (int i = 0; i < bitmapCells; ++i)
        w.write(givens[i] != 0, 1);
    std::vector<std::uint32_t> digits;
    for (int i = 0; i < 81; ++i)
        if (givens[i])
            digits.push_back(givens[i] - 1u);
    writeNonary(w, digits);

    if (hasEntries)
        writeDigitSection(w, board.entries, open);
    if (hasCorner)
        writeMaskSection(w, board.corner, open);
    if (hasCentre)
        writeMaskSection(w, board.centre, open);
    if (hasColour)
        writeDigitSection(w, board.colour, all);

    return std::string(1, ShareFormatVersion) + w.toBase64Url();
}

std::optional<SharedBoard> decodeBoard(std::string_view payload)
{
    if (payload.size() < 2 || payload.size() > kMaxPayload || payload[0] != ShareFormatVersion)
        return std::nullopt;

    BitReader r;
    if (!r.load(payload.substr(1)))
        return std::nullopt;

    SharedBoard board;
    std::uint32_t v = 0;
    if (!r.read(3, v) || v > static_cast<std::uint32_t>(DifficultyCount))
        return std::nullopt;
    if (v > 0)
        board.level = static_cast<Difficulty>(v - 1);
    std::uint32_t flags = 0;
    if (!r.read(5, flags))
        return std::nullopt;
    const bool symmetric = flags & 0x10;
    const bool hasEntries = flags & 0x08;
    const bool hasCorner = flags & 0x04;
    const bool hasCentre = flags & 0x02;
    const bool hasColour = flags & 0x01;

    std::array<bool, 81> given {};
    const int bitmapCells = symmetric ? 41 : 81;
    for (int i = 0; i < bitmapCells; ++i) {
        if (!r.read(1, v))
            return std::nullopt;
        given[i] = v != 0;
        if (symmetric)
            given[80 - i] = given[i];
    }
    std::vector<std::uint32_t> digits;
    std::size_t count = 0;
    for (bool g : given)
        count += g;
    if (!readNonary(r, count, digits))
        return std::nullopt;
    for (int i = 0, k = 0; i < 81; ++i)
        if (given[i])
            board.givens[i] = static_cast<std::uint8_t>(digits[static_cast<std::size_t>(k++)] + 1);
    if (!isConsistent(board.givens))
        return std::nullopt;

    const std::vector<int> open = openCells(board.givens);
    std::vector<int> all(81);
    for (int i = 0; i < 81; ++i)
        all[i] = i;
    if (hasEntries && !readDigitSection(r, board.entries, open))
        return std::nullopt;
    if (hasCorner && !readMaskSection(r, board.corner, open))
        return std::nullopt;
    if (hasCentre && !readMaskSection(r, board.centre, open))
        return std::nullopt;
    if (hasColour && !readDigitSection(r, board.colour, all))
        return std::nullopt;

    // Accept only the canonical form: this rejects trailing characters,
    // non-zero padding bits and an unset symmetric flag on symmetric givens.
    if (encodeBoard(board) != payload)
        return std::nullopt;
    return board;
}

std::string puzzleCode(Difficulty difficulty, std::uint64_t seed)
{
    std::string code;
    code.push_back(levelLetter(difficulty));
    code.push_back(static_cast<char>('0' + GeneratorVersion));
    code.push_back('-');
    const int chars = seed <= PuzzleSeedMask ? 8 : 13;
    for (int i = chars - 1; i >= 0; --i)
        code.push_back(kCrockford[static_cast<std::size_t>((seed >> (5 * i)) & 31u)]);
    return code;
}

std::optional<std::pair<Difficulty, std::uint64_t>> parsePuzzleCode(std::string_view code)
{
    auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    while (!code.empty() && isSpace(code.front()))
        code.remove_prefix(1);
    while (!code.empty() && isSpace(code.back()))
        code.remove_suffix(1);
    if (code.size() < 3 || code.size() > 64)
        return std::nullopt;

    std::optional<Difficulty> difficulty;
    for (int d = 0; d < DifficultyCount; ++d) {
        const char letter = levelLetter(static_cast<Difficulty>(d));
        if (code[0] == letter || code[0] == letter - 'A' + 'a')
            difficulty = static_cast<Difficulty>(d);
    }
    if (!difficulty || code[1] != '0' + GeneratorVersion)
        return std::nullopt;
    code.remove_prefix(2);
    if (code.empty() || code[0] != '-')
        return std::nullopt;

    std::vector<int> values;
    for (char c : code) {
        if (c == '-')
            continue;
        const int value = crockfordValue(c);
        if (value < 0 || values.size() >= 13)
            return std::nullopt;
        values.push_back(value);
    }
    if (values.size() != 8 && values.size() != 13)
        return std::nullopt;
    // 13 characters hold 65 bits, so the leading one may only be 0..F.
    if (values.size() == 13 && values.front() > 15)
        return std::nullopt;
    std::uint64_t seed = 0;
    for (int value : values)
        seed = (seed << 5) | static_cast<std::uint64_t>(value);
    return std::make_pair(*difficulty, seed);
}

} // namespace sudoku
