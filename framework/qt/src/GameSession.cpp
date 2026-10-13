#include "GameSession.h"

#include "SaveStore.h"
#include "Stats.h"

GameSession::GameSession(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &GameSession::tick);
}

bool GameSession::hasSavedGame() const
{
    return SaveStore::exists(gameId());
}

void GameSession::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    emit pausedChanged();
    updateTimer();
    if (paused)
        autosave();
}

void GameSession::startNewGame(const QString& level)
{
    if (m_loading)
        return;
    if (m_active && !m_complete)
        abandon();
    setLevel(level);
    setComplete(false);
    setLoading(true);
    createGame(level);
}

void GameSession::beginGame()
{
    m_elapsed = 0;
    emit elapsedSecondsChanged();
    setHintsUsed(0);
    m_history.clear();
    emit historyChanged();
    setComplete(false);
    setLoading(false);
    setActive(true);
    Stats::recordStarted(gameId(), m_level);
    autosave();
    emit stateChanged();
    updateTimer();
}

bool GameSession::resume()
{
    const QVariantMap saved = SaveStore::load(gameId());
    if (saved.isEmpty())
        return false;
    if (!restoreState(saved.value(QStringLiteral("game")).toMap())) {
        SaveStore::remove(gameId());
        emit hasSavedGameChanged();
        return false;
    }
    setLevel(saved.value(QStringLiteral("level")).toString());
    m_elapsed = saved.value(QStringLiteral("elapsed")).toInt();
    emit elapsedSecondsChanged();
    // Saves from before hint tracking have no count: treat them as hint-free.
    setHintsUsed(saved.value(QStringLiteral("hintsUsed")).toInt());
    m_history.clear();
    emit historyChanged();
    setComplete(false);
    setActive(true);
    emit stateChanged();
    updateTimer();
    return true;
}

void GameSession::beginMove()
{
    if (!m_active || m_complete)
        return;
    m_history.push(snapshot());
    emit historyChanged();
}

void GameSession::endMove()
{
    autosave();
    emit stateChanged();
}

void GameSession::undo()
{
    if (!m_active || m_complete)
        return;
    if (auto previous = m_history.undo(snapshot())) {
        applySnapshot(*previous);
        emit historyChanged();
        emitFeedback(QStringLiteral("soft"));
        endMove();
    }
}

void GameSession::redo()
{
    if (!m_active || m_complete)
        return;
    if (auto next = m_history.redo(snapshot())) {
        applySnapshot(*next);
        emit historyChanged();
        emitFeedback(QStringLiteral("soft"));
        endMove();
    }
}

void GameSession::abandon()
{
    if (!m_active)
        return;
    setActive(false);
    SaveStore::remove(gameId());
    emit hasSavedGameChanged();
    updateTimer();
}

void GameSession::finishGame()
{
    if (m_complete)
        return;
    setComplete(true);
    updateTimer();
    Stats::recordWon(gameId(), m_level, m_elapsed, m_hintsUsed > 0);
    SaveStore::remove(gameId());
    emit hasSavedGameChanged();
    emit stateChanged();
    emitFeedback(QStringLiteral("win"));
    emit won(m_elapsed);
}

void GameSession::noteHintUsed()
{
    if (!m_active || m_complete)
        return;
    setHintsUsed(m_hintsUsed + 1);
}

void GameSession::setHintsUsed(int hints)
{
    if (m_hintsUsed == hints)
        return;
    m_hintsUsed = hints;
    emit hintsUsedChanged();
}

void GameSession::setLevel(const QString& level)
{
    if (m_level == level)
        return;
    m_level = level;
    emit levelChanged();
}

void GameSession::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();
}

void GameSession::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void GameSession::setComplete(bool complete)
{
    if (m_complete == complete)
        return;
    m_complete = complete;
    emit completeChanged();
}

void GameSession::updateTimer()
{
    const bool run = m_active && !m_complete && !m_paused;
    if (run && !m_timer.isActive())
        m_timer.start();
    else if (!run)
        m_timer.stop();
}

void GameSession::autosave()
{
    if (!m_active || m_complete)
        return;
    QVariantMap saved;
    saved.insert(QStringLiteral("level"), m_level);
    saved.insert(QStringLiteral("elapsed"), m_elapsed);
    saved.insert(QStringLiteral("hintsUsed"), m_hintsUsed);
    saved.insert(QStringLiteral("game"), saveState());
    const bool existed = SaveStore::exists(gameId());
    SaveStore::save(gameId(), saved);
    if (!existed)
        emit hasSavedGameChanged();
}

void GameSession::tick()
{
    ++m_elapsed;
    emit elapsedSecondsChanged();
}
