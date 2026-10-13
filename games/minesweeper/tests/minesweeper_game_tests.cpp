// Qt-side tests: the Minesweeper session on top of the framework's GameSession.

#include "MinesweeperGame.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class MinesweeperGameTests : public QObject {
    Q_OBJECT

private:
    static int centre(const MinesweeperGame& game) { return (game.rows() / 2) * game.columns() + game.columns() / 2; }

    static int count(const MinesweeperGame& game, int view)
    {
        int n = 0;
        for (const QVariant& c : game.cells())
            n += c.toInt() == view;
        return n;
    }

    static int countOpen(const MinesweeperGame& game)
    {
        int n = 0;
        for (const QVariant& c : game.cells())
            n += c.toInt() >= 0 && c.toInt() <= 8;
        return n;
    }

    // Plays a whole board using hints only (each one proven safe).
    static void solveWithHints(MinesweeperGame& game)
    {
        for (int guard = 0; guard < 2000 && !game.complete(); ++guard)
            QVERIFY(game.hint());
        QVERIFY(game.complete());
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("CalmGamesTest"));
        QCoreApplication::setApplicationName(QStringLiteral("MinesweeperTest"));
    }

    void init()
    {
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
        QSettings().clear();
    }

    void newGameIsHiddenAndSized()
    {
        MinesweeperGame game;
        game.startNewGame(QStringLiteral("Medium"));
        QVERIFY(game.active());
        QCOMPARE(game.columns(), 10);
        QCOMPARE(game.rows(), 14);
        QCOMPARE(game.minesLeft(), 24);
        QCOMPARE(count(game, MinesweeperGame::Hidden), 140);
        QVERIFY(game.hasSavedGame());
    }

    void firstTapOpensAnArea()
    {
        MinesweeperGame game;
        game.startNewGame(QStringLiteral("Expert"));
        QSignalSpy feedback(&game, &MinesweeperGame::feedback);
        game.tap(0);
        QVERIFY(!game.lost());
        QVERIFY(countOpen(game) > 1);
        QCOMPARE(game.cells().at(0).toInt(), 0);
        QCOMPARE(feedback.count(), 1);
        QCOMPARE(feedback.first().at(0).toString(), QStringLiteral("complete"));
    }

    void flagModeAndUndo()
    {
        MinesweeperGame game;
        game.startNewGame(QStringLiteral("Easy"));
        game.setFlagMode(true);
        game.tap(5);
        QCOMPARE(game.cells().at(5).toInt(), int(MinesweeperGame::Flagged));
        QCOMPARE(game.minesLeft(), 9);
        game.open(5); // flagged cells don't open
        QCOMPARE(game.cells().at(5).toInt(), int(MinesweeperGame::Flagged));
        game.undo();
        QCOMPARE(game.cells().at(5).toInt(), int(MinesweeperGame::Hidden));
        QVERIFY(!game.canUndo());
    }

    void hintsClearTheBoardAndRecordAWin()
    {
        MinesweeperGame game;
        game.startNewGame(QStringLiteral("Hard"));
        QSignalSpy won(&game, &MinesweeperGame::won);
        solveWithHints(game);
        QCOMPARE(won.count(), 1);
        QCOMPARE(game.minesLeft(), 0);
        QVERIFY(!game.hasSavedGame());
    }

    void hintRemovesWrongFlagFirst()
    {
        MinesweeperGame game;
        game.startNewGame(QStringLiteral("Easy"));
        game.open(centre(game));
        // Flag hidden cells one by one until a hint takes a flag back off.
        int safeHidden = -1;
        for (int i = 0; i < 81 && safeHidden < 0; ++i) {
            if (game.cells().at(i).toInt() != MinesweeperGame::Hidden)
                continue;
            game.flag(i);
            const int before = game.minesLeft();
            game.hint();
            if (game.cells().at(i).toInt() == MinesweeperGame::Hidden && game.minesLeft() == before + 1)
                safeHidden = i;
            else if (game.cells().at(i).toInt() == MinesweeperGame::Flagged)
                game.flag(i);
        }
        QVERIFY(safeHidden >= 0);
    }

    void steppingOnAMineCanBeTakenBack()
    {
        MinesweeperGame game;
        game.startNewGame(QStringLiteral("Easy"));
        game.open(centre(game));
        // Flag every mine the hints find, then open a flagged mine on purpose
        // by unflagging it first.
        int mine = -1;
        for (int guard = 0; guard < 200 && mine < 0; ++guard) {
            const int flagsBefore = count(game, MinesweeperGame::Flagged);
            QVERIFY(game.hint());
            if (count(game, MinesweeperGame::Flagged) > flagsBefore) {
                for (int i = 0; i < 81; ++i)
                    if (game.cells().at(i).toInt() == MinesweeperGame::Flagged)
                        mine = i;
            }
        }
        QVERIFY(mine >= 0);
        game.flag(mine);
        QSignalSpy feedback(&game, &MinesweeperGame::feedback);
        game.open(mine);
        QVERIFY(game.lost());
        QVERIFY(game.paused());
        QCOMPARE(game.cells().at(mine).toInt(), int(MinesweeperGame::Exploded));
        QCOMPARE(feedback.last().at(0).toString(), QStringLiteral("error"));

        // A lost game resumes from just before the mine.
        {
            MinesweeperGame resumed;
            QVERIFY(resumed.resume());
            QVERIFY(!resumed.lost());
            QCOMPARE(resumed.cells().at(mine).toInt(), int(MinesweeperGame::Hidden));
        }

        game.undo();
        QVERIFY(!game.lost());
        QVERIFY(!game.paused());
        QCOMPARE(game.cells().at(mine).toInt(), int(MinesweeperGame::Hidden));
    }

    void saveAndResume()
    {
        QVariantList before;
        {
            MinesweeperGame game;
            game.startNewGame(QStringLiteral("Medium"));
            game.open(centre(game));
            game.flag(0);
            before = game.cells();
        }
        MinesweeperGame game;
        QVERIFY(game.resume());
        QCOMPARE(game.level(), QStringLiteral("Medium"));
        QCOMPARE(game.cells(), before);
        solveWithHints(game);
    }

    void optionsPersist()
    {
        {
            MinesweeperGame game;
            QVERIFY(game.noGuessing());
            game.setNoGuessing(false);
        }
        MinesweeperGame game;
        QVERIFY(!game.noGuessing());
    }
};

QTEST_MAIN(MinesweeperGameTests)
#include "minesweeper_game_tests.moc"
