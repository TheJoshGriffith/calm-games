#pragma once

#include <GameSession.h>

#include <sudoku/Generator.h>

#include <QHash>
#include <QList>
#include <QQmlEngine>
#include <QSettings>
#include <QTimer>
#include <QVariantList>
#include <QtGlobal>

// Puzzles are generated on a worker thread where Qt has threads. A single-
// threaded build (WebAssembly) generates them one at a time on the event
// loop instead, between frames.
#if QT_CONFIG(thread)
#define CALM_SUDOKU_THREADED_GENERATION 1
#include <QFutureWatcher>
#else
#define CALM_SUDOKU_THREADED_GENERATION 0
#endif

#include <array>

// The Sudoku game session: board state, player input (multi-cell selection,
// digits, corner and centre marks, colours), hints and game-specific options,
// built on the framework's GameSession.
class SudokuGame : public GameSession {
    Q_OBJECT
    QML_ELEMENT

    // 81 maps: { digit, given, corner, centre (bitmasks, bit d = digit d),
    //            colour (0 none, 1-9), mistake }
    Q_PROPERTY(QVariantList cells READ cells NOTIFY stateChanged)
    // Digits still to place, index 0 = digit 1.
    Q_PROPERTY(QVariantList remaining READ remaining NOTIFY stateChanged)
    // The primary (most recently touched) selected cell, or -1. Peer and
    // matching highlights follow it. Writing it selects just that cell.
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged)
    // Every selected cell, in the order they were added; the last is `selected`.
    Q_PROPERTY(QVariantList selection READ selection NOTIFY selectedChanged)
    // When on, tapping a cell adds it to (or removes it from) the selection.
    Q_PROPERTY(bool multiSelect READ multiSelect WRITE setMultiSelect NOTIFY multiSelectChanged)
    Q_PROPERTY(int inputMode READ inputMode WRITE setInputMode NOTIFY inputModeChanged)
    // With nothing selected, pressing a digit focuses it (0 = none): the board
    // highlights that digit and shades every cell it can no longer go in.
    // Selecting a cell, or pressing the same digit again, clears it.
    Q_PROPERTY(int focusDigit READ focusDigit WRITE setFocusDigit NOTIFY focusDigitChanged)
    Q_PROPERTY(QString technique READ technique NOTIFY stateChanged)
    // The code a friend can type to get this puzzle ("H1-3KX9QZ0A"), or ""
    // when it isn't known (a board opened from a share link carries no seed).
    Q_PROPERTY(QString puzzleCode READ puzzleCode NOTIFY stateChanged)

    Q_PROPERTY(bool highlightPeers READ highlightPeers WRITE setHighlightPeers NOTIFY optionsChanged)
    Q_PROPERTY(bool highlightMatching READ highlightMatching WRITE setHighlightMatching NOTIFY optionsChanged)
    Q_PROPERTY(int mistakeMode READ mistakeMode WRITE setMistakeMode NOTIFY optionsChanged)
    // Number pad layout: width as a fraction of the screen (0.5-1), and its
    // position in the space left over (-1 = far left, 0 = centred, 1 = far right).
    Q_PROPERTY(qreal padWidth READ padWidth WRITE setPadWidth NOTIFY optionsChanged)
    Q_PROPERTY(qreal padOffset READ padOffset WRITE setPadOffset NOTIFY optionsChanged)
    Q_PROPERTY(bool showTimer READ showTimer WRITE setShowTimer NOTIFY optionsChanged)
    Q_PROPERTY(bool autoRemoveNotes READ autoRemoveNotes WRITE setAutoRemoveNotes NOTIFY optionsChanged)

public:
    enum InputMode { NumberMode, CornerMode, CentreMode, ColourMode };
    Q_ENUM(InputMode)
    // Off: never flag. Clashes: a digit repeated in a row, column or box.
    // Answer: a player digit that differs from the solution.
    enum MistakeMode { MistakesOff, MistakesClashes, MistakesAnswer };
    Q_ENUM(MistakeMode)

    explicit SudokuGame(QObject* parent = nullptr);
    ~SudokuGame() override;

    QString gameId() const override { return QStringLiteral("sudoku"); }

    QVariantList cells() const;
    QVariantList remaining() const;
    int selected() const { return m_selection.isEmpty() ? -1 : m_selection.constLast(); }
    void setSelected(int cell);
    QVariantList selection() const;
    bool multiSelect() const { return m_multiSelect; }
    void setMultiSelect(bool on);
    int inputMode() const { return m_inputMode; }
    void setInputMode(int mode);
    int focusDigit() const { return m_focusDigit; }
    void setFocusDigit(int digit);
    QString technique() const;
    QString puzzleCode() const;

    bool highlightPeers() const { return option(QStringLiteral("highlightPeers"), true); }
    void setHighlightPeers(bool on) { setOption(QStringLiteral("highlightPeers"), on); }
    bool highlightMatching() const { return option(QStringLiteral("highlightMatching"), true); }
    void setHighlightMatching(bool on) { setOption(QStringLiteral("highlightMatching"), on); }
    int mistakeMode() const;
    qreal padWidth() const;
    void setPadWidth(qreal width);
    qreal padOffset() const;
    void setPadOffset(qreal offset);
    static constexpr qreal MinPadWidth = 0.5;
    void setMistakeMode(int mode);
    bool showTimer() const { return option(QStringLiteral("showTimer"), true); }
    void setShowTimer(bool on) { setOption(QStringLiteral("showTimer"), on); }
    bool autoRemoveNotes() const { return option(QStringLiteral("autoRemoveNotes"), true); }
    void setAutoRemoveNotes(bool on) { setOption(QStringLiteral("autoRemoveNotes"), on); }

    // Selection editing. selectOnly(-1) clears the selection.
    Q_INVOKABLE void selectOnly(int cell) { setSelected(cell); }
    Q_INVOKABLE void addToSelection(int cell);
    Q_INVOKABLE void removeFromSelection(int cell);
    Q_INVOKABLE void clearSelection() { setSelected(-1); }
    Q_INVOKABLE bool isSelected(int cell) const { return m_selection.contains(cell); }

    // Applies `digit` to the selection according to the input mode: places it
    // (Number), toggles a corner or centre mark, or paints colour `digit`.
    // With nothing selected (outside Colour mode) it toggles focusDigit.
    Q_INVOKABLE void enter(int digit);
    // Colour 1-9 paints the selection (or clears it if every selected cell
    // already has that colour); 0 clears colours.
    Q_INVOKABLE void paint(int colour);
    // Clears entries in the selection; if none, marks; if none, colours.
    Q_INVOKABLE void erase();
    // Fills the selected cell, or the next logical cell if none is selected
    // or the selected one is filled. Returns false if no hint is available.
    Q_INVOKABLE bool hint();

    // Sharing. shareUrl() is ShareUrlBase + a payload (sudoku/Share.h) for the
    // starting board or, withProgress, also the player's digits, corner and
    // centre marks and colours; "" when there is no game.
    Q_INVOKABLE QString shareUrl(bool withProgress) const;
    // Opens a share link: the whole URL or just its payload. It replaces the
    // current game the way startNewGame() does, at the link's level (or the
    // level the puzzle grades as), with the shared progress restored. Returns
    // false and changes nothing if the link holds no puzzle with exactly one
    // solution.
    Q_INVOKABLE bool openShared(const QString& link);
    // Starts the exact puzzle a code names (see puzzleCode), generating it
    // like any new game (`loading` until it is ready). Returns false and
    // changes nothing if the code is not valid.
    Q_INVOKABLE bool startFromCode(const QString& code);
    static inline const QString ShareUrlBase = QStringLiteral("https://sudoku.gwynant.app/s/");

    // Generated puzzles waiting per level, so a new game starts instantly.
    static constexpr int BufferSize = 2;

signals:
    void selectedChanged();
    void multiSelectChanged();
    void inputModeChanged();
    void focusDigitChanged();
    void optionsChanged();
    // The puzzle a code named could not be generated; no game started.
    void codeFailed();

protected:
    void createGame(const QString& level) override;
    QVariantMap saveState() const override;
    bool restoreState(const QVariantMap& state) override;
    QVariant snapshot() const override;
    void applySnapshot(const QVariant& snapshot) override;

private:
    struct Board {
        sudoku::Grid puzzle {};
        sudoku::Grid solution {};
        sudoku::Grid entries {};
        std::array<sudoku::Mask, 81> corner {};
        std::array<sudoku::Mask, 81> centre {};
        sudoku::Grid colours {};
        sudoku::Technique hardest = sudoku::Technique::None;
        quint64 seed = 0;
        bool seedKnown = false; // false for boards opened from a share link
        int completions = 0;
    };

    bool canPlay() const { return active() && !complete(); }
    int digitAt(int cell) const;
    bool isGiven(int cell) const { return m_board.puzzle[cell] != 0; }
    bool clashes(int cell) const;
    bool isMistake(int cell, int mode) const;
    void place(int cell, int digit);
    void afterPlacement(const QList<int>& cells);
    void toggleMark(std::array<sudoku::Mask, 81>& marks, int digit);
    bool unitCorrect(int unit) const;
    void requestPuzzle(sudoku::Difficulty difficulty);
    void generate(sudoku::Difficulty difficulty, quint64 seed);
    void onPuzzleReady(sudoku::Difficulty difficulty, quint64 seed, const std::optional<sudoku::Puzzle>& puzzle);
    void startWith(const sudoku::Puzzle& puzzle);
    void startWith(const Board& board);
    void stopWaiting();
    void resetInput();

    bool option(const QString& key, bool fallback) const;
    void setOption(const QString& key, bool on);

    Board m_board;
    QList<int> m_selection;
    bool m_multiSelect = false;
    int m_inputMode = NumberMode;
    int m_focusDigit = 0;
    std::optional<sudoku::Difficulty> m_waitingFor;
    // The seed the player is waiting for (a typed code), if any.
    std::optional<quint64> m_waitingSeed;
    // Handed from openShared() / startFromCode() to createGame().
    std::optional<Board> m_pendingBoard;
    std::optional<quint64> m_pendingSeed;
    QHash<int, QList<sudoku::Puzzle>> m_buffer;
    QHash<int, int> m_inFlight;
#if CALM_SUDOKU_THREADED_GENERATION
    QList<QFutureWatcher<std::optional<sudoku::Puzzle>>*> m_watchers;
#else
    void generateNext();
    void scheduleGeneration();
    QList<std::pair<sudoku::Difficulty, quint64>> m_pending;
    QTimer* m_generationTimer = nullptr;
    bool m_warmedUp = false;
#endif
    mutable QSettings m_settings;
};
