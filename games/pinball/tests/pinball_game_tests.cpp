// Qt-side tests: the pinball session on top of the framework's GameSession.

#include "PinballGame.h"
#include "Stats.h"

#include <QDir>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class PinballGameTests : public QObject {
    Q_OBJECT

private:
    static constexpr double Frame = 1.0 / 60.0;

    // Plays unattended (no flipping) until `done` holds; false on timeout.
    template <typename Done>
    static bool playUntil(PinballGame& game, Done done, int maxFrames = 60 * 60 * 10)
    {
        for (int i = 0; i < maxFrames; ++i) {
            if (done())
                return true;
            if (game.ballReady()) {
                game.setPlunger(true);
                game.setPlunger(false);
            }
            game.advance(Frame);
        }
        return done();
    }

    static QStringList events(const QSignalSpy& spy)
    {
        QStringList out;
        for (const auto& args : spy)
            out << args.at(0).toString();
        return out;
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("CalmGamesTest"));
        QCoreApplication::setApplicationName(QStringLiteral("PinballTest"));
    }

    void init()
    {
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
        QSettings().clear();
    }

    void tableGeometryIsExposed()
    {
        PinballGame game;
        QCOMPARE(game.tableWidth(), 100.0);
        QVERIFY(game.outline().size() > 10);
        QVERIFY(game.walls().size() > 10);
        QCOMPARE(game.bumpers().size(), 3);
        QCOMPARE(game.slingshots().size(), 2);
        QCOMPARE(game.rollovers().size(), 3);
        QCOMPARE(game.flippers().size(), 2);
        QCOMPARE(game.lights().size(), 3);
    }

    void newGameWaitsInTheLane()
    {
        PinballGame game;
        game.startNewGame(QStringLiteral("Gentle"));
        QVERIFY(game.active());
        QVERIFY(game.endless());
        QVERIFY(game.ballReady());
        QCOMPARE(game.score(), 0.0);
        QVERIFY(game.hasSavedGame());
        QVERIFY(game.ballX() > game.laneLeft());
    }

    void launchPlaysASoftSound()
    {
        PinballGame game;
        game.startNewGame(QStringLiteral("Gentle"));
        QSignalSpy feedback(&game, &GameSession::feedback);
        game.setPlunger(true);
        for (int i = 0; i < 30; ++i)
            game.advance(Frame);
        QVERIFY(game.plungerCharge() > 0.3);
        game.setPlunger(false);
        QVERIFY(!game.ballReady());
        QCOMPARE(events(feedback), QStringList { QStringLiteral("soft") });
        const double startY = game.ballY();
        game.advance(0.2);
        QVERIFY(game.ballY() < startY);
    }

    void flippersFollowInput()
    {
        PinballGame game;
        game.startNewGame(QStringLiteral("Gentle"));
        const double rest = game.leftAngle();
        QSignalSpy flipped(&game, &PinballGame::flipped);
        game.setFlipper(0, true);
        game.advance(0.1);
        QVERIFY(game.leftAngle() < rest - 30);
        QCOMPARE(flipped.count(), 1);
        game.setFlipper(0, false);
        game.advance(0.3);
        QCOMPARE(game.leftAngle(), rest);
    }

    void classicEndsAfterThreeBalls()
    {
        PinballGame game;
        game.startNewGame(QStringLiteral("Classic"));
        QVERIFY(!game.endless());
        QCOMPARE(game.ballsLeft(), 3);
        QSignalSpy feedback(&game, &GameSession::feedback);
        QSignalSpy drained(&game, &PinballGame::ballDrained);
        QVERIFY(playUntil(game, [&] { return game.complete(); }));
        QCOMPARE(drained.count(), 3);
        QCOMPARE(game.ballsLeft(), 0);
        QVERIFY(events(feedback).contains(QStringLiteral("erase")));
        QCOMPARE(events(feedback).last(), QStringLiteral("win"));
        QVERIFY(!game.hasSavedGame());
        QCOMPARE(Stats::forLevel(QStringLiteral("pinball"), QStringLiteral("Classic")).value(QStringLiteral("played")).toInt(), 1);
        QCOMPARE(game.best(), game.score());

        // Input after the game is over does nothing.
        game.setFlipper(0, true);
        game.setPlunger(true);
        game.advance(0.5);
        QVERIFY(feedback.last().at(0).toString() == QStringLiteral("win"));
    }

    void gentleNeverEnds()
    {
        PinballGame game;
        game.startNewGame(QStringLiteral("Gentle"));
        QSignalSpy drained(&game, &PinballGame::ballDrained);
        QVERIFY(playUntil(game, [&] { return drained.count() >= 4; }));
        QVERIFY(!game.complete());
        QVERIFY(game.active());
    }

    void resumeRestoresTheScoreboard()
    {
        double score = 0;
        {
            PinballGame game;
            game.startNewGame(QStringLiteral("Classic"));
            QSignalSpy drained(&game, &PinballGame::ballDrained);
            QVERIFY(playUntil(game, [&] { return drained.count() == 1; }));
            score = game.score();
            QCOMPARE(game.ballsLeft(), 2);
            QVERIFY(game.hasSavedGame());
        }
        PinballGame game;
        QVERIFY(game.resume());
        QCOMPARE(game.level(), QStringLiteral("Classic"));
        QVERIFY(!game.endless());
        QCOMPARE(game.ballsLeft(), 2);
        QCOMPARE(game.score(), score);
        QVERIFY(game.ballReady());
    }

    void physicsRunsOnlyOnTable()
    {
        PinballGame game;
        QSignalSpy running(&game, &PinballGame::runningChanged);
        game.startNewGame(QStringLiteral("Gentle"));
        QVERIFY(!game.running()); // not on screen yet

        game.setOnTable(true);
        QVERIFY(game.running());
        QVERIFY(!game.paused());

        game.setOnTable(false);
        QVERIFY(!game.running());
        QVERIFY(game.paused());
        QCOMPARE(running.count(), 2);
    }

    void partsNotifyOnlyWhenTheyChange()
    {
        PinballGame game;
        game.startNewGame(QStringLiteral("Classic"));
        QSignalSpy stars(&game, &PinballGame::starsChanged);
        QSignalSpy drops(&game, &PinballGame::dropTargetsChanged);
        QSignalSpy frames(&game, &PinballGame::frame);
        game.setPlunger(true);
        game.setPlunger(false);
        for (int i = 0; i < 120; ++i)
            game.advance(1.0 / 60);
        QCOMPARE(frames.count(), 120);
        QVERIFY(stars.count() < 10);
        QVERIFY(drops.count() < 10);
    }
};

QTEST_MAIN(PinballGameTests)
#include "pinball_game_tests.moc"
