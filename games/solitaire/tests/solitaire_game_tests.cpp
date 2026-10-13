// Qt-side tests: the Solitaire session on top of the framework's GameSession.

#include "SolitaireGame.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class SolitaireGameTests : public QObject {
    Q_OBJECT

private:
    static void startAndWait(SolitaireGame& game, const QString& level)
    {
        game.startNewGame(level);
        QTRY_VERIFY_WITH_TIMEOUT(game.active() && !game.loading(), 30000);
    }

    static QVariantMap card(const SolitaireGame& game, int id)
    {
        return game.cards().at(id).toMap();
    }

    // Plays hints until the game is won or no hint helps; true if won.
    static bool playByHints(SolitaireGame& game, int limit)
    {
        for (int i = 0; i < limit && !game.complete(); ++i) {
            if (game.canAutoFinish()) {
                game.autoFinish();
                return QTest::qWaitFor([&game] { return game.complete(); }, 20000);
            }
            if (!game.showHint())
                return false;
            const QVariantMap hint = game.hint();
            if (hint.value(QStringLiteral("stock")).toBool()) {
                game.draw();
                continue;
            }
            const int id = hint.value(QStringLiteral("cards")).toList().constFirst().toInt();
            if (!game.dropCard(id, hint.value(QStringLiteral("pile")).toInt(), hint.value(QStringLiteral("slot")).toInt()))
                return false;
        }
        return game.complete();
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("CalmGamesTest"));
        QCoreApplication::setApplicationName(QStringLiteral("SolitaireTest"));
    }

    void init()
    {
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
        QSettings().clear();
    }

    void newGameDealsTheTable()
    {
        SolitaireGame game;
        startAndWait(game, QStringLiteral("Draw one"));
        QCOMPARE(game.drawCount(), 1);
        QCOMPARE(game.stockCount(), 24);
        QCOMPARE(game.moves(), 0);
        int faceUp = 0;
        for (const QVariant& c : game.cards()) {
            const QVariantMap m = c.toMap();
            QVERIFY(m.contains(QStringLiteral("pile")));
            faceUp += m.value(QStringLiteral("faceUp")).toBool();
        }
        QCOMPARE(faceUp, 7);
        QVERIFY(game.hasSavedGame());

        startAndWait(game, QStringLiteral("Draw three"));
        QCOMPARE(game.drawCount(), 3);
    }

    void drawUndoAndRedo()
    {
        SolitaireGame game;
        startAndWait(game, QStringLiteral("Draw three"));
        QSignalSpy feedback(&game, &GameSession::feedback);
        game.draw();
        QCOMPARE(game.stockCount(), 21);
        QCOMPARE(game.wasteCount(), 3);
        QCOMPARE(game.moves(), 1);
        QCOMPARE(feedback.last().at(0).toString(), QStringLiteral("soft"));
        QVERIFY(game.canUndo());
        game.undo();
        QCOMPARE(game.stockCount(), 24);
        QCOMPARE(game.moves(), 0);
        game.redo();
        QCOMPARE(game.wasteCount(), 3);

        // Turning the whole stock over and back again.
        for (int i = 0; i < 7; ++i)
            game.draw();
        QCOMPARE(game.stockCount(), 0);
        game.draw();
        QCOMPARE(game.stockCount(), 24);
        QCOMPARE(feedback.last().at(0).toString(), QStringLiteral("erase"));
    }

    void illegalDropIsRefused()
    {
        SolitaireGame game;
        startAndWait(game, QStringLiteral("Draw one"));
        // A face-down card can't go anywhere.
        int hidden = -1;
        for (int id = 0; id < 52 && hidden < 0; ++id) {
            const QVariantMap c = card(game, id);
            if (c.value(QStringLiteral("pile")).toInt() == SolitaireGame::Tableau && !c.value(QStringLiteral("faceUp")).toBool())
                hidden = id;
        }
        QVERIFY(hidden >= 0);
        QSignalSpy history(&game, &GameSession::historyChanged);
        QVERIFY(!game.dropCard(hidden, SolitaireGame::Tableau, 0));
        QVERIFY(!game.canDrop(hidden, SolitaireGame::Tableau, 1));
        QCOMPARE(history.count(), 0);
        QCOMPARE(game.moves(), 0);
    }

    void saveAndResume()
    {
        QString table;
        {
            SolitaireGame game;
            startAndWait(game, QStringLiteral("Draw one"));
            game.draw();
            game.draw();
            table = QString::number(game.wasteCount());
        }
        SolitaireGame resumed;
        QVERIFY(resumed.hasSavedGame());
        QVERIFY(resumed.resume());
        QCOMPARE(resumed.level(), QStringLiteral("Draw one"));
        QCOMPARE(QString::number(resumed.wasteCount()), table);
        QCOMPARE(resumed.moves(), 2);
    }

    void winnableDealCanBePlayedOut()
    {
        // Hints alone won't win every deal, but across a few winnable deals
        // the session must reach a win and record it.
        bool wonOne = false;
        for (int attempt = 0; attempt < 6 && !wonOne; ++attempt) {
            SolitaireGame game;
            startAndWait(game, QStringLiteral("Draw one"));
            QSignalSpy won(&game, &GameSession::won);
            wonOne = playByHints(game, 800);
            if (wonOne) {
                QCOMPARE(won.count(), 1);
                QCOMPARE(game.foundationCards(), 52);
                QVERIFY(!game.hasSavedGame());
            }
        }
        QVERIFY(wonOne);
    }
};

QTEST_MAIN(SolitaireGameTests)
#include "solitaire_game_tests.moc"
