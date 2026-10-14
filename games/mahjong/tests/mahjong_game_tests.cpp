// Qt-side tests: the Mahjong session on top of the framework's GameSession.

#include "MahjongGame.h"

#include <mahjong/Tiles.h>

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class MahjongGameTests : public QObject {
    Q_OBJECT

private:
    // A pair of free, matching tiles on the table, or {-1, -1}.
    static std::pair<int, int> openPair(const MahjongGame& game)
    {
        const QVariantList kinds = game.kinds();
        const QVariantList free = game.freeList();
        for (int a = 0; a < kinds.size(); ++a)
            for (int b = a + 1; b < kinds.size(); ++b)
                if (free[a].toBool() && free[b].toBool() && mahjong::matches(kinds[a].toInt(), kinds[b].toInt()))
                    return { a, b };
        return { -1, -1 };
    }

    // Plays the whole deal by following hints.
    static void clearWithHints(MahjongGame& game)
    {
        for (int guard = 0; guard < 200 && !game.complete(); ++guard) {
            QVERIFY(game.hint());
            const QVariantList pair = game.hintPair();
            QCOMPARE(pair.size(), 2);
            game.tap(pair[0].toInt());
            game.tap(pair[1].toInt());
        }
        QVERIFY(game.complete());
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("CalmGamesTest"));
        QCoreApplication::setApplicationName(QStringLiteral("MahjongTest"));
    }

    void init()
    {
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
        QSettings().clear();
    }

    void newGameDealsTheLayout()
    {
        MahjongGame game;
        game.startNewGame(QStringLiteral("Turtle"));
        QVERIFY(game.active());
        QCOMPARE(game.level(), QStringLiteral("Turtle"));
        QCOMPARE(game.tilesLeft(), 144);
        QCOMPARE(game.slotList().size(), 144);
        QVERIFY(game.movesAvailable() > 0);
        QVERIFY(!game.stuck());
        QVERIFY(game.dealCode().startsWith(QStringLiteral("turtle-")));
        QVERIFY(game.layouts().size() >= 6);
    }

    void selectingAndMatching()
    {
        MahjongGame game;
        game.startNewGame(QStringLiteral("Garden"));
        const auto [a, b] = openPair(game);
        QVERIFY(a >= 0);
        QSignalSpy feedback(&game, &GameSession::feedback);

        game.tap(a);
        QCOMPARE(game.selected(), a);
        game.tap(a); // tapping again lets go
        QCOMPARE(game.selected(), -1);
        game.tap(a);
        game.tap(b);
        QCOMPARE(game.selected(), -1);
        QCOMPARE(game.tilesLeft(), 70);
        QVERIFY(!game.present()[a].toBool());
        QCOMPARE(feedback.last().at(0).toString(), QStringLiteral("complete"));
        QVERIFY(game.canUndo());

        game.undo();
        QCOMPARE(game.tilesLeft(), 72);
        QVERIFY(game.present()[a].toBool());
    }

    void blockedTilesCantBePicked()
    {
        MahjongGame game;
        game.startNewGame(QStringLiteral("Pyramid"));
        const int blocked = game.freeList().indexOf(false);
        QVERIFY(blocked >= 0);
        QSignalSpy spy(&game, &MahjongGame::blockedTapped);
        game.tap(blocked);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(game.selected(), -1);
    }

    void hintsClearTheWholeDeal()
    {
        MahjongGame game;
        game.startNewGame(QStringLiteral("Fortress"));
        clearWithHints(game);
        QVERIFY(game.hintsUsed() > 0);
        QVERIFY(!game.hasSavedGame());
    }

    void shuffleKeepsTheTiles()
    {
        MahjongGame game;
        game.startNewGame(QStringLiteral("Butterfly"));
        const auto [a, b] = openPair(game);
        game.tap(a);
        game.tap(b);
        auto faces = [&game] {
            QList<int> list;
            const QVariantList kinds = game.kinds();
            const QVariantList present = game.present();
            for (int i = 0; i < kinds.size(); ++i)
                if (present[i].toBool())
                    list.append(kinds[i].toInt());
            std::sort(list.begin(), list.end());
            return list;
        };
        const QList<int> before = faces();
        QVERIFY(game.shuffle());
        QCOMPARE(faces(), before);
        QCOMPARE(game.hintsUsed(), 1);
        // Still winnable after the shuffle.
        clearWithHints(game);
    }

    void savesAndResumes()
    {
        QVariantList kinds;
        QVariantList present;
        QString code;
        {
            MahjongGame game;
            game.startNewGame(QStringLiteral("Lantern"));
            const auto [a, b] = openPair(game);
            game.tap(a);
            game.tap(b);
            kinds = game.kinds();
            present = game.present();
            code = game.dealCode();
            QVERIFY(game.hasSavedGame());
        }
        MahjongGame resumed;
        QVERIFY(resumed.resume());
        QCOMPARE(resumed.level(), QStringLiteral("Lantern"));
        QCOMPARE(resumed.kinds(), kinds);
        QCOMPARE(resumed.present(), present);
        QCOMPARE(resumed.dealCode(), code);
        QVERIFY(resumed.hint());
    }

    void shareLinksRecreateTheDeal()
    {
        MahjongGame game;
        game.startNewGame(QStringLiteral("Turtle"));
        const QString url = game.shareUrl();
        QVERIFY(url.startsWith(MahjongGame::ShareUrlBase));
        const QVariantList kinds = game.kinds();

        MahjongGame friendGame;
        QVERIFY(friendGame.openShared(url));
        QCOMPARE(friendGame.level(), QStringLiteral("Turtle"));
        QCOMPARE(friendGame.kinds(), kinds);
        QCOMPARE(friendGame.dealCode(), game.dealCode());
        // A bare code works too, in any case.
        MahjongGame byCode;
        QVERIFY(byCode.openShared(game.dealCode().toUpper()));
        QCOMPARE(byCode.kinds(), kinds);

        QVERIFY(!friendGame.openShared(QStringLiteral("https://mahjong.gwynant.app/s/not-a-deal!")));
        QVERIFY(!friendGame.openShared(QStringLiteral("nowhere-123")));
        QVERIFY(!friendGame.openShared(QString()));
    }

    void optionsPersist()
    {
        {
            MahjongGame game;
            QCOMPARE(game.tileTheme(), QStringLiteral("jade"));
            QVERIFY(game.dimBlocked());
            game.setTileTheme(QStringLiteral("blossom"));
            game.setDimBlocked(false);
        }
        MahjongGame game;
        QCOMPARE(game.tileTheme(), QStringLiteral("blossom"));
        QVERIFY(!game.dimBlocked());
    }
};

QTEST_MAIN(MahjongGameTests)
#include "mahjong_game_tests.moc"
