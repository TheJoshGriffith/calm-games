#include "Stats.h"

#include "SaveStore.h"

#include <algorithm>
#include <functional>

namespace {

QString keyFor(const QString& gameId)
{
    return QStringLiteral("stats-") + gameId;
}

void update(const QString& gameId, const QString& level, const std::function<void(QVariantMap&)>& change)
{
    QVariantMap all = SaveStore::load(keyFor(gameId));
    QVariantMap entry = all.value(level).toMap();
    change(entry);
    all.insert(level, entry);
    SaveStore::save(keyFor(gameId), all);
}

} // namespace

void Stats::recordStarted(const QString& gameId, const QString& level)
{
    update(gameId, level, [](QVariantMap& e) {
        e[QStringLiteral("played")] = e.value(QStringLiteral("played")).toInt() + 1;
        // Starting again while the previous game was never won ends the streak.
        if (e.value(QStringLiteral("unfinished")).toBool())
            e[QStringLiteral("streak")] = 0;
        e[QStringLiteral("unfinished")] = true;
    });
}

void Stats::recordWon(const QString& gameId, const QString& level, int seconds, bool usedHints)
{
    update(gameId, level, [seconds, usedHints](QVariantMap& e) {
        e[QStringLiteral("won")] = e.value(QStringLiteral("won")).toInt() + 1;
        // Stats stored before hint tracking simply lack this key (read as 0).
        if (!usedHints)
            e[QStringLiteral("wonNoHints")] = e.value(QStringLiteral("wonNoHints")).toInt() + 1;
        e[QStringLiteral("totalSeconds")] = e.value(QStringLiteral("totalSeconds")).toLongLong() + seconds;
        const int best = e.value(QStringLiteral("bestSeconds")).toInt();
        if (best == 0 || seconds < best)
            e[QStringLiteral("bestSeconds")] = seconds;
        const int streak = e.value(QStringLiteral("streak")).toInt() + 1;
        e[QStringLiteral("streak")] = streak;
        e[QStringLiteral("bestStreak")] = std::max(streak, e.value(QStringLiteral("bestStreak")).toInt());
        e[QStringLiteral("unfinished")] = false;
    });
}

QVariantMap Stats::forLevel(const QString& gameId, const QString& level)
{
    const QVariantMap e = SaveStore::load(keyFor(gameId)).value(level).toMap();
    const int played = e.value(QStringLiteral("played")).toInt();
    const int won = e.value(QStringLiteral("won")).toInt();
    QVariantMap out;
    out[QStringLiteral("played")] = played;
    out[QStringLiteral("won")] = won;
    out[QStringLiteral("wonNoHints")] = e.value(QStringLiteral("wonNoHints")).toInt();
    out[QStringLiteral("winRate")] = played > 0 ? std::clamp(double(won) / played, 0.0, 1.0) : 0.0;
    out[QStringLiteral("bestSeconds")] = e.value(QStringLiteral("bestSeconds")).toInt();
    out[QStringLiteral("averageSeconds")] = won ? int(e.value(QStringLiteral("totalSeconds")).toLongLong() / won) : 0;
    out[QStringLiteral("streak")] = e.value(QStringLiteral("streak")).toInt();
    out[QStringLiteral("bestStreak")] = e.value(QStringLiteral("bestStreak")).toInt();
    return out;
}

void Stats::reset(const QString& gameId)
{
    SaveStore::remove(keyFor(gameId));
}
