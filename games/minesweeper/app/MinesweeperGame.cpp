#include "MinesweeperGame.h"

#include <minesweeper/Generator.h>
#include <minesweeper/Solver.h>

#include <calm/Rng.h>

#include <QRandomGenerator>

using minesweeper::Cell;
using minesweeper::Difficulty;

namespace {

QByteArray encodeMines(const minesweeper::Field& field)
{
    if (!field.minesPlaced())
        return {};
    const auto& mines = field.mines();
    return QByteArray(reinterpret_cast<const char*>(mines.data()), qsizetype(mines.size()));
}

QByteArray encodeStates(const minesweeper::Field& field)
{
    QByteArray bytes;
    bytes.reserve(field.cellCount());
    for (Cell state : field.states())
        bytes.append(char(state));
    return bytes;
}

} // namespace

MinesweeperGame::MinesweeperGame(QObject* parent)
    : GameSession(parent)
    , m_field(9, 9, 10)
{
}

QVariantList MinesweeperGame::cells() const
{
    QVariantList out;
    out.reserve(m_field.cellCount());
    for (int i = 0; i < m_field.cellCount(); ++i) {
        const Cell state = m_field.state(i);
        const bool mine = m_field.minesPlaced() && m_field.isMine(i);
        int view = Hidden;
        if (i == m_exploded)
            view = Exploded;
        else if (state == Cell::Flagged)
            view = (lost() && !mine) ? WrongFlag : Flagged;
        else if (lost() && mine)
            view = Mine;
        else if (state == Cell::Revealed)
            view = m_field.adjacentMines(i);
        out.append(view);
    }
    return out;
}

void MinesweeperGame::setFlagMode(bool on)
{
    if (m_flagMode == on)
        return;
    m_flagMode = on;
    emit flagModeChanged();
}

bool MinesweeperGame::playable(int cell) const
{
    return active() && !complete() && !loading() && !lost() && cell >= 0 && cell < m_field.cellCount();
}

void MinesweeperGame::tap(int cell)
{
    if (m_flagMode && playable(cell) && m_field.state(cell) != Cell::Revealed)
        flag(cell);
    else
        open(cell);
}

void MinesweeperGame::open(int cell)
{
    openCell(cell, false);
}

void MinesweeperGame::openCell(int cell, bool hinted)
{
    if (!playable(cell))
        return;
    const Cell state = m_field.state(cell);
    if (state == Cell::Flagged)
        return;
    if (state == Cell::Revealed
        && (m_field.adjacentMines(cell) == 0 || m_field.flaggedNeighbours(cell) != m_field.adjacentMines(cell)))
        return;

    beginMove();
    if (hinted)
        noteHintUsed();
    const QVariantMap before = snapshotOf();
    if (!m_field.minesPlaced()) {
        calm::Rng rng(m_seed);
        minesweeper::layMines(m_field, cell, rng, noGuessing());
    }
    m_origin = cell;
    const auto result = state == Cell::Revealed ? m_field.chord(cell) : m_field.reveal(cell);
    afterReveal(result, before);
}

void MinesweeperGame::afterReveal(const minesweeper::RevealResult& result, const QVariantMap& before)
{
    if (result.hitMine) {
        // Save the board as it was before this move, so a saved game resumes
        // just before the mine rather than after it.
        m_beforeLoss = before;
        m_exploded = result.mineCell;
        setPaused(true);
        emitFeedback(QStringLiteral("error"));
        endMove();
        return;
    }
    if (m_field.cleared()) {
        for (int i = 0; i < m_field.cellCount(); ++i)
            if (m_field.isMine(i))
                m_field.setState(i, Cell::Flagged);
        endMove();
        finishGame();
        return;
    }
    if (result.opened > 1)
        emitFeedback(QStringLiteral("complete"), m_cascades++);
    else
        emitFeedback(QStringLiteral("tap"));
    endMove();
}

void MinesweeperGame::flag(int cell)
{
    flagCell(cell, false);
}

void MinesweeperGame::flagCell(int cell, bool hinted)
{
    if (!playable(cell) || m_field.state(cell) == Cell::Revealed)
        return;
    beginMove();
    if (hinted)
        noteHintUsed();
    m_field.toggleFlag(cell);
    emitFeedback(m_field.state(cell) == Cell::Flagged ? QStringLiteral("soft") : QStringLiteral("erase"));
    endMove();
}

bool MinesweeperGame::hint()
{
    if (!active() || complete() || loading() || lost())
        return false;
    if (!m_field.minesPlaced()) {
        openCell((m_field.rows() / 2) * m_field.cols() + m_field.cols() / 2, true);
        return true;
    }
    // A wrong flag would mislead every deduction, so clear it first.
    for (int i = 0; i < m_field.cellCount(); ++i) {
        if (m_field.state(i) == Cell::Flagged && !m_field.isMine(i)) {
            flagCell(i, true);
            return true;
        }
    }
    const minesweeper::Deductions found = minesweeper::deduce(m_field);
    if (!found.safe.empty()) {
        openCell(found.safe.front(), true);
        return true;
    }
    if (!found.mines.empty()) {
        flagCell(found.mines.front(), true);
        return true;
    }
    emitFeedback(QStringLiteral("error"));
    return false;
}

void MinesweeperGame::createGame(const QString& level)
{
    Difficulty difficulty = Difficulty::Easy;
    minesweeper::difficultyFromName(level.toStdString(), difficulty);
    const minesweeper::Layout layout = minesweeper::layoutFor(difficulty);
    m_field = minesweeper::Field(layout.cols, layout.rows, layout.mines);
    m_seed = QRandomGenerator::global()->generate64();
    m_exploded = -1;
    m_origin = -1;
    m_cascades = 0;
    m_beforeLoss.clear();
    setFlagMode(false);
    setPaused(false);
    beginGame();
}

QVariantMap MinesweeperGame::snapshotOf() const
{
    QVariantMap snap;
    snap.insert(QStringLiteral("mines"), encodeMines(m_field));
    snap.insert(QStringLiteral("states"), encodeStates(m_field));
    snap.insert(QStringLiteral("exploded"), m_exploded);
    snap.insert(QStringLiteral("origin"), m_origin);
    snap.insert(QStringLiteral("cascades"), m_cascades);
    return snap;
}

QVariant MinesweeperGame::snapshot() const
{
    return snapshotOf();
}

void MinesweeperGame::applySnapshot(const QVariant& snapshot)
{
    const QVariantMap snap = snapshot.toMap();
    const QByteArray mines = snap.value(QStringLiteral("mines")).toByteArray();
    const QByteArray states = snap.value(QStringLiteral("states")).toByteArray();
    if (mines.isEmpty()) {
        m_field.clearMines();
    } else {
        std::vector<int> cells;
        for (int i = 0; i < mines.size(); ++i)
            if (mines.at(i))
                cells.push_back(i);
        m_field.placeMines(cells);
    }
    std::vector<Cell> cellStates;
    cellStates.reserve(std::size_t(states.size()));
    for (char c : states)
        cellStates.push_back(static_cast<Cell>(c));
    m_field.setStates(cellStates);
    const bool wasLost = lost();
    m_exploded = snap.value(QStringLiteral("exploded"), -1).toInt();
    m_origin = snap.value(QStringLiteral("origin"), -1).toInt();
    m_cascades = snap.value(QStringLiteral("cascades")).toInt();
    if (wasLost && !lost()) {
        m_beforeLoss.clear();
        setPaused(false);
    }
}

QVariantMap MinesweeperGame::saveState() const
{
    QVariantMap state = lost() ? m_beforeLoss.toMap() : snapshotOf();
    state.insert(QStringLiteral("cols"), m_field.cols());
    state.insert(QStringLiteral("rows"), m_field.rows());
    state.insert(QStringLiteral("mineCount"), m_field.mineCount());
    state.insert(QStringLiteral("seed"), QString::number(m_seed));
    return state;
}

bool MinesweeperGame::restoreState(const QVariantMap& state)
{
    const int cols = state.value(QStringLiteral("cols")).toInt();
    const int rows = state.value(QStringLiteral("rows")).toInt();
    const int mineCount = state.value(QStringLiteral("mineCount")).toInt();
    const qsizetype cells = qsizetype(cols) * rows;
    const QByteArray mines = state.value(QStringLiteral("mines")).toByteArray();
    const QByteArray states = state.value(QStringLiteral("states")).toByteArray();
    if (cols < 2 || rows < 2 || cols > 64 || rows > 64 || mineCount < 1 || mineCount >= cells
        || states.size() != cells || (!mines.isEmpty() && mines.size() != cells))
        return false;
    for (char c : states)
        if (c < char(Cell::Hidden) || c > char(Cell::Flagged))
            return false;

    m_field = minesweeper::Field(cols, rows, mineCount);
    m_seed = state.value(QStringLiteral("seed")).toString().toULongLong();
    m_exploded = -1;
    m_beforeLoss.clear();
    applySnapshot(state);
    m_exploded = -1; // a lost game is saved as it was just before the mine
    setFlagMode(false);
    return true;
}

bool MinesweeperGame::option(const QString& key, bool fallback) const
{
    return m_settings.value(QStringLiteral("minesweeper/") + key, fallback).toBool();
}

void MinesweeperGame::setOption(const QString& key, bool on)
{
    if (option(key, !on) == on && m_settings.contains(QStringLiteral("minesweeper/") + key))
        return;
    m_settings.setValue(QStringLiteral("minesweeper/") + key, on);
    emit optionsChanged();
}
