// Qt-side tests: the Sudoku session on top of the framework's GameSession.

#include "SudokuGame.h"
#include "SaveStore.h"
#include "Stats.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class SudokuGameTests : public QObject {
    Q_OBJECT

private:
    static int firstEmpty(const SudokuGame& game)
    {
        const QVariantList cells = game.cells();
        for (int i = 0; i < 81; ++i)
            if (cells[i].toMap().value(QStringLiteral("digit")).toInt() == 0)
                return i;
        return -1;
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
        QCOMPARE(game.cells()[cell].toMap().value(QStringLiteral("digit")).toInt(), 5);
        QVERIFY(game.canUndo());
        QCOMPARE(feedback.count(), 1);

        game.undo();
        QCOMPARE(game.cells()[cell].toMap().value(QStringLiteral("digit")).toInt(), 0);
        QVERIFY(game.canRedo());

        game.setNotesMode(true);
        game.enter(3);
        game.enter(7);
        QCOMPARE(game.cells()[cell].toMap().value(QStringLiteral("notes")).toInt(), (1 << 3) | (1 << 7));
        game.enter(3);
        QCOMPARE(game.cells()[cell].toMap().value(QStringLiteral("notes")).toInt(), 1 << 7);

        game.erase();
        QCOMPARE(game.cells()[cell].toMap().value(QStringLiteral("notes")).toInt(), 0);
    }

    void givensCannotChange()
    {
        SudokuGame game;
        startAndWait(game, QStringLiteral("Easy"));
        const QVariantList cells = game.cells();
        int given = -1;
        for (int i = 0; i < 81 && given < 0; ++i)
            if (cells[i].toMap().value(QStringLiteral("given")).toBool())
                given = i;
        game.setSelected(given);
        const int before = cells[given].toMap().value(QStringLiteral("digit")).toInt();
        game.enter(before == 9 ? 1 : before + 1);
        game.erase();
        QCOMPARE(game.cells()[given].toMap().value(QStringLiteral("digit")).toInt(), before);
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
        {
            SudokuGame game;
            startAndWait(game, QStringLiteral("Expert"));
            const int cell = firstEmpty(game);
            game.setSelected(cell);
            game.enter(4);
            saved = game.cells();
        }
        SudokuGame restored;
        QVERIFY(restored.hasSavedGame());
        QVERIFY(restored.resume());
        QCOMPARE(restored.level(), QStringLiteral("Expert"));
        QCOMPARE(restored.cells(), saved);
        QVERIFY(restored.active());
    }
};

QTEST_GUILESS_MAIN(SudokuGameTests)
#include "sudoku_game_tests.moc"
