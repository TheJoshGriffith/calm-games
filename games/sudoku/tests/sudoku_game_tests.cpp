// Qt-side tests: the Sudoku session on top of the framework's GameSession.

#include "SaveStore.h"
#include "SudokuGame.h"
#include "Stats.h"

#include <sudoku/Board.h>

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
};

QTEST_GUILESS_MAIN(SudokuGameTests)
#include "sudoku_game_tests.moc"
