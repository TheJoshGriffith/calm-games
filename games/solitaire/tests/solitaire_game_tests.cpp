// Qt-side tests: the Solitaire session on top of the framework's GameSession.

#include "SolitaireGame.h"
#include "SaveStore.h"

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

    // Resumes a game from a hand-made table (Klondike::serialize format).
    static void resumeTable(SolitaireGame& game, const QString& table)
    {
        QVariantMap saved;
        saved.insert(QStringLiteral("level"), QStringLiteral("Draw one"));
        saved.insert(QStringLiteral("elapsed"), 30);
        saved.insert(QStringLiteral("game"), QVariantMap { { QStringLiteral("table"), table } });
        QVERIFY(SaveStore::save(game.gameId(), saved));
        QVERIFY(game.resume());
    }

    static int cardAt(const SolitaireGame& game, int pile, int slot, int pos)
    {
        const QVariantList cards = game.cards();
        for (int id = 0; id < cards.size(); ++id) {
            const QVariantMap c = cards[id].toMap();
            if (c.value(QStringLiteral("pile")).toInt() == pile && c.value(QStringLiteral("slot")).toInt() == slot
                && c.value(QStringLiteral("pos")).toInt() == pos)
                return id;
        }
        return -1;
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

    void quickMoveGoesHomeThenLeftmost()
    {
        SolitaireGame game;
        // Waste 6h; black sevens in columns 2 and 6; the ace of clubs alone
        // in column 4; the king of diamonds alone in column 0.
        resumeTable(game, QStringLiteral(
            "1|2s3s4s5s6s9sTsJsQsKsAh2h3h4h5h7h8h9hThJhQhKhAd2d3d4d5d6d7d8d9dTdJdQd2c3c4c5c6c8c9cTcJcQcKcAs|6H|||||KD|8S|7S||AC||7C"));
        QSignalSpy feedback(&game, &GameSession::feedback);

        const int ace = cardAt(game, SolitaireGame::Tableau, 4, 0);
        QVERIFY(game.quickMove(ace));
        QCOMPARE(card(game, ace).value(QStringLiteral("pile")).toInt(), int(SolitaireGame::Foundation));

        const int six = cardAt(game, SolitaireGame::Waste, 0, 0);
        QVERIFY(game.quickMove(six));
        QCOMPARE(card(game, six).value(QStringLiteral("slot")).toInt(), 2); // leftmost black seven

        // The lone king has nowhere better to go: refused, no move counted.
        const int moves = game.moves();
        QVERIFY(!game.quickMove(cardAt(game, SolitaireGame::Tableau, 0, 0)));
        QCOMPARE(game.moves(), moves);
        QCOMPARE(feedback.last().at(0).toString(), QStringLiteral("error"));
    }

    void warnsAboutLosingMove()
    {
        // Found by random play: playing the ten of diamonds from the waste
        // onto the jack of spades loses a deal that was still winnable.
        const QString table = QStringLiteral(
            "1|Jh3d7d7c4dKcTs6s9s2c9cAd|4H8HTD||AH|||9D|Jc7H6C5D4C3H|2hQc7S|6dAcKHQSJD|Qd4s8s2s9H|Tc5c8dKd6h3S2D|8c5s3cTh5hAsKSQHJS");
        SolitaireGame game;
        resumeTable(game, table);
        const int ten = cardAt(game, SolitaireGame::Waste, 0, 2);
        QVERIFY(game.dropCard(ten, SolitaireGame::Tableau, 6));
        QTRY_VERIFY_WITH_TIMEOUT(game.losingMove(), 60000);

        // Undo clears it, and an unrelated draw doesn't raise it.
        game.undo();
        QVERIFY(!game.losingMove());
        game.draw();
        QTest::qWait(200);
        QVERIFY(!game.losingMove());

        // Switched off: no warning.
        SolitaireGame quiet;
        quiet.setWarnLosingMoves(false);
        resumeTable(quiet, table);
        QVERIFY(quiet.dropCard(cardAt(quiet, SolitaireGame::Waste, 0, 2), SolitaireGame::Tableau, 6));
        QTest::qWait(1500);
        QVERIFY(!quiet.losingMove());
    }

    void autoCompleteOnceAllFaceUp()
    {
        SolitaireGame game;
        resumeTable(game, QStringLiteral(
            "1|9c|TC|AS2S3S4S5S6S7S8S9STSJS|AH2H3H4H5H6H7H8H9HTHJH|AD2D3D4D5D6D7D8D9DTDJD|AC2C3C4C5C6C7C8C|KSQH|KHQS|KDQCJC|KCQD|||"));
        QVERIFY(game.canAutoFinish());
        QSignalSpy won(&game, &GameSession::won);
        game.autoFinish();
        QVERIFY(game.autoFinishing());
        QTRY_VERIFY_WITH_TIMEOUT(game.complete(), 20000);
        QCOMPARE(won.count(), 1);
        QVERIFY(!game.autoFinishing());

        // Not while a card is still face down.
        SolitaireGame hidden;
        resumeTable(hidden, QStringLiteral(
            "1|9c|TC|AS2S3S4S5S6S7S8S9STSJS|AH2H3H4H5H6H7H8H9HTHJH|AD2D3D4D5D6D7D8D9DTDJD|AC2C3C4C5C6C7C8C|KSQH|KHQS|KDQcJC|KCQD|||"));
        QVERIFY(!hidden.canAutoFinish());
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
