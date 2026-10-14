#pragma once

#include <GameSession.h>

#include <mahjong/Board.h>
#include <mahjong/Generator.h>

#include <QQmlEngine>
#include <QSettings>
#include <QVariantList>

// The Mahjong solitaire session: a layout of stacked tiles, cleared by taking
// matching pairs of free tiles, built on the framework's GameSession.
//
// Every deal can be cleared. Hints show a pair that keeps the game winnable
// whenever the session can prove one, and a shuffle rearranges the tiles left
// (into a winnable order) when the player runs out of moves.
class MahjongGame : public GameSession {
    Q_OBJECT
    QML_ELEMENT

    // Every layout, gentlest first: [{ name, difficulty, tiles }].
    Q_PROPERTY(QVariantList layouts READ layouts CONSTANT)

    // The current layout, in half-tile units (see mahjong::Slot).
    Q_PROPERTY(int boardWidth READ boardWidth NOTIFY layoutChanged)
    Q_PROPERTY(int boardHeight READ boardHeight NOTIFY layoutChanged)
    Q_PROPERTY(int boardLayers READ boardLayers NOTIFY layoutChanged)
    // One [x, y, z] list per tile, drawing order (lower layers first).
    Q_PROPERTY(QVariantList tileSlots READ slotList NOTIFY layoutChanged)

    // Per tile: its face (0-41, see mahjong/Tiles.h), whether it's still on
    // the table, and whether it's free to take.
    Q_PROPERTY(QVariantList kinds READ kinds NOTIFY stateChanged)
    Q_PROPERTY(QVariantList present READ present NOTIFY stateChanged)
    Q_PROPERTY(QVariantList free READ freeList NOTIFY stateChanged)
    Q_PROPERTY(int tilesLeft READ tilesLeft NOTIFY stateChanged)
    // Pairs the player could take right now; 0 with tiles left means stuck.
    Q_PROPERTY(int movesAvailable READ movesAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool stuck READ stuck NOTIFY stateChanged)

    // The selected tile, or -1.
    Q_PROPERTY(int selected READ selected NOTIFY selectionChanged)
    // The pair the last hint pointed at (empty when none is showing).
    Q_PROPERTY(QVariantList hintPair READ hintPair NOTIFY hintChanged)

    // A short code for this deal ("turtle-2k9x3f"), and its share link.
    Q_PROPERTY(QString dealCode READ dealCode NOTIFY stateChanged)

    // Options, stored under mahjong/.
    Q_PROPERTY(QString tileTheme READ tileTheme WRITE setTileTheme NOTIFY optionsChanged)
    Q_PROPERTY(bool dimBlocked READ dimBlocked WRITE setDimBlocked NOTIFY optionsChanged)
    Q_PROPERTY(bool showTimer READ showTimer WRITE setShowTimer NOTIFY optionsChanged)

public:
    explicit MahjongGame(QObject* parent = nullptr);

    QString gameId() const override { return QStringLiteral("mahjong"); }

    static QVariantList layouts();
    int boardWidth() const { return m_width; }
    int boardHeight() const { return m_height; }
    int boardLayers() const { return m_layers; }
    QVariantList slotList() const;

    QVariantList kinds() const;
    QVariantList present() const;
    QVariantList freeList() const;
    int tilesLeft() const { return m_board.remaining(); }
    int movesAvailable() const { return int(m_board.availablePairs().size()); }
    bool stuck() const { return active() && !complete() && tilesLeft() > 0 && movesAvailable() == 0; }
    int selected() const { return m_selected; }
    QVariantList hintPair() const { return m_hint; }
    QString dealCode() const;

    QString tileTheme() const { return m_settings.value(QStringLiteral("mahjong/tileTheme"), QStringLiteral("jade")).toString(); }
    void setTileTheme(const QString& theme);
    bool dimBlocked() const { return option(QStringLiteral("dimBlocked"), true); }
    void setDimBlocked(bool on) { setOption(QStringLiteral("dimBlocked"), on); }
    bool showTimer() const { return option(QStringLiteral("showTimer"), true); }
    void setShowTimer(bool on) { setOption(QStringLiteral("showTimer"), on); }

    // Selects a free tile, or takes the pair if it matches the selection.
    // Tapping a blocked tile fires blockedTapped.
    Q_INVOKABLE void tap(int tile);
    // Points out a pair to take (one that keeps the game winnable when it can
    // tell). Returns false when there is no pair at all.
    Q_INVOKABLE bool hint();
    // Rearranges the tiles left into a winnable order. Returns false if the
    // tiles left can't be cleared however they're arranged.
    Q_INVOKABLE bool shuffle();

    // Share links: https://mahjong.gwynant.app/s/<deal code>.
    Q_INVOKABLE QString shareUrl() const;
    // Starts the deal in a share link (the whole URL or just its code).
    // Returns false if the link holds no deal.
    Q_INVOKABLE bool openShared(const QString& link);

    // "3 of Bamboo", "Red dragon"... for accessibility.
    Q_INVOKABLE static QString kindName(int kind);

    static inline const QString ShareUrlBase = QStringLiteral("https://mahjong.gwynant.app/s/");

signals:
    void layoutChanged();
    void selectionChanged();
    void hintChanged();
    void optionsChanged();
    // A blocked tile was tapped (the board gives it a gentle shake).
    void blockedTapped(int tile);
    // A pair was taken off the table.
    void matched(int first, int second);

protected:
    void createGame(const QString& level) override;
    QVariantMap saveState() const override;
    bool restoreState(const QVariantMap& state) override;
    QVariant snapshot() const override;
    void applySnapshot(const QVariant& snapshot) override;

private:
    void setLayout(const mahjong::Layout& layout);
    void setSelected(int tile);
    void clearHint();
    // Keeps m_plan (a known way to win from here) up to date after a move.
    void followPlan(int a, int b);

    bool option(const QString& key, bool fallback) const;
    void setOption(const QString& key, bool on);

    const mahjong::Layout* m_layout = nullptr;
    mahjong::Board m_board;
    mahjong::Plan m_plan;
    quint64 m_seed = 0;
    bool m_seedKnown = false;
    int m_width = 0;
    int m_height = 0;
    int m_layers = 0;
    int m_selected = -1;
    QVariantList m_hint;
    // Handed from openShared() to createGame().
    bool m_pendingSeedSet = false;
    quint64 m_pendingSeed = 0;
    mutable QSettings m_settings;
};
