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

// The Sudoku game session: board state, player input, notes, hints and
// game-specific options, built on the framework's GameSession.
class SudokuGame : public GameSession {
    Q_OBJECT
    QML_ELEMENT

    // 81 maps: { digit, given, notes (bitmask, bit d = digit d), mistake }
    Q_PROPERTY(QVariantList cells READ cells NOTIFY stateChanged)
    // Digits still to place, index 0 = digit 1.
    Q_PROPERTY(QVariantList remaining READ remaining NOTIFY stateChanged)
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(bool notesMode READ notesMode WRITE setNotesMode NOTIFY notesModeChanged)
    Q_PROPERTY(QString technique READ technique NOTIFY stateChanged)

    Q_PROPERTY(bool highlightPeers READ highlightPeers WRITE setHighlightPeers NOTIFY optionsChanged)
    Q_PROPERTY(bool highlightMatching READ highlightMatching WRITE setHighlightMatching NOTIFY optionsChanged)
    Q_PROPERTY(bool showMistakes READ showMistakes WRITE setShowMistakes NOTIFY optionsChanged)
    Q_PROPERTY(bool showTimer READ showTimer WRITE setShowTimer NOTIFY optionsChanged)
    Q_PROPERTY(bool autoRemoveNotes READ autoRemoveNotes WRITE setAutoRemoveNotes NOTIFY optionsChanged)

public:
    explicit SudokuGame(QObject* parent = nullptr);
    ~SudokuGame() override;

    QString gameId() const override { return QStringLiteral("sudoku"); }

    QVariantList cells() const;
    QVariantList remaining() const;
    int selected() const { return m_selected; }
    void setSelected(int cell);
    bool notesMode() const { return m_notesMode; }
    void setNotesMode(bool on);
    QString technique() const;

    bool highlightPeers() const { return option(QStringLiteral("highlightPeers"), true); }
    void setHighlightPeers(bool on) { setOption(QStringLiteral("highlightPeers"), on); }
    bool highlightMatching() const { return option(QStringLiteral("highlightMatching"), true); }
    void setHighlightMatching(bool on) { setOption(QStringLiteral("highlightMatching"), on); }
    bool showMistakes() const { return option(QStringLiteral("showMistakes"), false); }
    void setShowMistakes(bool on) { setOption(QStringLiteral("showMistakes"), on); }
    bool showTimer() const { return option(QStringLiteral("showTimer"), true); }
    void setShowTimer(bool on) { setOption(QStringLiteral("showTimer"), on); }
    bool autoRemoveNotes() const { return option(QStringLiteral("autoRemoveNotes"), true); }
    void setAutoRemoveNotes(bool on) { setOption(QStringLiteral("autoRemoveNotes"), on); }

    // Places a digit (or toggles a note in notes mode) in the selected cell.
    Q_INVOKABLE void enter(int digit);
    Q_INVOKABLE void erase();
    // Fills the selected cell, or the next logical cell if none is selected
    // or the selected one is filled. Returns false if no hint is available.
    Q_INVOKABLE bool hint();

    // Generated puzzles waiting per level, so a new game starts instantly.
    static constexpr int BufferSize = 2;

signals:
    void selectedChanged();
    void notesModeChanged();
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
        std::array<sudoku::Mask, 81> notes {};
        sudoku::Technique hardest = sudoku::Technique::None;
        quint64 seed = 0;
        int completions = 0;
    };

    int digitAt(int cell) const;
    bool isGiven(int cell) const { return m_board.puzzle[cell] != 0; }
    void place(int cell, int digit);
    void afterPlacement(int cell);
    bool unitCorrect(int unit) const;
    void requestPuzzle(sudoku::Difficulty difficulty);
    void onPuzzleReady(sudoku::Difficulty difficulty, const std::optional<sudoku::Puzzle>& puzzle);
    void startWith(const sudoku::Puzzle& puzzle);

    bool option(const QString& key, bool fallback) const;
    void setOption(const QString& key, bool on);

    Board m_board;
    int m_selected = -1;
    bool m_notesMode = false;
    std::optional<sudoku::Difficulty> m_waitingFor;
    QHash<int, QList<sudoku::Puzzle>> m_buffer;
    QHash<int, int> m_inFlight;
    QList<QFutureWatcher<std::optional<sudoku::Puzzle>>*> m_watchers;
    mutable QSettings m_settings;
};
