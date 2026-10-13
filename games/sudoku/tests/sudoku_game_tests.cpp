// Qt-side tests: the Sudoku session on top of the framework's GameSession.

#include "SaveStore.h"
#include "SudokuGame.h"
#include "SaveStore.h"
#include "Stats.h"

#include <sudoku/Board.h>
#include <sudoku/Generator.h>
#include <sudoku/Grader.h>
#include <sudoku/Share.h>

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class SudokuGameTests : public QObject {
    Q_OBJECT

private:
    static QVariantMap cellAt(const SudokuGame& game, int cell) { return game.cells()[cell].toMap(); }
    static int digit(const SudokuGame& game, int cell) { return cellAt(game, cell).value(QStringLiteral("digit")).toInt(); }
    static int corner(const SudokuGame& game, int cell) { return cellAt(game, cell).value(QStringLiteral("corner")).toInt(); }
    static int centre(const SudokuGame& game, int cell) { return cellAt(game, cell).value(QStringLiteral("centre")).toInt(); }
    static int colour(const SudokuGame& game, int cell) { return cellAt(game, cell).value(QStringLiteral("colour")).toInt(); }
    static bool mistake(const SudokuGame& game, int cell) { return cellAt(game, cell).value(QStringLiteral("mistake")).toBool(); }
    static bool given(const SudokuGame& game, int cell) { return cellAt(game, cell).value(QStringLiteral("given")).toBool(); }

    static QList<int> emptyCells(const SudokuGame& game)
    {
        QList<int> out;
        for (int i = 0; i < 81; ++i)
            if (digit(game, i) == 0)
                out.append(i);
        return out;
    }

    static int firstEmpty(const SudokuGame& game)
    {
        const QList<int> empty = emptyCells(game);
        return empty.isEmpty() ? -1 : empty.first();
    }

    static int firstGiven(const SudokuGame& game)
    {
        for (int i = 0; i < 81; ++i)
            if (given(game, i))
                return i;
        return -1;
    }

    // Two empty cells that see each other.
    static std::pair<int, int> emptyPeers(const SudokuGame& game)
    {
        const QList<int> empty = emptyCells(game);
        for (int a : empty)
            for (int b : empty)
                if (a != b && sudoku::arePeers(a, b))
                    return { a, b };
        return { -1, -1 };
    }

    // Two empty cells that don't see each other.
    static std::pair<int, int> emptyStrangers(const SudokuGame& game)
    {
        const QList<int> empty = emptyCells(game);
        for (int a : empty)
            for (int b : empty)
                if (a != b && !sudoku::arePeers(a, b))
                    return { a, b };
        return { -1, -1 };
    }

    static void select(SudokuGame& game, const QList<int>& cells)
    {
        game.clearSelection();
        for (int c : cells)
            game.addToSelection(c);
    }

    static void startAndWait(SudokuGame& game, const QString& level)
    {
        game.startNewGame(level);
        QTRY_VERIFY_WITH_TIMEOUT(game.active() && !game.loading(), 20000);
    }

    static sudoku::Grid givens(const SudokuGame& game)
    {
        sudoku::Grid out {};
        for (int i = 0; i < 81; ++i)
            if (given(game, i))
                out[i] = static_cast<std::uint8_t>(digit(game, i));
        return out;
    }

    // A share payload for a fixed Hard puzzle, independent of any game.
    static QString hardPayload()
    {
        const auto puzzle = sudoku::generate(sudoku::Difficulty::Hard, 4);
        sudoku::SharedBoard board;
        board.givens = puzzle->puzzle;
        board.level = sudoku::Difficulty::Hard;
        return QString::fromStdString(sudoku::encodeBoard(board));
    }

    // Player digits (one wrong), corner and centre marks and colours (one on
    // a given), so a board has every kind of progress to share.
    static void playSome(SudokuGame& game)
    {
        const auto [a, b] = emptyStrangers(game);
        QList<int> others;
        for (int e : emptyCells(game))
            if (e != a && e != b)
                others.append(e);
        game.selectOnly(others.at(0));
        game.enter(4);
        game.hint();
        game.setInputMode(SudokuGame::CornerMode);
        select(game, { a, b });
        game.enter(1);
        game.enter(9);
        game.setInputMode(SudokuGame::CentreMode);
        game.enter(2);
        game.paint(5);
        select(game, { firstGiven(game), b });
        game.paint(8);
        game.setInputMode(SudokuGame::NumberMode);
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("CalmGamesTest"));
        QCoreApplication::setApplicationName(QStringLiteral("SudokuTest"));
    }

    void init()
    {
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
        QSettings().clear();
    }

    void newGameHasPuzzle()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        QCOMPARE(game.level(), QStringLiteral("Easy"));
        int givens = 0;
        for (const QVariant& c : game.cells())
            givens += c.toMap().value(QStringLiteral("given")).toBool();
        QVERIFY(givens >= 36);
        QVERIFY(game.hasSavedGame());
    }

    void enterUndoAndNotes()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Medium"));
        const int cell = firstEmpty(game);
        QVERIFY(cell >= 0);
        game.setSelected(cell);

        QSignalSpy feedback(&game, &GameSession::feedback);
        game.enter(5);
        QCOMPARE(digit(game, cell), 5);
        QVERIFY(game.canUndo());
        QCOMPARE(feedback.count(), 1);

        game.undo();
        QCOMPARE(digit(game, cell), 0);
        QVERIFY(game.canRedo());

        game.setInputMode(SudokuGame::CentreMode);
        game.enter(3);
        game.enter(7);
        QCOMPARE(centre(game, cell), (1 << 3) | (1 << 7));
        game.enter(3);
        QCOMPARE(centre(game, cell), 1 << 7);

        game.erase();
        QCOMPARE(centre(game, cell), 0);
    }

    void selectionEditing()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        QCOMPARE(game.selected(), -1);
        QVERIFY(game.selection().isEmpty());

        game.setSelected(10);
        game.addToSelection(20);
        game.addToSelection(30);
        QCOMPARE(game.selection(), (QVariantList { 10, 20, 30 }));
        QCOMPARE(game.selected(), 30);

        // Re-adding moves a cell to the end, making it primary.
        game.addToSelection(10);
        QCOMPARE(game.selection(), (QVariantList { 20, 30, 10 }));
        QCOMPARE(game.selected(), 10);

        game.removeFromSelection(10);
        QCOMPARE(game.selected(), 30);
        QVERIFY(game.isSelected(20) && !game.isSelected(10));

        game.selectOnly(5);
        QCOMPARE(game.selection(), (QVariantList { 5 }));
        game.clearSelection();
        QCOMPARE(game.selected(), -1);
        QVERIFY(game.selection().isEmpty());
    }

    void digitFocusWithNothingSelected()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        QSignalSpy moves(&game, &SudokuGame::stateChanged);

        // Nothing selected: a digit focuses instead of placing, and pressing
        // it again (or another digit) toggles or moves the focus.
        game.enter(4);
        QCOMPARE(game.focusDigit(), 4);
        QCOMPARE(moves.count(), 0);
        QVERIFY(!game.canUndo());
        game.enter(7);
        QCOMPARE(game.focusDigit(), 7);
        game.enter(7);
        QCOMPARE(game.focusDigit(), 0);

        // Mark modes focus too; Colour mode doesn't.
        game.setInputMode(SudokuGame::CornerMode);
        game.enter(2);
        QCOMPARE(game.focusDigit(), 2);
        game.setInputMode(SudokuGame::ColourMode);
        game.enter(5);
        QCOMPARE(game.focusDigit(), 2);
        game.setInputMode(SudokuGame::NumberMode);

        // Selecting a cell clears the focus; the digit then places as usual.
        const int cell = emptyCells(game).first();
        game.selectOnly(cell);
        QCOMPARE(game.focusDigit(), 0);
        game.enter(3);
        QCOMPARE(game.cells()[cell].toMap()[QStringLiteral("digit")].toInt(), 3);

        game.clearSelection();
        game.enter(6);
        game.addToSelection(cell);
        QCOMPARE(game.focusDigit(), 0);

        // A new game starts unfocused.
        game.clearSelection();
        game.enter(6);
        startAndWait(game, QStringLiteral("Easy"));
        QCOMPARE(game.focusDigit(), 0);
    }

    void multiCellEntryIsOneStep()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Medium"));
        const QList<int> empty = emptyCells(game);
        const auto [a, b] = emptyStrangers(game);
        QVERIFY(a >= 0);
        int filled = -1;
        for (int c : empty)
            if (c != a && c != b && !sudoku::arePeers(c, a) && !sudoku::arePeers(c, b)) {
                filled = c;
                break;
            }
        QVERIFY(filled >= 0);
        game.selectOnly(filled);
        game.enter(2);
        QCOMPARE(digit(game, filled), 2);

        const int givenCell = firstGiven(game);
        const int givenDigit = digit(game, givenCell);
        select(game, { a, b, filled, givenCell });
        QSignalSpy feedback(&game, &GameSession::feedback);
        QSignalSpy history(&game, &GameSession::historyChanged);
        game.enter(7);
        QCOMPARE(digit(game, a), 7);
        QCOMPARE(digit(game, b), 7);
        QCOMPARE(digit(game, filled), 2); // a multi-cell entry only fills empty cells
        QCOMPARE(digit(game, givenCell), givenDigit);
        QCOMPARE(feedback.count(), 1);
        QCOMPARE(history.count(), 1);

        game.undo();
        QCOMPARE(digit(game, a), 0);
        QCOMPARE(digit(game, b), 0);
        QCOMPARE(digit(game, filled), 2);

        // A single selected cell can still be overwritten.
        game.selectOnly(filled);
        game.enter(3);
        QCOMPARE(digit(game, filled), 3);
    }

    void markToggleRule()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Medium"));
        const auto [a, b] = emptyStrangers(game);
        game.setInputMode(SudokuGame::CornerMode);

        game.selectOnly(a);
        game.enter(3);
        QCOMPARE(corner(game, a), 1 << 3);

        // Not every cell has it: add to all, in one undo step.
        select(game, { a, b });
        QSignalSpy feedback(&game, &GameSession::feedback);
        QSignalSpy history(&game, &GameSession::historyChanged);
        game.enter(3);
        QCOMPARE(corner(game, a), 1 << 3);
        QCOMPARE(corner(game, b), 1 << 3);
        QCOMPARE(feedback.count(), 1);
        QCOMPARE(history.count(), 1);

        // Every cell has it: remove from all.
        game.enter(3);
        QCOMPARE(corner(game, a), 0);
        QCOMPARE(corner(game, b), 0);

        game.undo();
        QCOMPARE(corner(game, a), 1 << 3);
        QCOMPARE(corner(game, b), 1 << 3);

        // Givens and filled cells are skipped and don't block the rule.
        game.selectOnly(b);
        game.setInputMode(SudokuGame::NumberMode);
        game.enter(9);
        game.setInputMode(SudokuGame::CentreMode);
        select(game, { a, b, firstGiven(game) });
        game.enter(4);
        QCOMPARE(centre(game, a), 1 << 4);
        game.enter(4);
        QCOMPARE(centre(game, a), 0);
    }

    void cornerAndCentreAreSeparate()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Medium"));
        const int cell = firstEmpty(game);
        game.selectOnly(cell);

        game.setInputMode(SudokuGame::CornerMode);
        game.enter(1);
        game.enter(2);
        game.setInputMode(SudokuGame::CentreMode);
        game.enter(2);
        game.enter(6);
        QCOMPARE(corner(game, cell), (1 << 1) | (1 << 2));
        QCOMPARE(centre(game, cell), (1 << 2) | (1 << 6));

        game.enter(2);
        QCOMPARE(corner(game, cell), (1 << 1) | (1 << 2));
        QCOMPARE(centre(game, cell), 1 << 6);

        game.undo();
        game.undo();
        QCOMPARE(centre(game, cell), 1 << 2);
        QCOMPARE(corner(game, cell), (1 << 1) | (1 << 2));
    }

    void autoRemoveClearsBothKinds()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Medium"));
        QVERIFY(game.autoRemoveNotes());
        const auto [a, peer] = emptyPeers(game);
        QVERIFY(a >= 0);
        int stranger = -1;
        for (int c : emptyCells(game))
            if (c != a && c != peer && !sudoku::arePeers(c, a)) {
                stranger = c;
                break;
            }
        QVERIFY(stranger >= 0);

        select(game, { peer, stranger });
        game.setInputMode(SudokuGame::CornerMode);
        game.enter(5);
        game.enter(6);
        game.setInputMode(SudokuGame::CentreMode);
        game.enter(5);
        game.enter(8);

        game.selectOnly(a);
        game.setInputMode(SudokuGame::NumberMode);
        game.enter(5);
        QCOMPARE(corner(game, peer), 1 << 6);
        QCOMPARE(centre(game, peer), 1 << 8);
        QCOMPARE(corner(game, stranger), (1 << 5) | (1 << 6));
        QCOMPARE(centre(game, stranger), (1 << 5) | (1 << 8));

        // Undo brings the removed marks back with the digit.
        game.undo();
        QCOMPARE(corner(game, peer), (1 << 5) | (1 << 6));
        QCOMPARE(centre(game, peer), (1 << 5) | (1 << 8));

        game.setAutoRemoveNotes(false);
        game.enter(5);
        QCOMPARE(corner(game, peer), (1 << 5) | (1 << 6));
        QCOMPARE(centre(game, peer), (1 << 5) | (1 << 8));
    }

    void coloursSetAndClear()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        const int a = firstEmpty(game);
        const int g = firstGiven(game);

        game.selectOnly(a);
        game.paint(4);
        QCOMPARE(colour(game, a), 4);

        // Not all have colour 4: set it on all (givens too), one step.
        select(game, { a, g });
        QSignalSpy history(&game, &GameSession::historyChanged);
        QSignalSpy feedback(&game, &GameSession::feedback);
        game.paint(4);
        QCOMPARE(colour(game, a), 4);
        QCOMPARE(colour(game, g), 4);
        QCOMPARE(history.count(), 1);
        QCOMPARE(feedback.count(), 1);

        // All have it: tapping it again clears.
        game.paint(4);
        QCOMPARE(colour(game, a), 0);
        QCOMPARE(colour(game, g), 0);

        // A different colour replaces; one colour per cell.
        game.paint(2);
        game.paint(7);
        QCOMPARE(colour(game, a), 7);
        QCOMPARE(colour(game, g), 7);

        // In Colour mode the pad's keys paint.
        game.setInputMode(SudokuGame::ColourMode);
        game.enter(3);
        QCOMPARE(colour(game, a), 3);
        QCOMPARE(digit(game, a), 0);

        // The clear key.
        game.paint(0);
        QCOMPARE(colour(game, a), 0);
        QCOMPARE(colour(game, g), 0);
        game.undo();
        QCOMPARE(colour(game, a), 3);
    }

    void eraseOrder()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Medium"));
        const QList<int> empty = emptyCells(game);
        // Three cells that don't see each other, so no auto-removal interferes.
        QList<int> cells;
        for (int c : empty) {
            bool ok = true;
            for (int o : std::as_const(cells))
                ok = ok && !sudoku::arePeers(c, o);
            if (ok)
                cells.append(c);
            if (cells.size() == 3)
                break;
        }
        QCOMPARE(cells.size(), 3);
        const int entryCell = cells[0], markCell = cells[1], colourCell = cells[2];

        game.selectOnly(entryCell);
        game.enter(4);
        game.selectOnly(markCell);
        game.setInputMode(SudokuGame::CornerMode);
        game.enter(1);
        game.setInputMode(SudokuGame::CentreMode);
        game.enter(2);
        game.selectOnly(colourCell);
        game.paint(6);

        select(game, cells);
        QSignalSpy history(&game, &GameSession::historyChanged);
        game.erase();
        QCOMPARE(digit(game, entryCell), 0);
        QCOMPARE(corner(game, markCell), 1 << 1);
        QCOMPARE(centre(game, markCell), 1 << 2);
        QCOMPARE(colour(game, colourCell), 6);

        game.erase();
        QCOMPARE(corner(game, markCell), 0);
        QCOMPARE(centre(game, markCell), 0);
        QCOMPARE(colour(game, colourCell), 6);

        game.erase();
        QCOMPARE(colour(game, colourCell), 0);
        QCOMPARE(history.count(), 3);

        // Nothing left: no move.
        game.erase();
        QCOMPARE(history.count(), 3);

        // Each erase was one step.
        game.undo();
        QCOMPARE(colour(game, colourCell), 6);
        QCOMPARE(corner(game, markCell), 0);
        game.undo();
        QCOMPARE(corner(game, markCell), 1 << 1);
        QCOMPARE(digit(game, entryCell), 0);
        game.undo();
        QCOMPARE(digit(game, entryCell), 4);
    }

    void clashDetection()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        QCOMPARE(game.mistakeMode(), int(SudokuGame::MistakesClashes));

        // An empty cell and a given it sees.
        int cell = -1, givenPeer = -1;
        for (int c : emptyCells(game)) {
            for (int p : sudoku::peers()[c])
                if (given(game, p)) {
                    givenPeer = p;
                    break;
                }
            if (givenPeer >= 0) {
                cell = c;
                break;
            }
        }
        QVERIFY(cell >= 0);
        const int d = digit(game, givenPeer);

        game.selectOnly(cell);
        QSignalSpy feedback(&game, &GameSession::feedback);
        game.enter(d);
        QCOMPARE(feedback.last().at(0).toString(), QStringLiteral("error"));
        QVERIFY(mistake(game, cell));
        QVERIFY(mistake(game, givenPeer));
        int flagged = 0;
        for (int i = 0; i < 81; ++i)
            flagged += mistake(game, i);
        int sameDigitPeers = 0;
        for (int p : sudoku::peers()[cell])
            sameDigitPeers += digit(game, p) == d;
        QCOMPARE(flagged, 1 + sameDigitPeers);

        QSignalSpy state(&game, &GameSession::stateChanged);
        game.setMistakeMode(SudokuGame::MistakesAnswer);
        QVERIFY(state.count() > 0);
        QVERIFY(mistake(game, cell)); // a clash can never be the answer
        QVERIFY(!mistake(game, givenPeer));

        game.setMistakeMode(SudokuGame::MistakesOff);
        QVERIFY(!mistake(game, cell));
        QVERIFY(!mistake(game, givenPeer));
    }

    void givensCannotChange()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        const int g = firstGiven(game);
        game.setSelected(g);
        const int before = digit(game, g);
        game.enter(before == 9 ? 1 : before + 1);
        game.erase();
        game.setInputMode(SudokuGame::CornerMode);
        game.enter(1);
        QCOMPARE(digit(game, g), before);
        QCOMPARE(corner(game, g), 0);
        QVERIFY(!game.canUndo());
    }

    void hintsSolveTheGame()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Hard"));
        QSignalSpy won(&game, &GameSession::won);
        QSignalSpy feedback(&game, &GameSession::feedback);

        int guard = 0;
        while (!game.complete() && guard++ < 81)
            QVERIFY(game.hint());

        QVERIFY(game.complete());
        QCOMPARE(won.count(), 1);
        QCOMPARE(feedback.last().at(0).toString(), QStringLiteral("win"));
        bool chimed = false;
        for (const auto& args : feedback)
            chimed |= args.at(0).toString() == QLatin1String("complete");
        QVERIFY(chimed);
        QVERIFY(!game.hasSavedGame());

        const QVariantMap stats = Stats::forLevel(QStringLiteral("sudoku"), QStringLiteral("Hard"));
        QCOMPARE(stats.value(QStringLiteral("played")).toInt(), 1);
        QCOMPARE(stats.value(QStringLiteral("won")).toInt(), 1);
        QCOMPARE(stats.value(QStringLiteral("streak")).toInt(), 1);
        QVERIFY(game.hintsUsed() > 0);
        QCOMPARE(stats.value(QStringLiteral("wonNoHints")).toInt(), 0);
        QCOMPARE(stats.value(QStringLiteral("winRate")).toDouble(), 1.0);
    }

    void hintsUsedResetsAndPersists()
    {
        {
            SudokuGame game;
            startAndWait(game, QStringLiteral("Easy"));
            QCOMPARE(game.hintsUsed(), 0);
            QSignalSpy changed(&game, &GameSession::hintsUsedChanged);
            game.setSelected(-1);
            QVERIFY(game.hint());
            QVERIFY(game.hint());
            QCOMPARE(game.hintsUsed(), 2);
            QCOMPARE(changed.count(), 2);

            // An ordinary move is not a hint.
            const int cell = firstEmpty(game);
            game.setSelected(cell);
            game.enter(5);
            QCOMPARE(game.hintsUsed(), 2);
        }
        {
            SudokuGame restored;
            QCOMPARE(restored.hintsUsed(), 0);
            QVERIFY(restored.resume());
            QCOMPARE(restored.hintsUsed(), 2);

            startAndWait(restored, QStringLiteral("Easy"));
            QCOMPARE(restored.hintsUsed(), 0);
        }
    }

    void hintFreeWinsAndWinRate()
    {
        const QString id = QStringLiteral("stats-test");
        const QString level = QStringLiteral("Easy");

        QVariantMap s = Stats::forLevel(id, level);
        QCOMPARE(s.value(QStringLiteral("played")).toInt(), 0);
        QCOMPARE(s.value(QStringLiteral("wonNoHints")).toInt(), 0);
        QCOMPARE(s.value(QStringLiteral("winRate")).toDouble(), 0.0);

        // Four games: two clean wins, one win with hints, one abandoned.
        Stats::recordStarted(id, level);
        Stats::recordWon(id, level, 100, false);
        Stats::recordStarted(id, level);
        Stats::recordWon(id, level, 200, true);
        Stats::recordStarted(id, level);
        Stats::recordWon(id, level, 300);
        Stats::recordStarted(id, level);

        s = Stats::forLevel(id, level);
        QCOMPARE(s.value(QStringLiteral("played")).toInt(), 4);
        QCOMPARE(s.value(QStringLiteral("won")).toInt(), 3);
        QCOMPARE(s.value(QStringLiteral("wonNoHints")).toInt(), 2);
        QCOMPARE(s.value(QStringLiteral("winRate")).toDouble(), 0.75);
        QCOMPARE(s.value(QStringLiteral("averageSeconds")).toInt(), 200);

        // Other levels are untouched.
        QCOMPARE(Stats::forLevel(id, QStringLiteral("Hard")).value(QStringLiteral("winRate")).toDouble(), 0.0);

        Stats::reset(id);
        QCOMPARE(Stats::forLevel(id, level).value(QStringLiteral("played")).toInt(), 0);
    }

    void statsFromBeforeHintTrackingStillLoad()
    {
        // The pre-hint format: no wonNoHints key at all.
        QVariantMap old;
        old.insert(QStringLiteral("played"), 5);
        old.insert(QStringLiteral("won"), 4);
        old.insert(QStringLiteral("totalSeconds"), 400);
        old.insert(QStringLiteral("bestSeconds"), 60);
        QVariantMap all;
        all.insert(QStringLiteral("Medium"), old);
        QVERIFY(SaveStore::save(QStringLiteral("stats-sudoku"), all));

        QVariantMap s = Stats::forLevel(QStringLiteral("sudoku"), QStringLiteral("Medium"));
        QCOMPARE(s.value(QStringLiteral("won")).toInt(), 4);
        QCOMPARE(s.value(QStringLiteral("wonNoHints")).toInt(), 0);
        QCOMPARE(s.value(QStringLiteral("winRate")).toDouble(), 0.8);
        QCOMPARE(s.value(QStringLiteral("averageSeconds")).toInt(), 100);

        Stats::recordStarted(QStringLiteral("sudoku"), QStringLiteral("Medium"));
        Stats::recordWon(QStringLiteral("sudoku"), QStringLiteral("Medium"), 50, false);
        s = Stats::forLevel(QStringLiteral("sudoku"), QStringLiteral("Medium"));
        QCOMPARE(s.value(QStringLiteral("won")).toInt(), 5);
        QCOMPARE(s.value(QStringLiteral("wonNoHints")).toInt(), 1);
        QCOMPARE(s.value(QStringLiteral("bestSeconds")).toInt(), 50);
    }

    void resumeRestoresBoard()
    {
        QVariantList saved;
        QVariantList selection;
        {
            SudokuGame game;
            startAndWait(game, QStringLiteral("Expert"));
            const auto [a, b] = emptyStrangers(game);
            int c = -1;
            for (int e : emptyCells(game))
                if (e != a && e != b) {
                    c = e;
                    break;
                }
            game.selectOnly(c);
            game.enter(4);
            game.setInputMode(SudokuGame::CornerMode);
            select(game, { a, b });
            game.enter(1);
            game.enter(9);
            game.setInputMode(SudokuGame::CentreMode);
            game.enter(2);
            game.paint(5);
            select(game, { firstGiven(game), b, a });
            game.paint(8);
            saved = game.cells();
            selection = game.selection();
        }
        SudokuGame restored;
        QVERIFY(restored.hasSavedGame());
        QVERIFY(restored.resume());
        QCOMPARE(restored.level(), QStringLiteral("Expert"));
        QCOMPARE(restored.cells(), saved);
        QCOMPARE(restored.selection(), selection);
        QVERIFY(restored.active());
    }

    void oldNotesBecomeCentreMarks()
    {
        int cell = -1;
        {
            SudokuGame game;
            startAndWait(game, QStringLiteral("Medium"));
            cell = firstEmpty(game);
        }
        // Rewrite the autosave the way the previous version wrote it.
        QVariantMap saved = SaveStore::load(QStringLiteral("sudoku"));
        QVariantMap state = saved.value(QStringLiteral("game")).toMap();
        QVERIFY(state.contains(QStringLiteral("centre")));
        for (const char* key : { "corner", "centre", "colours", "selection" })
            state.remove(QLatin1String(key));
        QVariantList notes;
        for (int i = 0; i < 81; ++i)
            notes.append(i == cell ? (1 << 2) | (1 << 7) : 0);
        state.insert(QStringLiteral("notes"), notes);
        state.insert(QStringLiteral("selected"), cell);
        saved.insert(QStringLiteral("game"), state);
        QVERIFY(SaveStore::save(QStringLiteral("sudoku"), saved));

        SudokuGame restored;
        QVERIFY(restored.resume());
        QCOMPARE(centre(restored, cell), (1 << 2) | (1 << 7));
        QCOMPARE(corner(restored, cell), 0);
        for (int i = 0; i < 81; ++i)
            QCOMPARE(colour(restored, i), 0);
        QCOMPARE(restored.selection(), QVariantList { cell });
    }

    void mistakeOptionMigrates()
    {
        QSettings settings;
        {
            SudokuGame game;
            QCOMPARE(game.mistakeMode(), int(SudokuGame::MistakesClashes));
        }
        settings.setValue(QStringLiteral("sudoku/showMistakes"), true);
        {
            SudokuGame game;
            QCOMPARE(game.mistakeMode(), int(SudokuGame::MistakesAnswer));
        }
        settings.setValue(QStringLiteral("sudoku/showMistakes"), false);
        {
            SudokuGame game;
            QCOMPARE(game.mistakeMode(), int(SudokuGame::MistakesOff));
            // A new choice wins over the old option.
            QSignalSpy options(&game, &SudokuGame::optionsChanged);
            game.setMistakeMode(SudokuGame::MistakesClashes);
            QCOMPARE(options.count(), 1);
        }
        {
            SudokuGame game;
            QCOMPARE(game.mistakeMode(), int(SudokuGame::MistakesClashes));
        }
    }

    void newPuzzlesHaveShortCodes()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Hard"));
        const QString code = game.puzzleCode();
        QCOMPARE(code.size(), 11); // "H1-" and an 8-character seed
        QVERIFY(code.startsWith(QStringLiteral("H1-")));
        const auto parsed = sudoku::parsePuzzleCode(code.toStdString());
        QVERIFY(parsed);
        QVERIFY(parsed->first == sudoku::Difficulty::Hard);
        QVERIFY(parsed->second <= sudoku::PuzzleSeedMask);
        // The code really names this puzzle.
        const auto puzzle = sudoku::generate(parsed->first, parsed->second);
        QVERIFY(puzzle);
        QVERIFY(puzzle->puzzle == givens(game));
    }

    void codeOpensTheSamePuzzle()
    {
        QString code;
        sudoku::Grid start {};
        {
            SudokuGame first;
            startAndWait(first, QStringLiteral("Medium"));
            code = first.puzzleCode();
            start = givens(first);
        }
        for (int run = 0; run < 2; ++run) {
            SudokuGame game;
            startAndWait(game, QStringLiteral("Easy"));
            // Typed loosely: lower case, with spaces around it.
            QVERIFY(game.startFromCode(QStringLiteral("  ") + code.toLower() + QStringLiteral(" ")));
            QTRY_VERIFY_WITH_TIMEOUT(game.active() && !game.loading(), 20000);
            QCOMPARE(game.level(), QStringLiteral("Medium"));
            QVERIFY(givens(game) == start);
            QCOMPARE(game.puzzleCode(), code);
            QCOMPARE(game.hintsUsed(), 0);
            QVERIFY(!game.canUndo());
        }
        // The puzzle code survives a resume.
        SudokuGame resumed;
        QVERIFY(resumed.resume());
        QCOMPARE(resumed.puzzleCode(), code);
    }

    void badCodesChangeNothing()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        const QVariantList before = game.cells();
        const QString code = game.puzzleCode();
        for (const char* bad : { "", "   ", "hello", "H1-", "H1-0000000", "Q1-00000000", "H2-00000000", "H1-0000000U",
                 "https://sudoku.gwynant.app/s/1AAA" })
            QVERIFY2(!game.startFromCode(QString::fromLatin1(bad)), bad);
        QVERIFY(game.active());
        QVERIFY(!game.loading());
        QCOMPARE(game.cells(), before);
        QCOMPARE(game.level(), QStringLiteral("Easy"));
        QCOMPARE(game.puzzleCode(), code);
    }

    void shareStartingBoard()
    {
        SudokuGame from;
        startAndWait(from, QStringLiteral("Hard"));
        playSome(from);
        const QString url = from.shareUrl(false);
        QVERIFY(url.startsWith(QStringLiteral("https://sudoku.gwynant.app/s/1")));
        QVERIFY(url.size() < SudokuGame::ShareUrlBase.size() + 40);

        SudokuGame to;
        QVERIFY(to.openShared(url));
        QVERIFY(to.active());
        QVERIFY(!to.loading());
        QCOMPARE(to.level(), QStringLiteral("Hard"));
        QVERIFY(givens(to) == givens(from));
        for (int i = 0; i < 81; ++i) {
            if (!given(to, i)) {
                QCOMPARE(digit(to, i), 0);
                QCOMPARE(corner(to, i), 0);
                QCOMPARE(centre(to, i), 0);
            }
            QCOMPARE(colour(to, i), 0);
        }
        // A shared board has no seed, so no code; it can be shared on.
        QCOMPARE(to.puzzleCode(), QString());
        QCOMPARE(to.shareUrl(false), url);
    }

    void shareProgress()
    {
        SudokuGame from;
        startAndWait(from, QStringLiteral("Expert"));
        playSome(from);
        const QString url = from.shareUrl(true);
        QVERIFY(url != from.shareUrl(false));

        SudokuGame to;
        QVERIFY(to.openShared(url));
        QCOMPARE(to.level(), QStringLiteral("Expert"));
        QCOMPARE(to.cells(), from.cells());
        QCOMPARE(to.hintsUsed(), 0);
        QVERIFY(!to.canUndo());
        QCOMPARE(to.shareUrl(true), url);

        // The bare payload, and links as messaging apps mangle them.
        const QString payload = url.mid(SudokuGame::ShareUrlBase.size());
        for (const QString& link : { payload, QStringLiteral("  ") + url + QStringLiteral("\n"),
                 url + QStringLiteral("/"), url + QStringLiteral("?utm_source=chat"),
                 QStringLiteral("https://share-links.gwynant-sudoku.pages.dev/s/") + payload }) {
            SudokuGame again;
            QVERIFY2(again.openShared(link.trimmed()), qPrintable(link));
            QCOMPARE(again.cells(), from.cells());
        }

        // Opened boards are autosaved like any game.
        SudokuGame resumed;
        QVERIFY(resumed.resume());
        QCOMPARE(resumed.cells(), from.cells());
        QCOMPARE(resumed.level(), QStringLiteral("Expert"));
        QCOMPARE(resumed.puzzleCode(), QString());
    }

    void sharedBoardWithoutLevelIsGraded()
    {
        const auto puzzle = sudoku::generate(sudoku::Difficulty::Medium, 11);
        sudoku::SharedBoard board;
        board.givens = puzzle->puzzle;
        SudokuGame game;
        QVERIFY(game.openShared(QString::fromStdString(sudoku::encodeBoard(board))));
        QCOMPARE(game.level(), QStringLiteral("Medium"));
    }

    void badLinksChangeNothing()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        game.selectOnly(firstEmpty(game));
        game.enter(3);
        const QVariantList before = game.cells();

        const QString good = hardPayload();
        // Consistent givens with many solutions.
        sudoku::SharedBoard loose;
        loose.givens[0] = 1;
        loose.givens[40] = 5;
        // Every given but one wrong digit in the middle of the solution: still
        // consistent, but with no solution at all.
        const auto solved = sudoku::generate(sudoku::Difficulty::Easy, 2);
        sudoku::SharedBoard none;
        none.givens = solved->solution;
        for (int i : { 0, 1, 9, 10 })
            none.givens[i] = 0;
        std::swap(none.givens[2], none.givens[11]);
        if (!sudoku::isConsistent(none.givens))
            none.givens = loose.givens;

        const QStringList bad = {
            QString(),
            QStringLiteral("   "),
            QStringLiteral("not a payload"),
            QStringLiteral("2") + good.mid(1), // unknown format version
            good.chopped(3),
            good + QStringLiteral("A"),
            QStringLiteral("https://sudoku.gwynant.app/"),
            QStringLiteral("https://sudoku.gwynant.app/s/"),
            QStringLiteral("https://sudoku.gwynant.app/x/") + good,
            QStringLiteral("https://sudoku.gwynant.app/s/") + good + QStringLiteral("/more"),
            QStringLiteral("H1-00000000"),
            QString::fromStdString(sudoku::encodeBoard(loose)),
            QString::fromStdString(sudoku::encodeBoard(none)),
        };
        for (const QString& link : bad)
            QVERIFY2(!game.openShared(link), qPrintable(link));
        QVERIFY(game.active());
        QCOMPARE(game.level(), QStringLiteral("Easy"));
        QCOMPARE(game.cells(), before);
        QVERIFY(game.canUndo());
    }

    void openSharedReplacesTheGame()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        game.hint();
        QCOMPARE(game.hintsUsed(), 1);
        QSignalSpy started(&game, &GameSession::stateChanged);
        QVERIFY(game.openShared(hardPayload()));
        QCOMPARE(game.level(), QStringLiteral("Hard"));
        QCOMPARE(game.hintsUsed(), 0);
        QCOMPARE(game.elapsedSeconds(), 0);
        QVERIFY(!game.canUndo());
        QVERIFY(started.count() > 0);
        const auto easy = Stats::forLevel(QStringLiteral("sudoku"), QStringLiteral("Easy"));
        const auto hard = Stats::forLevel(QStringLiteral("sudoku"), QStringLiteral("Hard"));
        QCOMPARE(easy.value(QStringLiteral("played")).toInt(), 1);
        QCOMPARE(hard.value(QStringLiteral("played")).toInt(), 1);
    }

    void openSharedWhileAPuzzleLoads()
    {
        SudokuGame game;
        game.startNewGame(QStringLiteral("Expert")); // the buffers are still empty
        QVERIFY(game.loading());
        QVERIFY(game.openShared(hardPayload()));
        QVERIFY(!game.loading());
        QVERIFY(game.active());
        QCOMPARE(game.level(), QStringLiteral("Hard"));
        const sudoku::Grid shared = givens(game);
        // The Expert puzzle that finishes later goes to the buffer instead.
        QTest::qWait(1500);
        QCOMPARE(game.level(), QStringLiteral("Hard"));
        QVERIFY(givens(game) == shared);
    }

    void padLayoutIsClampedAndPersisted()
    {
        {
            SudokuGame game;
            QCOMPARE(game.padWidth(), 1.0);
            QCOMPARE(game.padOffset(), 0.0);
            QSignalSpy options(&game, &SudokuGame::optionsChanged);
            game.setPadWidth(0.2); // below the minimum
            game.setPadOffset(3.0); // past the right edge
            QCOMPARE(game.padWidth(), SudokuGame::MinPadWidth);
            QCOMPARE(game.padOffset(), 1.0);
            QCOMPARE(options.count(), 2);
            game.setPadOffset(1.0); // unchanged: no signal
            QCOMPARE(options.count(), 2);
            game.setPadWidth(0.75);
            game.setPadOffset(-0.4);
        }
        SudokuGame game;
        QCOMPARE(game.padWidth(), 0.75);
        QCOMPARE(game.padOffset(), -0.4);
    }
};

QTEST_GUILESS_MAIN(SudokuGameTests)
#include "sudoku_game_tests.moc"
