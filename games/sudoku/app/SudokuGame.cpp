#include "SudokuGame.h"

#include <sudoku/Board.h>
#include <sudoku/Grader.h>

#include <QRandomGenerator>
#include <QtConcurrent/QtConcurrentRun>

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
    QVariantList out;
    out.reserve(81);
    for (int i = 0; i < 81; ++i) {
        const int digit = digitAt(i);
        QVariantMap cell;
        cell.insert(QStringLiteral("digit"), digit);
        cell.insert(QStringLiteral("given"), isGiven(i));
        cell.insert(QStringLiteral("notes"), digit ? 0 : int(m_board.notes[i]));
        cell.insert(QStringLiteral("mistake"), !isGiven(i) && digit != 0 && digit != m_board.solution[i]);
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
    if (cell < -1 || cell > 80 || cell == m_selected)
        return;
    m_selected = cell;
    emit selectedChanged();
}

void SudokuGame::setNotesMode(bool on)
{
    if (m_notesMode == on)
        return;
    m_notesMode = on;
    emit notesModeChanged();
}

QString SudokuGame::technique() const
{
    return QString::fromUtf8(sudoku::techniqueName(m_board.hardest).data());
}

void SudokuGame::enter(int digit)
{
    if (!active() || complete() || digit < 1 || digit > 9 || m_selected < 0 || isGiven(m_selected))
        return;
    const int cell = m_selected;

    if (m_notesMode) {
        if (m_board.entries[cell] != 0)
            return;
        beginMove();
        m_board.notes[cell] ^= sudoku::bit(digit);
        emitFeedback(QStringLiteral("soft"));
        endMove();
        return;
    }

    if (m_board.entries[cell] == digit)
        return;
    beginMove();
    place(cell, digit);
    afterPlacement(cell);
    endMove();
}

void SudokuGame::erase()
{
    if (!active() || complete() || m_selected < 0 || isGiven(m_selected))
        return;
    const int cell = m_selected;
    if (m_board.entries[cell] == 0 && m_board.notes[cell] == 0)
        return;
    beginMove();
    m_board.entries[cell] = 0;
    m_board.notes[cell] = 0;
    emitFeedback(QStringLiteral("erase"));
    endMove();
}

bool SudokuGame::hint()
{
    if (!active() || complete())
        return false;

    int target = -1;
    if (m_selected >= 0 && !isGiven(m_selected) && m_board.entries[m_selected] != m_board.solution[m_selected]) {
        target = m_selected;
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
    afterPlacement(target);
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
    m_board.notes[cell] = 0;
    if (autoRemoveNotes()) {
        for (int peer : sudoku::peers()[cell])
            m_board.notes[peer] &= static_cast<sudoku::Mask>(~sudoku::bit(digit));
    }
}

bool SudokuGame::unitCorrect(int unit) const
{
    for (int cell : sudoku::units()[unit])
        if (digitAt(cell) != m_board.solution[cell])
            return false;
    return true;
}

void SudokuGame::afterPlacement(int cell)
{
    if (digitAt(cell) != m_board.solution[cell]) {
        emitFeedback(showMistakes() ? QStringLiteral("error") : QStringLiteral("tap"));
        return;
    }

    int completed = 0;
    for (int unit : { sudoku::rowOf(cell), 9 + sudoku::colOf(cell), 18 + sudoku::boxOf(cell) })
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

void SudokuGame::startWith(const sudoku::Puzzle& puzzle)
{
    m_board = {};
    m_board.puzzle = puzzle.puzzle;
    m_board.solution = puzzle.solution;
    m_board.hardest = puzzle.hardest;
    m_board.seed = puzzle.seed;
    m_selected = -1;
    emit selectedChanged();
    setNotesMode(false);
    beginGame();
}

QVariantMap SudokuGame::saveState() const
{
    QVariantList notes;
    for (auto n : m_board.notes)
        notes.append(int(n));
    QVariantMap state;
    state.insert(QStringLiteral("puzzle"), gridToString(m_board.puzzle));
    state.insert(QStringLiteral("solution"), gridToString(m_board.solution));
    state.insert(QStringLiteral("entries"), gridToString(m_board.entries));
    state.insert(QStringLiteral("notes"), notes);
    state.insert(QStringLiteral("hardest"), int(m_board.hardest));
    state.insert(QStringLiteral("seed"), QString::number(m_board.seed));
    state.insert(QStringLiteral("completions"), m_board.completions);
    state.insert(QStringLiteral("selected"), m_selected);
    return state;
}

bool SudokuGame::restoreState(const QVariantMap& state)
{
    const auto puzzle = gridFromString(state.value(QStringLiteral("puzzle")).toString());
    const auto solution = gridFromString(state.value(QStringLiteral("solution")).toString());
    const auto entries = gridFromString(state.value(QStringLiteral("entries")).toString());
    const QVariantList notes = state.value(QStringLiteral("notes")).toList();
    if (!puzzle || !solution || !entries || notes.size() != 81 || !sudoku::isSolved(*solution))
        return false;

    Board board;
    board.puzzle = *puzzle;
    board.solution = *solution;
    board.entries = *entries;
    for (int i = 0; i < 81; ++i)
        board.notes[i] = static_cast<sudoku::Mask>(notes.at(i).toInt() & sudoku::AllDigits);
    board.hardest = static_cast<sudoku::Technique>(
        std::clamp(state.value(QStringLiteral("hardest")).toInt(), 0, sudoku::TechniqueCount - 1));
    board.seed = state.value(QStringLiteral("seed")).toString().toULongLong();
    board.completions = state.value(QStringLiteral("completions")).toInt();
    m_board = board;

    m_selected = std::clamp(state.value(QStringLiteral("selected"), -1).toInt(), -1, 80);
    emit selectedChanged();
    setNotesMode(false);
    return true;
}

QVariant SudokuGame::snapshot() const
{
    QByteArray bytes;
    bytes.reserve(81 * 3 + 1);
    for (auto v : m_board.entries)
        bytes.append(char(v));
    for (auto n : m_board.notes) {
        bytes.append(char(n & 0xFF));
        bytes.append(char(n >> 8));
    }
    bytes.append(char(m_board.completions));
    return bytes;
}

void SudokuGame::applySnapshot(const QVariant& snapshot)
{
    const QByteArray bytes = snapshot.toByteArray();
    if (bytes.size() != 81 * 3 + 1)
        return;
    for (int i = 0; i < 81; ++i) {
        m_board.entries[i] = static_cast<std::uint8_t>(bytes.at(i));
        const auto lo = static_cast<std::uint8_t>(bytes.at(81 + 2 * i));
        const auto hi = static_cast<std::uint8_t>(bytes.at(82 + 2 * i));
        m_board.notes[i] = static_cast<sudoku::Mask>(lo | (hi << 8));
    }
    m_board.completions = static_cast<std::uint8_t>(bytes.at(243));
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
