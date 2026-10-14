#pragma once

#include <GameSession.h>

#include <solitaire/Solver.h>

#include <QHash>
#include <QList>
#include <QQmlEngine>
#include <QSettings>
#include <QTimer>
#include <QVariantList>
#include <QtGlobal>

// Deals and losing-move checks run on worker threads where Qt has threads. A
// single-threaded build (WebAssembly) runs them in slices on the event loop
// instead, between frames.
#if QT_CONFIG(thread)
#define CALM_SOLITAIRE_THREADS 1
#include <QFutureWatcher>
#else
#define CALM_SOLITAIRE_THREADS 0
#endif

// The Solitaire game session: the table, player moves, hints, auto-complete
// and losing-move warnings, built on the framework's GameSession. Each
// level is one game: "Klondike", "Klondike · Draw three", "FreeCell",
// "Spider · One suit" (two, four), "Yukon", "Pyramid", "TriPeaks", "Golf".
class SolitaireGame : public GameSession {
    Q_OBJECT
    QML_ELEMENT

    // The game being played: "klondike", "freecell", "spider", ...
    Q_PROPERTY(QString variant READ variant NOTIFY stateChanged)
    // One map per card in the deck (52, or 104 for Spider): { rank, suit,
    // red, faceUp, pile, slot, pos, count, movable }. `pile` is a Pile value,
    // `slot` its index, `pos` the card's place in its pile of `count` cards.
    Q_PROPERTY(QVariantList cards READ cards NOTIFY stateChanged)
    // Where the piles go: { pile, slot, x, y, fan, bottom, mark, outline }.
    // `x` is in columns (a card and a gap), `y` in card heights. `fan` is
    // 0 (stacked), 1 (down) or 2 (the last three to the right); a downward
    // fan ends by `bottom` (card heights) if that is above 0. `mark` is the
    // suit drawn in an empty foundation (-1 for none); `outline` whether an
    // empty place shows.
    Q_PROPERTY(QVariantList piles READ piles NOTIFY stateChanged)
    // The table's size in columns and card heights (its least height).
    Q_PROPERTY(qreal tableColumns READ tableColumns NOTIFY stateChanged)
    Q_PROPERTY(qreal tableRows READ tableRows NOTIFY stateChanged)
    // Pyramid, TriPeaks and Golf: a single tap plays a card.
    Q_PROPERTY(bool tapToPlay READ tapToPlay NOTIFY stateChanged)
    // Pyramid: the card picked first, waiting for its partner (-1 if none).
    Q_PROPERTY(int selectedCard READ selectedCard NOTIFY selectedCardChanged)
    Q_PROPERTY(int stockCount READ stockCount NOTIFY stateChanged)
    Q_PROPERTY(int wasteCount READ wasteCount NOTIFY stateChanged)
    Q_PROPERTY(bool canDraw READ canDraw NOTIFY stateChanged)
    // Pyramid: turns of the stock left after this one (-1: endless).
    Q_PROPERTY(int passesLeft READ passesLeft NOTIFY stateChanged)
    Q_PROPERTY(int drawCount READ drawCount NOTIFY stateChanged)
    Q_PROPERTY(int moves READ moves NOTIFY stateChanged)
    Q_PROPERTY(int foundationCards READ foundationCards NOTIFY stateChanged)
    // Cards home (or cleared) out of `goal`.
    Q_PROPERTY(int progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(int goal READ goal NOTIFY stateChanged)
    Q_PROPERTY(bool canAutoFinish READ canAutoFinish NOTIFY stateChanged)
    Q_PROPERTY(bool autoFinishing READ autoFinishing NOTIFY autoFinishingChanged)
    // The last move left the deal unwinnable (found by the solver after the
    // move, so it turns true a moment later). Cleared by the next move.
    Q_PROPERTY(bool losingMove READ losingMove NOTIFY losingMoveChanged)
    // The current hint: { cards: [indexes], stock: bool, pile, slot }, or empty.
    Q_PROPERTY(QVariantMap hint READ hint NOTIFY hintChanged)

    Q_PROPERTY(bool winnableDeals READ winnableDeals WRITE setWinnableDeals NOTIFY optionsChanged)
    Q_PROPERTY(bool showTimer READ showTimer WRITE setShowTimer NOTIFY optionsChanged)
    Q_PROPERTY(bool autoCompleteButton READ autoCompleteButton WRITE setAutoCompleteButton NOTIFY optionsChanged)
    Q_PROPERTY(bool warnLosingMoves READ warnLosingMoves WRITE setWarnLosingMoves NOTIFY optionsChanged)
    // The home screen's picks for Klondike (1 or 3) and Spider (1, 2 or 4).
    Q_PROPERTY(int klondikeDraw READ klondikeDraw WRITE setKlondikeDraw NOTIFY optionsChanged)
    Q_PROPERTY(int spiderSuits READ spiderSuits WRITE setSpiderSuits NOTIFY optionsChanged)

public:
    enum Pile { Stock, Waste, Foundation, Tableau, Cell };
    Q_ENUM(Pile)

    explicit SolitaireGame(QObject* parent = nullptr);
    ~SolitaireGame() override;

    QString gameId() const override { return QStringLiteral("solitaire"); }

    QString variant() const;
    QVariantList cards() const;
    QVariantList piles() const;
    qreal tableColumns() const;
    qreal tableRows() const;
    bool tapToPlay() const;
    int selectedCard() const { return m_selected; }
    int stockCount() const { return int(m_table.stock().size()); }
    int wasteCount() const { return int(m_table.waste().size()); }
    bool canDraw() const { return m_table.canDraw(); }
    int passesLeft() const { return m_table.passesLeft(); }
    int drawCount() const { return m_table.drawCount(); }
    int moves() const { return m_moves; }
    int foundationCards() const { return m_table.foundationCards(); }
    int progress() const { return m_table.progress(); }
    int goal() const { return m_table.goal(); }
    bool canAutoFinish() const;
    bool autoFinishing() const { return m_finishTimer.isActive(); }
    bool losingMove() const { return m_losingMove; }
    QVariantMap hint() const { return m_hint; }

    bool winnableDeals() const { return option(QStringLiteral("winnableDeals"), true); }
    void setWinnableDeals(bool on) { setOption(QStringLiteral("winnableDeals"), on); }
    bool showTimer() const { return option(QStringLiteral("showTimer"), true); }
    void setShowTimer(bool on) { setOption(QStringLiteral("showTimer"), on); }
    bool autoCompleteButton() const { return option(QStringLiteral("autoCompleteButton"), true); }
    void setAutoCompleteButton(bool on) { setOption(QStringLiteral("autoCompleteButton"), on); }
    bool warnLosingMoves() const { return option(QStringLiteral("warnLosingMoves"), true); }
    void setWarnLosingMoves(bool on) { setOption(QStringLiteral("warnLosingMoves"), on); }
    int klondikeDraw() const;
    void setKlondikeDraw(int draw);
    int spiderSuits() const;
    void setSpiderSuits(int suits);

    // The level name for a game ("klondike", "spider", ...) and its option.
    Q_INVOKABLE static QString levelFor(const QString& variant, int option = 0);

    // Deals from the stock, or turns the waste back over.
    Q_INVOKABLE void draw();
    // Double tap: sends the card (and any cards on it) home, else the
    // leftmost column it fits on. False if it fits nowhere.
    Q_INVOKABLE bool quickMove(int card);
    // A single tap in Pyramid, TriPeaks and Golf. Pyramid picks the card
    // (or removes a king), and a second tap pairs it with another card.
    // False if the card can't be played.
    Q_INVOKABLE bool tapCard(int card);
    // Whether the card (and any cards on it) may be dropped on a pile.
    Q_INVOKABLE bool canDrop(int card, int pile, int slot) const;
    // Moves the card (and any cards on it) onto a pile; false if illegal.
    Q_INVOKABLE bool dropCard(int card, int pile, int slot);
    // Highlights a helpful move. Returns false when nothing helps.
    Q_INVOKABLE bool showHint();
    // Plays every remaining card home, one by one, once canAutoFinish.
    Q_INVOKABLE void autoFinish();

signals:
    void hintChanged();
    void optionsChanged();
    void autoFinishingChanged();
    void losingMoveChanged();
    void selectedCardChanged();

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

    void setTable(const solitaire::Game& table);
    int indexOf(const solitaire::Card& card) const;
    std::optional<Location> locate(int card) const;
    bool perform(const solitaire::Move& move);
    void setSelected(int card);
    std::optional<solitaire::Hint> plannedStep();
    void clearHint();
    std::optional<std::vector<solitaire::Hint>> finishPath() const;
    void finishStep();
    void stopAutoFinish();
    void setLosingMove(bool losing);
    void checkForLosingMove(const std::string& before);
    void startAnalysis();
    void onVerdict(const std::string& before, const QByteArray& after, solitaire::Verdict verdict);

    // Deals waiting or being made, by level and whether they must be winnable.
    static int bufferKey(int level, bool winnable) { return level * 2 + (winnable ? 1 : 0); }
    void requestDeal(int level, bool winnable);
    void onDealReady(int key, const solitaire::Deal& deal);
    void startWith(const solitaire::Deal& deal);

    bool option(const QString& key, bool fallback) const;
    void setOption(const QString& key, bool on);

    solitaire::Game m_table;
    std::vector<int> m_cardIndex; // card id -> index in `cards`
    quint64 m_seed = 0;
    int m_moves = 0;
    int m_selected = -1;
    QVariantMap m_hint;
    // A winning line from the solver: each step with the position before it.
    std::vector<std::pair<std::string, solitaire::Hint>> m_plan;
    QTimer m_finishTimer;
    std::vector<solitaire::Hint> m_finishSteps;
    // Auto-complete line for the position it was worked out for.
    mutable std::string m_finishFor;
    mutable std::optional<std::vector<solitaire::Hint>> m_finishFound;

    // Losing-move checks run one at a time in the background; the latest
    // waiting position replaces any older one.
    struct Analysis {
        std::string before;
        solitaire::Game after;
    };
    bool m_losingMove = false;
    QHash<QByteArray, int> m_verdicts; // position -> solitaire::Verdict
    std::optional<Analysis> m_pendingAnalysis;
#if CALM_SOLITAIRE_THREADS
    QFutureWatcher<solitaire::Verdict>* m_analysis = nullptr;
    std::atomic_bool m_stopAnalysis { false };
#endif

    std::optional<int> m_waitingFor;
    QHash<int, QList<solitaire::Deal>> m_buffer;
    QHash<int, int> m_inFlight;
#if CALM_SOLITAIRE_THREADS
    QList<QFutureWatcher<solitaire::Deal>*> m_watchers;
#else
    struct DealJob {
        int key;
        solitaire::SteppedDeal search;
    };
    void scheduleWork();
    void workNext();
    QList<std::shared_ptr<DealJob>> m_pendingDeals;
    // The losing-move check under way, searched a slice per timer tick.
    struct RunningAnalysis {
        std::string before;
        QByteArray after;
        solitaire::SteppedSolve search;
    };
    std::optional<RunningAnalysis> m_runningAnalysis;
    QTimer m_workTimer;
    bool m_warmedUp = false;
#endif
    mutable QSettings m_settings;
};
