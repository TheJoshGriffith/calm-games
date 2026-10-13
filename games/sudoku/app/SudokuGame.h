#pragma once

#include <GameSession.h>

#include <sudoku/Generator.h>

#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QQmlEngine>
#include <QSettings>
#include <QVariantList>

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
    Q_PROPERTY(QString technique READ technique NOTIFY stateChanged)

    Q_PROPERTY(bool highlightPeers READ highlightPeers WRITE setHighlightPeers NOTIFY optionsChanged)
    Q_PROPERTY(bool highlightMatching READ highlightMatching WRITE setHighlightMatching NOTIFY optionsChanged)
    Q_PROPERTY(int mistakeMode READ mistakeMode WRITE setMistakeMode NOTIFY optionsChanged)
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
    QString technique() const;

    bool highlightPeers() const { return option(QStringLiteral("highlightPeers"), true); }
    void setHighlightPeers(bool on) { setOption(QStringLiteral("highlightPeers"), on); }
    bool highlightMatching() const { return option(QStringLiteral("highlightMatching"), true); }
    void setHighlightMatching(bool on) { setOption(QStringLiteral("highlightMatching"), on); }
    int mistakeMode() const;
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
    Q_INVOKABLE void enter(int digit);
    // Colour 1-9 paints the selection (or clears it if every selected cell
    // already has that colour); 0 clears colours.
    Q_INVOKABLE void paint(int colour);
    // Clears entries in the selection; if none, marks; if none, colours.
    Q_INVOKABLE void erase();
    // Fills the selected cell, or the next logical cell if none is selected
    // or the selected one is filled. Returns false if no hint is available.
    Q_INVOKABLE bool hint();

    // Generated puzzles waiting per level, so a new game starts instantly.
    static constexpr int BufferSize = 2;

signals:
    void selectedChanged();
    void multiSelectChanged();
    void inputModeChanged();
    void optionsChanged();

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
    void onPuzzleReady(sudoku::Difficulty difficulty, const std::optional<sudoku::Puzzle>& puzzle);
    void startWith(const sudoku::Puzzle& puzzle);
    void resetInput();

    bool option(const QString& key, bool fallback) const;
    void setOption(const QString& key, bool on);

    Board m_board;
    QList<int> m_selection;
    bool m_multiSelect = false;
    int m_inputMode = NumberMode;
    std::optional<sudoku::Difficulty> m_waitingFor;
    QHash<int, QList<sudoku::Puzzle>> m_buffer;
    QHash<int, int> m_inFlight;
    QList<QFutureWatcher<std::optional<sudoku::Puzzle>>*> m_watchers;
    mutable QSettings m_settings;
};
