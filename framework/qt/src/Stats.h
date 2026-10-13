#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

// Per game and level: games played and won, wins without hints, best and total
// winning time, and win streaks. Stored with SaveStore under "stats-<gameId>".
class Stats : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    static void recordStarted(const QString& gameId, const QString& level);
    static void recordWon(const QString& gameId, const QString& level, int seconds, bool usedHints = false);

    // Keys: played, won, wonNoHints, winRate (0..1; 0 when none played),
    // bestSeconds (0 = none), averageSeconds, streak, bestStreak.
    Q_INVOKABLE static QVariantMap forLevel(const QString& gameId, const QString& level);
    Q_INVOKABLE static void reset(const QString& gameId);
};
