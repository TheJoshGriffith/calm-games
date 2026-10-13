#pragma once

#include <GameSession.h>

#include <solitaire/Solver.h>

#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QQmlEngine>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

// The Klondike game session: the table, player moves, hints and auto-finish,
// built on the framework's GameSession. Levels are "Draw one" and "Draw three".
class SolitaireGame : public GameSession {
    Q_OBJECT
    QML_ELEMENT

    // 52 maps indexed by card id: { rank, suit, red, faceUp, pile, slot,
    // pos, count, movable }. `pile` is a Pile value, `slot` the column or
    // foundation, `pos` the card's place in its pile of `count` cards.
    Q_PROPERTY(QVariantList cards READ cards NOTIFY stateChanged)
    Q_PROPERTY(int stockCount READ stockCount NOTIFY stateChanged)
    Q_PROPERTY(int wasteCount READ wasteCount NOTIFY stateChanged)
    Q_PROPERTY(int drawCount READ drawCount NOTIFY stateChanged)
    Q_PROPERTY(int moves READ moves NOTIFY stateChanged)
    Q_PROPERTY(int foundationCards READ foundationCards NOTIFY stateChanged)
    Q_PROPERTY(bool canAutoFinish READ canAutoFinish NOTIFY stateChanged)
    Q_PROPERTY(bool autoFinishing READ autoFinishing NOTIFY autoFinishingChanged)
    // The current hint: { cards: [ids], stock: bool, pile, slot }, or empty.
    Q_PROPERTY(QVariantMap hint READ hint NOTIFY hintChanged)

    Q_PROPERTY(bool winnableDeals READ winnableDeals WRITE setWinnableDeals NOTIFY optionsChanged)
    Q_PROPERTY(bool showTimer READ showTimer WRITE setShowTimer NOTIFY optionsChanged)
    Q_PROPERTY(bool tapToMove READ tapToMove WRITE setTapToMove NOTIFY optionsChanged)

public:
    enum Pile { Stock, Waste, Foundation, Tableau };
    Q_ENUM(Pile)

    explicit SolitaireGame(QObject* parent = nullptr);
    ~SolitaireGame() override;

    QString gameId() const override { return QStringLiteral("solitaire"); }

    QVariantList cards() const;
    int stockCount() const { return int(m_table.stock().size()); }
    int wasteCount() const { return int(m_table.waste().size()); }
    int drawCount() const { return m_table.drawCount(); }
    int moves() const { return m_moves; }
    int foundationCards() const { return m_table.foundationCards(); }
    bool canAutoFinish() const { return active() && !complete() && m_table.canAutoFinish(); }
    bool autoFinishing() const { return m_finishTimer.isActive(); }
    QVariantMap hint() const { return m_hint; }

    bool winnableDeals() const { return option(QStringLiteral("winnableDeals"), true); }
    void setWinnableDeals(bool on) { setOption(QStringLiteral("winnableDeals"), on); }
    bool showTimer() const { return option(QStringLiteral("showTimer"), true); }
    void setShowTimer(bool on) { setOption(QStringLiteral("showTimer"), on); }
    bool tapToMove() const { return option(QStringLiteral("tapToMove"), true); }
    void setTapToMove(bool on) { setOption(QStringLiteral("tapToMove"), on); }

    // Deals from the stock, or turns the waste back over when it is empty.
    Q_INVOKABLE void draw();
    // Sends the card (and any cards on it) to the best place, if any.
    Q_INVOKABLE bool tapCard(int cardId);
    // Whether the card (and any cards on it) may be dropped on a pile.
    Q_INVOKABLE bool canDrop(int cardId, int pile, int slot) const;
    // Moves the card (and any cards on it) onto a pile; false if illegal.
    Q_INVOKABLE bool dropCard(int cardId, int pile, int slot);
    // Highlights a helpful move. Returns false when nothing helps.
    Q_INVOKABLE bool showHint();
    // Plays every remaining card home, one by one.
    Q_INVOKABLE void autoFinish();

    static int drawCountFor(const QString& level);

signals:
    void hintChanged();
    void optionsChanged();
    void autoFinishingChanged();

protected:
    void createGame(const QString& level) override;
    QVariantMap saveState() const override;
    bool restoreState(const QVariantMap& state) override;
    QVariant snapshot() const override;
    void applySnapshot(const QVariant& snapshot) override;

private:
    struct Location {
        solitaire::PileRef pile;
        int pos = -1;
    };

    std::optional<Location> locate(int cardId) const;
    bool perform(const solitaire::Move& move);
    std::optional<solitaire::Hint> plannedStep();
    void clearHint();
    void finishStep();
    void stopAutoFinish();

    int bufferKey(int drawCount, bool winnable) const { return drawCount * 2 + (winnable ? 1 : 0); }
    void requestDeal(int drawCount, bool winnable);
    void onDealReady(int key, const solitaire::Deal& deal);
    void startWith(const solitaire::Deal& deal);

    bool option(const QString& key, bool fallback) const;
    void setOption(const QString& key, bool on);

    solitaire::Klondike m_table;
    quint64 m_seed = 0;
    int m_moves = 0;
    QVariantMap m_hint;
    // A winning line from the solver: each step with the position before it.
    std::vector<std::pair<std::string, solitaire::Hint>> m_plan;
    QTimer m_finishTimer;

    std::optional<int> m_waitingFor;
    QHash<int, QList<solitaire::Deal>> m_buffer;
    QHash<int, int> m_inFlight;
    QList<QFutureWatcher<solitaire::Deal>*> m_watchers;
    mutable QSettings m_settings;
};
