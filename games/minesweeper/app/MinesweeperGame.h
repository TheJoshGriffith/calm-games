#pragma once

#include <GameSession.h>

#include <minesweeper/Field.h>

#include <QQmlEngine>
#include <QSettings>
#include <QVariantList>

// The Minesweeper game session: the minefield, player input, hints and
// game-specific options, built on the framework's GameSession.
//
// A calm take on the classic: the first tap always opens an area, boards can
// be solved by logic alone (unless the player turns that off), and stepping
// on a mine can be taken back with undo.
class MinesweeperGame : public GameSession {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int columns READ columns NOTIFY stateChanged)
    Q_PROPERTY(int rows READ rows NOTIFY stateChanged)
    // One int per cell, row-major: Hidden, Flagged, 0-8 (revealed count),
    // or after a loss Mine, Exploded, WrongFlag.
    Q_PROPERTY(QVariantList cells READ cells NOTIFY stateChanged)
    Q_PROPERTY(int minesLeft READ minesLeft NOTIFY stateChanged)
    Q_PROPERTY(bool lost READ lost NOTIFY stateChanged)
    // The cell the last reveal started from, so the board can ripple out from it.
    Q_PROPERTY(int origin READ origin NOTIFY stateChanged)
    Q_PROPERTY(bool flagMode READ flagMode WRITE setFlagMode NOTIFY flagModeChanged)

    Q_PROPERTY(bool noGuessing READ noGuessing WRITE setNoGuessing NOTIFY optionsChanged)
    Q_PROPERTY(bool showTimer READ showTimer WRITE setShowTimer NOTIFY optionsChanged)

public:
    enum CellView { Hidden = -1, Flagged = -2, Mine = 9, Exploded = 10, WrongFlag = 11 };
    Q_ENUM(CellView)

    explicit MinesweeperGame(QObject* parent = nullptr);

    QString gameId() const override { return QStringLiteral("minesweeper"); }

    int columns() const { return m_field.cols(); }
    int rows() const { return m_field.rows(); }
    QVariantList cells() const;
    int minesLeft() const { return m_field.mineCount() - m_field.flagCount(); }
    bool lost() const { return m_exploded >= 0; }
    int origin() const { return m_origin; }
    bool flagMode() const { return m_flagMode; }
    void setFlagMode(bool on);

    bool noGuessing() const { return option(QStringLiteral("noGuessing"), true); }
    void setNoGuessing(bool on) { setOption(QStringLiteral("noGuessing"), on); }
    bool showTimer() const { return option(QStringLiteral("showTimer"), true); }
    void setShowTimer(bool on) { setOption(QStringLiteral("showTimer"), on); }

    // A plain tap: flags in flag mode, otherwise opens.
    Q_INVOKABLE void tap(int cell);
    // Opens a hidden cell, or on a satisfied number opens its neighbours.
    Q_INVOKABLE void open(int cell);
    // Flags or unflags a hidden cell (long press or right click). On a number
    // whose hidden neighbours must all be mines, flags every one of them.
    Q_INVOKABLE void flag(int cell);
    // Clears a wrong flag, or opens or flags a cell the numbers prove.
    // Returns false if nothing can be proved.
    Q_INVOKABLE bool hint();

signals:
    void flagModeChanged();
    void optionsChanged();

protected:
    void createGame(const QString& level) override;
    QVariantMap saveState() const override;
    bool restoreState(const QVariantMap& state) override;
    QVariant snapshot() const override;
    void applySnapshot(const QVariant& snapshot) override;

private:
    bool playable(int cell) const;
    // A hinted move also counts towards hintsUsed.
    void openCell(int cell, bool hinted);
    void flagCell(int cell, bool hinted);
    void afterReveal(const minesweeper::RevealResult& result, const QVariantMap& before);
    QVariantMap snapshotOf() const;

    bool option(const QString& key, bool fallback) const;
    void setOption(const QString& key, bool on);

    minesweeper::Field m_field;
    quint64 m_seed = 0;
    int m_exploded = -1;
    int m_origin = -1;
    int m_cascades = 0;
    bool m_flagMode = false;
    QVariant m_beforeLoss; // the board just before the mine, for saving
    mutable QSettings m_settings;
};
