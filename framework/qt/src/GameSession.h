#pragma once

#include <calm/UndoStack.h>

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>

// Base class for every Calm game. It owns everything games have in common:
// the play timer, pausing, undo/redo, autosave and resume, statistics, and the
// feedback vocabulary that drives sounds and haptics.
//
// A game subclass implements the five pure virtuals and follows two rules:
//   * wrap every player move in beginMove() ... endMove()
//   * call beginGame() once a new game is ready (sync or async) and
//     finishGame() when the player wins
class GameSession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("GameSession is the base class for games")

    Q_PROPERTY(QString gameId READ gameId CONSTANT)
    Q_PROPERTY(QString level READ level NOTIFY levelChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY elapsedSecondsChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool complete READ complete NOTIFY completeChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(bool hasSavedGame READ hasSavedGame NOTIFY hasSavedGameChanged)
    // Hints applied in the current game; 0 for a fresh game, kept on resume.
    Q_PROPERTY(int hintsUsed READ hintsUsed NOTIFY hintsUsedChanged)

public:
    explicit GameSession(QObject* parent = nullptr);

    virtual QString gameId() const = 0;

    QString level() const { return m_level; }
    int elapsedSeconds() const { return m_elapsed; }
    bool paused() const { return m_paused; }
    void setPaused(bool paused);
    bool loading() const { return m_loading; }
    bool active() const { return m_active; }
    bool complete() const { return m_complete; }
    bool canUndo() const { return m_history.canUndo(); }
    bool canRedo() const { return m_history.canRedo(); }
    bool hasSavedGame() const;
    int hintsUsed() const { return m_hintsUsed; }

    // Starts generating a new game; `active` turns true once it is ready.
    Q_INVOKABLE void startNewGame(const QString& level);
    // Restores the autosaved game, if any. Returns false when there is none.
    Q_INVOKABLE bool resume();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    // Abandons the current game (counts as played, not won).
    Q_INVOKABLE void abandon();

signals:
    void levelChanged();
    void elapsedSecondsChanged();
    void pausedChanged();
    void loadingChanged();
    void activeChanged();
    void completeChanged();
    void historyChanged();
    void hasSavedGameChanged();
    void hintsUsedChanged();
    // Any visible game state changed (board, selection, cards...).
    void stateChanged();
    // Shared sound/haptic vocabulary: "tap", "soft", "erase", "error",
    // "complete" (step = how many completions so far), "win".
    void feedback(const QString& event, int step);
    void won(int seconds);

protected:
    // Build a new game for `level`, then call beginGame() (now or later).
    virtual void createGame(const QString& level) = 0;
    // Full game state for autosave, and its inverse.
    virtual QVariantMap saveState() const = 0;
    virtual bool restoreState(const QVariantMap& state) = 0;
    // Cheap copy of the mutable state for undo, and its inverse.
    virtual QVariant snapshot() const = 0;
    virtual void applySnapshot(const QVariant& snapshot) = 0;

    void beginGame();
    void beginMove();
    void endMove();
    void finishGame();
    // Call when a hint is applied (inside the move, before any finishGame()),
    // so stats can tell wins with and without hints apart.
    void noteHintUsed();
    void emitFeedback(const QString& event, int step = 0) { emit feedback(event, step); }

private:
    void setLevel(const QString& level);
    void setActive(bool active);
    void setLoading(bool loading);
    void setComplete(bool complete);
    void setHintsUsed(int hints);
    void updateTimer();
    void autosave();
    void tick();

    QString m_level;
    int m_elapsed = 0;
    int m_hintsUsed = 0;
    bool m_paused = false;
    bool m_loading = false;
    bool m_active = false;
    bool m_complete = false;
    QTimer m_timer;
    calm::UndoStack<QVariant> m_history;
};
