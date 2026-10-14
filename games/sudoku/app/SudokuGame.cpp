#include "SudokuGame.h"

#include <sudoku/Board.h>
#include <sudoku/Grader.h>
#include <sudoku/Share.h>
#include <sudoku/Solver.h>

#include <QRandomGenerator>
#include <QSet>
#include <QUrl>

#if CALM_SUDOKU_THREADED_GENERATION
#include <QtConcurrent/QtConcurrentRun>
#endif

#include <algorithm>

using sudoku::Difficulty;
using sudoku::Variant;

namespace {

QString gridToString(const sudoku::Grid& grid)
{
    return QString::fromStdString(sudoku::formatGrid(grid));
}

std::optional<sudoku::Grid> gridFromString(const QString& text)
{
    return sudoku::parseGrid(text.toStdString());
}

QVariantList masksToList(const std::array<sudoku::Mask, 81>& masks)
{
    QVariantList out;
    out.reserve(81);
    for (auto m : masks)
        out.append(int(m));
    return out;
}

bool masksFromList(const QVariantList& list, std::array<sudoku::Mask, 81>& masks)
{
    if (list.size() != 81)
        return false;
    for (int i = 0; i < 81; ++i)
        masks[i] = static_cast<sudoku::Mask>(list.at(i).toInt() & sudoku::AllDigits);
    return true;
}

const QString kMistakeMode = QStringLiteral("sudoku/mistakeMode");
const QString kPadWidth = QStringLiteral("sudoku/padWidth");
const QString kPadOffset = QStringLiteral("sudoku/padOffset");
// The boolean option this replaced: true meant "compare with the answer".
const QString kLegacyShowMistakes = QStringLiteral("sudoku/showMistakes");

QString levelName(Difficulty difficulty)
{
    return QString::fromUtf8(sudoku::difficultyName(difficulty).data());
}

QString fromView(std::string_view text)
{
    return QString::fromUtf8(text.data(), qsizetype(text.size()));
}

// The rules of classic Sudoku, compiled once and shared by every classic board.
std::shared_ptr<const sudoku::VariantModel> classicModel()
{
    static const auto model = std::make_shared<const sudoku::VariantModel>(sudoku::baseRules(Variant::Classic));
    return model;
}

// The payload of a share link, from the whole URL (any host, so preview
// deployments work too) or the bare payload.
QString sharePayload(const QString& link)
{
    const QString text = link.trimmed();
    if (text.contains(QLatin1String("://"))) {
        const QUrl url(text);
        const QString path = url.path();
        if (!url.isValid() || !path.startsWith(QLatin1String("/s/")))
            return {};
        QString payload = path.mid(3);
        while (payload.endsWith(QLatin1Char('/')))
            payload.chop(1);
        return payload.contains(QLatin1Char('/')) ? QString() : payload;
    }
    return text.contains(QLatin1Char('/')) ? QString() : text;
}

QVariantList cellList(const std::vector<int>& cells)
{
    QVariantList out;
    for (int c : cells)
        out.append(c);
    return out;
}

QVariantList lineList(const std::vector<std::vector<int>>& lines)
{
    QVariantList out;
    for (const auto& line : lines)
        out.append(QVariant(cellList(line)));
    return out;
}

// Snapshot layout: entries, corner marks (2 bytes each), centre marks
// (2 bytes each), colours, then the completion count.
constexpr int kSnapshotSize = 81 + 162 + 162 + 81 + 1;

} // namespace

SudokuGame::SudokuGame(QObject* parent)
    : GameSession(parent)
{
    m_board.model = classicModel();
    updateClues();
#if !CALM_SUDOKU_THREADED_GENERATION
    m_generationTimer = new QTimer(this);
    m_generationTimer->setSingleShot(true);
    connect(m_generationTimer, &QTimer::timeout, this, &SudokuGame::generateNext);
#endif
    // Warm the buffers so the first game of any level starts instantly.
    for (int d = 0; d < sudoku::DifficultyCount; ++d)
        requestPuzzle(d);
}

SudokuGame::~SudokuGame()
{
#if CALM_SUDOKU_THREADED_GENERATION
    for (auto* watcher : std::as_const(m_watchers)) {
        watcher->disconnect(this);
        watcher->waitForFinished();
    }
#endif
}

int SudokuGame::keyFor(const QString& level)
{
    if (const auto difficulty = sudoku::parseDifficulty(level.toStdString()))
        return int(*difficulty);
    if (const auto variant = sudoku::parseVariant(level.toStdString()); variant && *variant != Variant::Classic)
        return VariantKey + int(*variant);
    return int(Difficulty::Medium);
}

QString SudokuGame::levelFor(int key)
{
    if (key >= VariantKey)
        return fromView(sudoku::variantName(static_cast<Variant>(key - VariantKey)));
    return levelName(static_cast<Difficulty>(key));
}

SudokuGame::Generated SudokuGame::produce(int key, quint64 seed)
{
    Generated out;
    if (key >= VariantKey)
        out.variant = sudoku::generateVariant(static_cast<Variant>(key - VariantKey), seed);
    else
        out.classic = sudoku::generate(static_cast<Difficulty>(key), seed);
    return out;
}

quint64 SudokuGame::seedOf(const Generated& puzzle)
{
    return puzzle.classic ? puzzle.classic->seed : puzzle.variant ? puzzle.variant->seed : 0;
}

QStringList SudokuGame::variantIds()
{
    QStringList out;
    for (int v = 1; v < sudoku::VariantCount; ++v)
        out.append(fromView(sudoku::variantId(static_cast<Variant>(v))));
    return out;
}

QString SudokuGame::variantLevel(const QString& id)
{
    const auto variant = sudoku::parseVariant(id.toStdString());
    return variant && *variant != Variant::Classic ? fromView(sudoku::variantName(*variant)) : QString();
}

QString SudokuGame::variantOfLevel(const QString& level)
{
    if (sudoku::parseDifficulty(level.toStdString()))
        return QStringLiteral("classic");
    const auto variant = sudoku::parseVariant(level.toStdString());
    return variant ? fromView(sudoku::variantId(*variant)) : QString();
}

QString SudokuGame::variant() const
{
    return fromView(sudoku::variantId(currentVariant()));
}

bool SudokuGame::sameHouse(int a, int b) const
{
    if (a < 0 || b < 0 || a > 80 || b > 80 || a == b)
        return false;
    const auto& region = model().rules().region;
    return sudoku::rowOf(a) == sudoku::rowOf(b) || sudoku::colOf(a) == sudoku::colOf(b) || region[a] == region[b];
}

QVariantList SudokuGame::blockedFor(int digit) const
{
    QVariantList out;
    out.reserve(81);
    if (digit < 1 || digit > 9) {
        for (int i = 0; i < 81; ++i)
            out.append(false);
        return out;
    }
    for (int i = 0; i < 81; ++i) {
        const int here = digitAt(i);
        bool blocked = here != 0 && here != digit;
        if (!blocked && here == 0) {
            blocked = !(model().allowed(i) & sudoku::bit(digit));
            for (int p : model().peers(i))
                blocked = blocked || digitAt(p) == digit;
        }
        out.append(blocked);
    }
    return out;
}

void SudokuGame::updateClues()
{
    const sudoku::Rules& rules = model().rules();
    QVariantMap out;
    QVariantList regions, parity;
    for (int i = 0; i < 81; ++i) {
        regions.append(int(rules.region[i]));
        parity.append(int(rules.parity[i]));
    }
    out.insert(QStringLiteral("regions"), regions);
    out.insert(QStringLiteral("parity"), parity);
    out.insert(QStringLiteral("jigsaw"), rules.region != sudoku::standardRegions());
    out.insert(QStringLiteral("diagonals"), rules.diagonals);
    out.insert(QStringLiteral("windoku"), rules.windoku);
    out.insert(QStringLiteral("antiKnight"), rules.antiKnight);
    out.insert(QStringLiteral("antiKing"), rules.antiKing);
    out.insert(QStringLiteral("nonConsecutive"), rules.nonConsecutive);

    QVariantList cages;
    for (const auto& cage : rules.cages)
        cages.append(QVariantMap { { QStringLiteral("cells"), cellList(cage.cells) }, { QStringLiteral("sum"), cage.sum } });
    out.insert(QStringLiteral("cages"), cages);
    out.insert(QStringLiteral("thermos"), lineList(rules.thermos));
    QVariantList arrows;
    for (const auto& arrow : rules.arrows)
        arrows.append(QVariantMap { { QStringLiteral("circle"), arrow.circle }, { QStringLiteral("path"), cellList(arrow.path) } });
    out.insert(QStringLiteral("arrows"), arrows);
    QVariantList rowSandwich, colSandwich;
    bool sandwich = false;
    for (int i = 0; i < 9; ++i) {
        rowSandwich.append(int(rules.rowSandwich[static_cast<std::size_t>(i)]));
        colSandwich.append(int(rules.colSandwich[static_cast<std::size_t>(i)]));
        sandwich = sandwich || rules.rowSandwich[static_cast<std::size_t>(i)] >= 0 || rules.colSandwich[static_cast<std::size_t>(i)] >= 0;
    }
    out.insert(QStringLiteral("rowSandwich"), rowSandwich);
    out.insert(QStringLiteral("colSandwich"), colSandwich);
    QVariantList dots;
    for (const auto& dot : rules.dots) {
        static const char* kinds[] = { "white", "black", "x", "v" };
        dots.append(QVariantMap { { QStringLiteral("a"), dot.a }, { QStringLiteral("b"), dot.b },
            { QStringLiteral("kind"), QString::fromLatin1(kinds[int(dot.kind)]) } });
    }
    out.insert(QStringLiteral("dots"), dots);
    QVariantList littleKillers;
    for (const auto& clue : rules.littleKillers)
        littleKillers.append(QVariantMap { { QStringLiteral("row"), clue.row }, { QStringLiteral("col"), clue.col },
            { QStringLiteral("dr"), clue.dr }, { QStringLiteral("dc"), clue.dc }, { QStringLiteral("sum"), clue.sum } });
    out.insert(QStringLiteral("littleKillers"), littleKillers);
    out.insert(QStringLiteral("whispers"), lineList(rules.whispers));
    out.insert(QStringLiteral("renbans"), lineList(rules.renbans));
    out.insert(QStringLiteral("palindromes"), lineList(rules.palindromes));
    out.insert(QStringLiteral("outside"), sandwich || !rules.littleKillers.empty());
    if (out == m_clues)
        return;
    m_clues = out;
    emit cluesChanged();
}

sudoku::Grid SudokuGame::digits() const
{
    sudoku::Grid out {};
    for (int i = 0; i < 81; ++i)
        out[i] = static_cast<std::uint8_t>(digitAt(i));
    return out;
}

std::array<bool, 81> SudokuGame::mistakes(int mode) const
{
    std::array<bool, 81> out {};
    switch (mode) {
    case MistakesClashes:
        // A digit that repeats where it can't, or breaks a variant's clue.
        // Givens never clash with each other, so a flagged given always has a
        // player digit beside it; the UI tints givens only subtly.
        return sudoku::conflicts(model().rules(), digits());
    case MistakesAnswer:
        for (int i = 0; i < 81; ++i)
            out[i] = !isGiven(i) && m_board.entries[i] != 0 && m_board.entries[i] != m_board.solution[i];
        return out;
    default:
        return out;
    }
}

QVariantList SudokuGame::cells() const
{
    const auto flagged = mistakes(mistakeMode());
    QVariantList out;
    out.reserve(81);
    for (int i = 0; i < 81; ++i) {
        const int digit = digitAt(i);
        QVariantMap cell;
        cell.insert(QStringLiteral("digit"), digit);
        cell.insert(QStringLiteral("given"), isGiven(i));
        cell.insert(QStringLiteral("corner"), digit ? 0 : int(m_board.corner[i]));
        cell.insert(QStringLiteral("centre"), digit ? 0 : int(m_board.centre[i]));
        cell.insert(QStringLiteral("colour"), int(m_board.colours[i]));
        cell.insert(QStringLiteral("mistake"), flagged[i]);
        out.append(cell);
    }
    return out;
}

QVariantList SudokuGame::remaining() const
{
    int counts[10] {};
    for (int i = 0; i < 81; ++i)
        ++counts[digitAt(i)];
    QVariantList out;
    for (int d = 1; d <= 9; ++d)
        out.append(std::max(0, 9 - counts[d]));
    return out;
}

void SudokuGame::setSelected(int cell)
{
    if (cell < -1 || cell > 80)
        return;
    const QList<int> next = cell < 0 ? QList<int> {} : QList<int> { cell };
    if (next == m_selection)
        return;
    m_selection = next;
    if (!m_selection.isEmpty())
        setFocusDigit(0);
    emit selectedChanged();
}

QVariantList SudokuGame::selection() const
{
    QVariantList out;
    out.reserve(m_selection.size());
    for (int cell : m_selection)
        out.append(cell);
    return out;
}

void SudokuGame::addToSelection(int cell)
{
    if (cell < 0 || cell > 80 || (!m_selection.isEmpty() && m_selection.constLast() == cell))
        return;
    m_selection.removeAll(cell);
    m_selection.append(cell);
    setFocusDigit(0);
    emit selectedChanged();
}

void SudokuGame::removeFromSelection(int cell)
{
    if (m_selection.removeAll(cell) > 0)
        emit selectedChanged();
}

void SudokuGame::setMultiSelect(bool on)
{
    if (m_multiSelect == on)
        return;
    m_multiSelect = on;
    emit multiSelectChanged();
}

void SudokuGame::setInputMode(int mode)
{
    if (mode < NumberMode || mode > ColourMode || mode == m_inputMode)
        return;
    m_inputMode = mode;
    emit inputModeChanged();
}

void SudokuGame::setFocusDigit(int digit)
{
    if (digit < 0 || digit > 9 || digit == m_focusDigit)
        return;
    m_focusDigit = digit;
    emit focusDigitChanged();
}

QString SudokuGame::technique() const
{
    if (currentVariant() != Variant::Classic)
        return {};
    return QString::fromUtf8(sudoku::techniqueName(m_board.hardest).data());
}

QString SudokuGame::puzzleCode() const
{
    if (!active() || !m_board.seedKnown)
        return {};
    if (currentVariant() != Variant::Classic)
        return QString::fromStdString(sudoku::variantPuzzleCode(currentVariant(), m_board.seed));
    const auto difficulty = sudoku::parseDifficulty(level().toStdString());
    if (!difficulty)
        return {};
    return QString::fromStdString(sudoku::puzzleCode(*difficulty, m_board.seed));
}

QString SudokuGame::shareUrl(bool withProgress) const
{
    if (!active())
        return {};
    sudoku::SharedBoard shared;
    shared.givens = m_board.puzzle;
    shared.level = sudoku::parseDifficulty(level().toStdString());
    if (withProgress) {
        for (int i = 0; i < 81; ++i) {
            shared.colour[i] = m_board.colours[i];
            if (isGiven(i))
                continue;
            shared.entries[i] = m_board.entries[i];
            // Marks under a digit aren't shown, so they aren't shared.
            if (m_board.entries[i] == 0) {
                shared.corner[i] = m_board.corner[i];
                shared.centre[i] = m_board.centre[i];
            }
        }
    }
    if (currentVariant() != Variant::Classic) {
        if (!m_board.seedKnown)
            return {};
        return ShareUrlBase + QString::fromStdString(sudoku::encodeVariantBoard(currentVariant(), m_board.seed, shared));
    }
    return ShareUrlBase + QString::fromStdString(sudoku::encodeBoard(shared));
}

bool SudokuGame::openShared(const QString& link)
{
    const QString payload = sharePayload(link);
    if (payload.isEmpty())
        return false;

    // A variant link: rebuild its puzzle from the code, then lay the
    // progress on it.
    if (const auto variant = sudoku::decodeVariantBoard(payload.toStdString())) {
        m_pendingProgress = variant->board;
        startCode(VariantKey + int(variant->variant), variant->seed);
        return true;
    }

    const auto shared = sudoku::decodeBoard(payload.toStdString());
    if (!shared || sudoku::countSolutions(shared->givens, 2) != 1)
        return false;
    const auto solution = sudoku::solve(shared->givens);
    if (!solution)
        return false;

    const sudoku::GradeResult graded = sudoku::grade(shared->givens);
    const Difficulty difficulty = shared->level ? *shared->level : graded.difficulty().value_or(Difficulty::Expert);

    Board board;
    board.puzzle = shared->givens;
    board.solution = *solution;
    board.hardest = graded.hardest;
    board.model = classicModel();
    // A board that is already solved would be a game with nothing left to
    // do, so it opens at the start instead.
    bool solved = true;
    for (int i = 0; i < 81 && solved; ++i)
        solved = (board.puzzle[i] ? board.puzzle[i] : shared->entries[i]) == board.solution[i];
    if (!solved) {
        board.entries = shared->entries;
        board.corner = shared->corner;
        board.centre = shared->centre;
        board.colours = shared->colour;
    }

    if (loading()) {
        stopWaiting();
        cancelNewGame();
    }
    m_pendingProgress.reset();
    m_pendingBoard = board;
    startNewGame(levelName(difficulty));
    m_pendingBoard.reset();
    return true;
}

bool SudokuGame::startFromCode(const QString& code)
{
    if (const auto parsed = sudoku::parsePuzzleCode(code.toStdString())) {
        m_pendingProgress.reset();
        startCode(int(parsed->first), parsed->second);
        return true;
    }
    if (const auto parsed = sudoku::parseVariantPuzzleCode(code.toStdString())) {
        m_pendingProgress.reset();
        startCode(VariantKey + int(parsed->first), parsed->second);
        return true;
    }
    return false;
}

void SudokuGame::startCode(int key, quint64 seed)
{
    if (loading()) {
        stopWaiting();
        cancelNewGame();
    }
    m_pendingSeed = seed;
    startNewGame(levelFor(key));
    m_pendingSeed.reset();
}

void SudokuGame::stopWaiting()
{
    m_waitingFor.reset();
    m_waitingSeed.reset();
}

int SudokuGame::mistakeMode() const
{
    if (m_settings.contains(kMistakeMode))
        return std::clamp(m_settings.value(kMistakeMode).toInt(), int(MistakesOff), int(MistakesAnswer));
    if (m_settings.contains(kLegacyShowMistakes))
        return m_settings.value(kLegacyShowMistakes).toBool() ? MistakesAnswer : MistakesOff;
    return MistakesClashes;
}

void SudokuGame::setMistakeMode(int mode)
{
    if (mode < MistakesOff || mode > MistakesAnswer)
        return;
    if (m_settings.contains(kMistakeMode) && mistakeMode() == mode)
        return;
    const bool changed = mistakeMode() != mode;
    m_settings.setValue(kMistakeMode, mode);
    if (changed) {
        emit optionsChanged();
        emit stateChanged(); // per-cell `mistake` depends on it
    }
}

qreal SudokuGame::padWidth() const
{
    return std::clamp(m_settings.value(kPadWidth, 1.0).toReal(), MinPadWidth, 1.0);
}

void SudokuGame::setPadWidth(qreal width)
{
    width = std::clamp(width, MinPadWidth, 1.0);
    if (qFuzzyCompare(width, padWidth()))
        return;
    m_settings.setValue(kPadWidth, width);
    emit optionsChanged();
}

qreal SudokuGame::padOffset() const
{
    return std::clamp(m_settings.value(kPadOffset, 0.0).toReal(), -1.0, 1.0);
}

void SudokuGame::setPadOffset(qreal offset)
{
    offset = std::clamp(offset, -1.0, 1.0);
    if (qFuzzyCompare(offset + 2.0, padOffset() + 2.0))
        return;
    m_settings.setValue(kPadOffset, offset);
    emit optionsChanged();
}

void SudokuGame::enter(int digit)
{
    if (!active() || digit < 1 || digit > 9)
        return;
    if (m_selection.isEmpty()) {
        if (m_inputMode != ColourMode)
            setFocusDigit(digit == m_focusDigit ? 0 : digit);
        return;
    }
    if (!canPlay())
        return;

    switch (m_inputMode) {
    case CornerMode:
        toggleMark(m_board.corner, digit);
        return;
    case CentreMode:
        toggleMark(m_board.centre, digit);
        return;
    case ColourMode:
        paint(digit);
        return;
    default:
        break;
    }

    // A single selected cell can be overwritten; a multi-cell selection only
    // fills its empty cells, so a sweep never clobbers earlier work.
    const bool single = m_selection.size() == 1;
    QList<int> targets;
    for (int cell : std::as_const(m_selection)) {
        if (isGiven(cell) || m_board.entries[cell] == digit)
            continue;
        if (single || m_board.entries[cell] == 0)
            targets.append(cell);
    }
    if (targets.isEmpty())
        return;

    beginMove();
    for (int cell : std::as_const(targets))
        place(cell, digit);
    afterPlacement(targets);
    endMove();
}

void SudokuGame::toggleMark(std::array<sudoku::Mask, 81>& marks, int digit)
{
    QList<int> targets;
    bool allHave = true;
    for (int cell : std::as_const(m_selection)) {
        if (isGiven(cell) || m_board.entries[cell] != 0)
            continue;
        targets.append(cell);
        allHave = allHave && (marks[cell] & sudoku::bit(digit));
    }
    if (targets.isEmpty())
        return;

    beginMove();
    for (int cell : std::as_const(targets)) {
        if (allHave)
            marks[cell] &= static_cast<sudoku::Mask>(~sudoku::bit(digit));
        else
            marks[cell] |= sudoku::bit(digit);
    }
    emitFeedback(QStringLiteral("soft"));
    endMove();
}

void SudokuGame::paint(int colour)
{
    if (!canPlay() || colour < 0 || colour > 9 || m_selection.isEmpty())
        return;

    bool allHave = true;
    bool anyColour = false;
    for (int cell : std::as_const(m_selection)) {
        allHave = allHave && m_board.colours[cell] == colour;
        anyColour = anyColour || m_board.colours[cell] != 0;
    }
    if (colour == 0 && !anyColour)
        return;

    const int value = allHave ? 0 : colour;
    beginMove();
    for (int cell : std::as_const(m_selection))
        m_board.colours[cell] = static_cast<std::uint8_t>(value);
    emitFeedback(value == 0 ? QStringLiteral("erase") : QStringLiteral("soft"));
    endMove();
}

void SudokuGame::erase()
{
    if (!canPlay() || m_selection.isEmpty())
        return;

    bool anyEntry = false;
    bool anyMark = false;
    bool anyColour = false;
    for (int cell : std::as_const(m_selection)) {
        anyColour = anyColour || m_board.colours[cell] != 0;
        if (isGiven(cell))
            continue;
        anyEntry = anyEntry || m_board.entries[cell] != 0;
        anyMark = anyMark || m_board.corner[cell] != 0 || m_board.centre[cell] != 0;
    }
    if (!anyEntry && !anyMark && !anyColour)
        return;

    beginMove();
    for (int cell : std::as_const(m_selection)) {
        if (anyEntry) {
            if (!isGiven(cell))
                m_board.entries[cell] = 0;
        } else if (anyMark) {
            if (!isGiven(cell)) {
                m_board.corner[cell] = 0;
                m_board.centre[cell] = 0;
            }
        } else {
            m_board.colours[cell] = 0;
        }
    }
    emitFeedback(QStringLiteral("erase"));
    endMove();
}

bool SudokuGame::hint()
{
    if (!canPlay())
        return false;

    const int current = selected();
    int target = -1;
    if (current >= 0 && !isGiven(current) && m_board.entries[current] != m_board.solution[current]) {
        target = current;
    } else {
        // Ask the logical solver for the next cell a person could fill,
        // ignoring any wrong entries so the hint is always sound.
        sudoku::Grid position = m_board.puzzle;
        for (int i = 0; i < 81; ++i)
            if (m_board.entries[i] == m_board.solution[i])
                position[i] = m_board.entries[i];
        if (currentVariant() == Variant::Classic) {
            if (auto h = sudoku::hint(position))
                target = h->cell;
        } else if (auto h = model().hint(position)) {
            target = h->cell;
        }
        for (int i = 0; target < 0 && i < 81; ++i)
            if (digitAt(i) != m_board.solution[i])
                target = i;
    }
    if (target < 0)
        return false;

    setSelected(target);
    beginMove();
    place(target, m_board.solution[target]);
    noteHintUsed();
    afterPlacement({ target });
    endMove();
    return true;
}

int SudokuGame::digitAt(int cell) const
{
    return isGiven(cell) ? m_board.puzzle[cell] : m_board.entries[cell];
}

void SudokuGame::place(int cell, int digit)
{
    m_board.entries[cell] = static_cast<std::uint8_t>(digit);
    m_board.corner[cell] = 0;
    m_board.centre[cell] = 0;
    if (autoRemoveNotes()) {
        const auto keep = static_cast<sudoku::Mask>(~sudoku::bit(digit));
        for (int peer : model().peers(cell)) {
            m_board.corner[peer] &= keep;
            m_board.centre[peer] &= keep;
        }
    }
}

bool SudokuGame::unitCorrect(const std::array<int, 9>& unit) const
{
    for (int cell : unit)
        if (digitAt(cell) != m_board.solution[cell])
            return false;
    return true;
}

// One feedback event for the whole placement, however many cells it touched.
void SudokuGame::afterPlacement(const QList<int>& cells)
{
    const auto flaggedCells = mistakes(mistakeMode());
    bool wrong = false;
    bool flagged = false;
    for (int cell : cells) {
        wrong = wrong || digitAt(cell) != m_board.solution[cell];
        flagged = flagged || flaggedCells[cell];
    }
    if (wrong || flagged) {
        emitFeedback(flagged ? QStringLiteral("error") : QStringLiteral("tap"));
        return;
    }

    // Rows, columns and regions (and any diagonals or extra boxes) the
    // placement just completed.
    int completed = 0;
    for (const auto& unit : model().units()) {
        const bool touched = std::any_of(unit.begin(), unit.end(), [&cells](int c) { return cells.contains(c); });
        if (touched)
            completed += unitCorrect(unit);
    }

    bool solved = true;
    for (int i = 0; i < 81 && solved; ++i)
        solved = digitAt(i) == m_board.solution[i];

    if (solved) {
        finishGame();
    } else if (completed > 0) {
        emitFeedback(QStringLiteral("complete"), m_board.completions % 8);
        m_board.completions += completed;
    } else {
        emitFeedback(QStringLiteral("tap"));
    }
}

void SudokuGame::createGame(const QString& level)
{
    if (m_pendingBoard) {
        startWith(*std::exchange(m_pendingBoard, std::nullopt));
        return;
    }
    const int key = keyFor(level);
    auto& queue = m_buffer[key];
    stopWaiting();

    // A typed code: that exact puzzle, from the buffer if it happens to be
    // there, else generated now ahead of anything else.
    if (const auto seed = std::exchange(m_pendingSeed, std::nullopt)) {
        for (qsizetype i = 0; i < queue.size(); ++i) {
            if (seedOf(queue.at(i)) == *seed) {
                startWith(queue.takeAt(i));
                requestPuzzle(key);
                return;
            }
        }
        m_waitingFor = key;
        m_waitingSeed = *seed;
        generate(key, *seed);
        return;
    }

    if (!queue.isEmpty()) {
        startWith(queue.takeFirst());
        requestPuzzle(key);
        return;
    }
    m_waitingFor = key;
    if (m_inFlight.value(key) == 0)
        requestPuzzle(key);
#if !CALM_SUDOKU_THREADED_GENERATION
    scheduleGeneration();
#endif
}

void SudokuGame::requestPuzzle(int key)
{
    // Classic levels are warmed from the start; a variant gets its spare
    // once it has been played.
    const int size = key >= VariantKey ? VariantBufferSize : BufferSize;
    if (m_buffer.value(key).size() + m_inFlight.value(key) >= size)
        return;
    // Seeds below 2^40 keep puzzle codes to 8 characters after the level.
    generate(key, QRandomGenerator::global()->generate64() & sudoku::PuzzleSeedMask);
}

void SudokuGame::generate(int key, quint64 seed)
{
    ++m_inFlight[key];
#if CALM_SUDOKU_THREADED_GENERATION
    auto* watcher = new QFutureWatcher<Generated>(this);
    m_watchers.append(watcher);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, key, seed] {
        m_watchers.removeOne(watcher);
        const Generated puzzle = watcher->result();
        watcher->deleteLater();
        onPuzzleReady(key, seed, puzzle);
    });
    watcher->setFuture(QtConcurrent::run([key, seed] { return produce(key, seed); }));
#else
    m_pending.append({ key, seed });
    scheduleGeneration();
#endif
}

#if !CALM_SUDOKU_THREADED_GENERATION
void SudokuGame::scheduleGeneration()
{
    if (m_pending.isEmpty())
        return;
    // Each puzzle takes a few milliseconds up to about half a second, and
    // blocks the only thread while it runs. Leave time for frames between
    // them, and let the first screen appear before warming the buffers. A
    // level the player is waiting for goes next without delay.
    const int delay = m_waitingFor ? 0 : m_warmedUp ? 50 : 600;
    if (!m_generationTimer->isActive() || m_generationTimer->remainingTime() > delay)
        m_generationTimer->start(delay);
}

void SudokuGame::generateNext()
{
    m_warmedUp = true;
    if (m_pending.isEmpty())
        return;
    qsizetype next = 0;
    if (m_waitingFor) {
        for (qsizetype i = 0; i < m_pending.size(); ++i) {
            const auto& [key, seed] = m_pending.at(i);
            if (key == *m_waitingFor && (!m_waitingSeed || *m_waitingSeed == seed)) {
                next = i;
                break;
            }
        }
    }
    const auto [key, seed] = m_pending.takeAt(next);
    onPuzzleReady(key, seed, produce(key, seed));
    scheduleGeneration();
}
#endif

void SudokuGame::onPuzzleReady(int key, quint64 seed, const Generated& puzzle)
{
    --m_inFlight[key];
    const bool wanted = m_waitingFor == key && (!m_waitingSeed || *m_waitingSeed == seed);
    if (puzzle.ok()) {
        if (wanted) {
            stopWaiting();
            startWith(puzzle);
        } else {
            m_buffer[key].append(puzzle);
        }
    } else if (wanted && m_waitingSeed) {
        // A code whose seed never reaches its level: nothing to start.
        stopWaiting();
        m_pendingProgress.reset();
        cancelNewGame();
        emit codeFailed();
    }
    requestPuzzle(key);
}

void SudokuGame::resetInput()
{
    setInputMode(NumberMode);
    setMultiSelect(false);
    setFocusDigit(0);
}

void SudokuGame::startWith(const Generated& puzzle)
{
    Board board;
    if (puzzle.classic) {
        board.puzzle = puzzle.classic->puzzle;
        board.solution = puzzle.classic->solution;
        board.hardest = puzzle.classic->hardest;
        board.seed = puzzle.classic->seed;
        board.model = classicModel();
    } else {
        board.puzzle = puzzle.variant->puzzle;
        board.solution = puzzle.variant->solution;
        board.seed = puzzle.variant->seed;
        board.model = std::make_shared<const sudoku::VariantModel>(puzzle.variant->rules);
    }
    board.seedKnown = true;

    // Progress from a variant share link goes onto its rebuilt board, unless
    // it would leave nothing to do.
    if (const auto progress = std::exchange(m_pendingProgress, std::nullopt); progress && puzzle.variant) {
        bool solved = true;
        for (int i = 0; i < 81 && solved; ++i)
            solved = (board.puzzle[i] ? board.puzzle[i] : progress->entries[i]) == board.solution[i];
        if (!solved) {
            for (int i = 0; i < 81; ++i) {
                board.colours[i] = progress->colour[i];
                if (board.puzzle[i])
                    continue;
                board.entries[i] = progress->entries[i];
                board.corner[i] = progress->corner[i];
                board.centre[i] = progress->centre[i];
            }
        }
    }
    startWith(board);
}

void SudokuGame::startWith(const Board& board)
{
    m_board = board;
    if (!m_board.model)
        m_board.model = classicModel();
    updateClues();
    setSelected(-1);
    resetInput();
    beginGame();
}

QVariantMap SudokuGame::saveState() const
{
    QVariantMap state;
    state.insert(QStringLiteral("puzzle"), gridToString(m_board.puzzle));
    state.insert(QStringLiteral("solution"), gridToString(m_board.solution));
    state.insert(QStringLiteral("entries"), gridToString(m_board.entries));
    state.insert(QStringLiteral("corner"), masksToList(m_board.corner));
    state.insert(QStringLiteral("centre"), masksToList(m_board.centre));
    state.insert(QStringLiteral("colours"), gridToString(m_board.colours));
    state.insert(QStringLiteral("hardest"), int(m_board.hardest));
    if (m_board.seedKnown)
        state.insert(QStringLiteral("seed"), QString::number(m_board.seed));
    state.insert(QStringLiteral("completions"), m_board.completions);
    state.insert(QStringLiteral("selected"), selected());
    state.insert(QStringLiteral("selection"), selection());
    // Variant games keep their rules too; classic saves stay as they were.
    if (currentVariant() != Variant::Classic)
        state.insert(QStringLiteral("rules"), QString::fromStdString(sudoku::encodeRules(model().rules())));
    return state;
}

bool SudokuGame::restoreState(const QVariantMap& state)
{
    const auto puzzle = gridFromString(state.value(QStringLiteral("puzzle")).toString());
    const auto solution = gridFromString(state.value(QStringLiteral("solution")).toString());
    const auto entries = gridFromString(state.value(QStringLiteral("entries")).toString());
    if (!puzzle || !solution || !entries)
        return false;

    std::shared_ptr<const sudoku::VariantModel> rules = classicModel();
    if (state.contains(QStringLiteral("rules"))) {
        const auto decoded = sudoku::decodeRules(state.value(QStringLiteral("rules")).toString().toStdString());
        if (!decoded)
            return false;
        rules = std::make_shared<const sudoku::VariantModel>(*decoded);
    }
    if (!sudoku::satisfies(rules->rules(), *solution))
        return false;
    for (int i = 0; i < 81; ++i)
        if ((*puzzle)[i] && (*puzzle)[i] != (*solution)[i])
            return false;

    Board board;
    board.puzzle = *puzzle;
    board.solution = *solution;
    board.entries = *entries;
    board.model = rules;
    // Saves from before corner marks existed have one kind of note, `notes`,
    // which were drawn as small digits in the cell: they become centre marks.
    const QString centreKey = state.contains(QStringLiteral("centre")) ? QStringLiteral("centre") : QStringLiteral("notes");
    if (!masksFromList(state.value(centreKey).toList(), board.centre))
        return false;
    if (state.contains(QStringLiteral("corner")) && !masksFromList(state.value(QStringLiteral("corner")).toList(), board.corner))
        return false;
    if (state.contains(QStringLiteral("colours"))) {
        const auto colours = gridFromString(state.value(QStringLiteral("colours")).toString());
        if (!colours)
            return false;
        board.colours = *colours;
    }
    board.hardest = static_cast<sudoku::Technique>(
        std::clamp(state.value(QStringLiteral("hardest")).toInt(), 0, sudoku::TechniqueCount - 1));
    // Boards opened from a share link have no seed (and so no puzzle code).
    board.seedKnown = state.contains(QStringLiteral("seed"));
    board.seed = state.value(QStringLiteral("seed")).toString().toULongLong();
    board.completions = state.value(QStringLiteral("completions")).toInt();
    m_board = board;
    updateClues();

    QList<int> restored;
    if (state.contains(QStringLiteral("selection"))) {
        for (const QVariant& v : state.value(QStringLiteral("selection")).toList()) {
            const int cell = v.toInt();
            if (cell >= 0 && cell <= 80 && !restored.contains(cell))
                restored.append(cell);
        }
    } else {
        const int cell = state.value(QStringLiteral("selected"), -1).toInt();
        if (cell >= 0 && cell <= 80)
            restored.append(cell);
    }
    m_selection = restored;
    emit selectedChanged();
    resetInput();
    return true;
}

QVariant SudokuGame::snapshot() const
{
    QByteArray bytes;
    bytes.reserve(kSnapshotSize);
    for (auto v : m_board.entries)
        bytes.append(char(v));
    for (const auto* marks : { &m_board.corner, &m_board.centre }) {
        for (auto n : *marks) {
            bytes.append(char(n & 0xFF));
            bytes.append(char(n >> 8));
        }
    }
    for (auto c : m_board.colours)
        bytes.append(char(c));
    bytes.append(char(m_board.completions));
    return bytes;
}

void SudokuGame::applySnapshot(const QVariant& snapshot)
{
    const QByteArray bytes = snapshot.toByteArray();
    if (bytes.size() != kSnapshotSize)
        return;
    const auto byteAt = [&bytes](int i) { return static_cast<std::uint8_t>(bytes.at(i)); };
    for (int i = 0; i < 81; ++i) {
        m_board.entries[i] = byteAt(i);
        m_board.corner[i] = static_cast<sudoku::Mask>(byteAt(81 + 2 * i) | (byteAt(82 + 2 * i) << 8));
        m_board.centre[i] = static_cast<sudoku::Mask>(byteAt(243 + 2 * i) | (byteAt(244 + 2 * i) << 8));
        m_board.colours[i] = byteAt(405 + i);
    }
    m_board.completions = byteAt(486);
}

bool SudokuGame::option(const QString& key, bool fallback) const
{
    return m_settings.value(QStringLiteral("sudoku/") + key, fallback).toBool();
}

void SudokuGame::setOption(const QString& key, bool on)
{
    if (option(key, !on) == on && m_settings.contains(QStringLiteral("sudoku/") + key))
        return;
    m_settings.setValue(QStringLiteral("sudoku/") + key, on);
    emit optionsChanged();
}
