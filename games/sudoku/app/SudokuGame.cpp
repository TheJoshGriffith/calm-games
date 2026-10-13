#include "SudokuGame.h"

#include <sudoku/Board.h>
#include <sudoku/Grader.h>

#include <QRandomGenerator>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

using sudoku::Difficulty;

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

// Snapshot layout: entries, corner marks (2 bytes each), centre marks
// (2 bytes each), colours, then the completion count.
constexpr int kSnapshotSize = 81 + 162 + 162 + 81 + 1;

} // namespace

SudokuGame::SudokuGame(QObject* parent)
    : GameSession(parent)
{
    // Warm the buffers so the first game of any level starts instantly.
    for (int d = 0; d < sudoku::DifficultyCount; ++d)
        requestPuzzle(static_cast<Difficulty>(d));
}

SudokuGame::~SudokuGame()
{
    for (auto* watcher : std::as_const(m_watchers)) {
        watcher->disconnect(this);
        watcher->waitForFinished();
    }
}

QVariantList SudokuGame::cells() const
{
    const int mode = mistakeMode();
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
        cell.insert(QStringLiteral("mistake"), isMistake(i, mode));
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

QString SudokuGame::technique() const
{
    return QString::fromUtf8(sudoku::techniqueName(m_board.hardest).data());
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
    if (!canPlay() || digit < 1 || digit > 9 || m_selection.isEmpty())
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
        if (auto h = sudoku::hint(position))
            target = h->cell;
        for (int i = 0; target < 0 && i < 81; ++i)
            if (digitAt(i) != m_board.solution[i])
                target = i;
    }
    if (target < 0)
        return false;

    setSelected(target);
    beginMove();
    place(target, m_board.solution[target]);
    afterPlacement({ target });
    endMove();
    return true;
}

int SudokuGame::digitAt(int cell) const
{
    return isGiven(cell) ? m_board.puzzle[cell] : m_board.entries[cell];
}

bool SudokuGame::clashes(int cell) const
{
    const int digit = digitAt(cell);
    if (digit == 0)
        return false;
    for (int peer : sudoku::peers()[cell])
        if (digitAt(peer) == digit)
            return true;
    return false;
}

bool SudokuGame::isMistake(int cell, int mode) const
{
    switch (mode) {
    case MistakesClashes:
        // Givens never clash with each other, so a flagged given always has a
        // player digit beside it; the UI tints givens only subtly.
        return clashes(cell);
    case MistakesAnswer:
        return !isGiven(cell) && m_board.entries[cell] != 0 && m_board.entries[cell] != m_board.solution[cell];
    default:
        return false;
    }
}

void SudokuGame::place(int cell, int digit)
{
    m_board.entries[cell] = static_cast<std::uint8_t>(digit);
    m_board.corner[cell] = 0;
    m_board.centre[cell] = 0;
    if (autoRemoveNotes()) {
        const auto keep = static_cast<sudoku::Mask>(~sudoku::bit(digit));
        for (int peer : sudoku::peers()[cell]) {
            m_board.corner[peer] &= keep;
            m_board.centre[peer] &= keep;
        }
    }
}

bool SudokuGame::unitCorrect(int unit) const
{
    for (int cell : sudoku::units()[unit])
        if (digitAt(cell) != m_board.solution[cell])
            return false;
    return true;
}

// One feedback event for the whole placement, however many cells it touched.
void SudokuGame::afterPlacement(const QList<int>& cells)
{
    const int mode = mistakeMode();
    bool wrong = false;
    bool flagged = false;
    for (int cell : cells) {
        wrong = wrong || digitAt(cell) != m_board.solution[cell];
        flagged = flagged || isMistake(cell, mode);
    }
    if (wrong || flagged) {
        emitFeedback(flagged ? QStringLiteral("error") : QStringLiteral("tap"));
        return;
    }

    QSet<int> units;
    for (int cell : cells)
        units << sudoku::rowOf(cell) << 9 + sudoku::colOf(cell) << 18 + sudoku::boxOf(cell);
    int completed = 0;
    for (int unit : std::as_const(units))
        completed += unitCorrect(unit);

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
    const Difficulty difficulty = sudoku::parseDifficulty(level.toStdString()).value_or(Difficulty::Medium);
    auto& queue = m_buffer[int(difficulty)];
    if (!queue.isEmpty()) {
        startWith(queue.takeFirst());
        requestPuzzle(difficulty);
        return;
    }
    m_waitingFor = difficulty;
    if (m_inFlight.value(int(difficulty)) == 0)
        requestPuzzle(difficulty);
}

void SudokuGame::requestPuzzle(Difficulty difficulty)
{
    const int key = int(difficulty);
    if (m_buffer.value(key).size() + m_inFlight.value(key) >= BufferSize)
        return;
    ++m_inFlight[key];

    const quint64 seed = QRandomGenerator::global()->generate64();
    auto* watcher = new QFutureWatcher<std::optional<sudoku::Puzzle>>(this);
    m_watchers.append(watcher);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, difficulty] {
        m_watchers.removeOne(watcher);
        const auto puzzle = watcher->result();
        watcher->deleteLater();
        onPuzzleReady(difficulty, puzzle);
    });
    watcher->setFuture(QtConcurrent::run([difficulty, seed] { return sudoku::generate(difficulty, seed); }));
}

void SudokuGame::onPuzzleReady(Difficulty difficulty, const std::optional<sudoku::Puzzle>& puzzle)
{
    const int key = int(difficulty);
    --m_inFlight[key];
    if (puzzle) {
        if (m_waitingFor == difficulty) {
            m_waitingFor.reset();
            startWith(*puzzle);
        } else {
            m_buffer[key].append(*puzzle);
        }
    }
    requestPuzzle(difficulty);
}

void SudokuGame::resetInput()
{
    setInputMode(NumberMode);
    setMultiSelect(false);
}

void SudokuGame::startWith(const sudoku::Puzzle& puzzle)
{
    m_board = {};
    m_board.puzzle = puzzle.puzzle;
    m_board.solution = puzzle.solution;
    m_board.hardest = puzzle.hardest;
    m_board.seed = puzzle.seed;
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
    state.insert(QStringLiteral("seed"), QString::number(m_board.seed));
    state.insert(QStringLiteral("completions"), m_board.completions);
    state.insert(QStringLiteral("selected"), selected());
    state.insert(QStringLiteral("selection"), selection());
    return state;
}

bool SudokuGame::restoreState(const QVariantMap& state)
{
    const auto puzzle = gridFromString(state.value(QStringLiteral("puzzle")).toString());
    const auto solution = gridFromString(state.value(QStringLiteral("solution")).toString());
    const auto entries = gridFromString(state.value(QStringLiteral("entries")).toString());
    if (!puzzle || !solution || !entries || !sudoku::isSolved(*solution))
        return false;

    Board board;
    board.puzzle = *puzzle;
    board.solution = *solution;
    board.entries = *entries;
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
    board.seed = state.value(QStringLiteral("seed")).toString().toULongLong();
    board.completions = state.value(QStringLiteral("completions")).toInt();
    m_board = board;

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
