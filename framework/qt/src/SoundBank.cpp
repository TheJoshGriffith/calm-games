#include "SoundBank.h"

#include "platform/Platform.h"

#include <QUrl>

namespace {
const QString kBase = QStringLiteral("qrc:/qt/qml/Calm/sounds/");
constexpr qint64 kCooldownMs = 45;
} // namespace

SoundBank::SoundBank(QObject* parent)
    : QObject(parent)
{
    calm::platform::configureAudioSession();
    m_clock.start();
    add(QStringLiteral("tap"), { QStringLiteral("tap_1"), QStringLiteral("tap_2"), QStringLiteral("tap_3") }, 1.0);
    add(QStringLiteral("soft"), { QStringLiteral("soft_1"), QStringLiteral("soft_2"), QStringLiteral("soft_3") }, 0.8);
    add(QStringLiteral("erase"), { QStringLiteral("erase") }, 0.9);
    add(QStringLiteral("error"), { QStringLiteral("error") }, 0.8);
    add(QStringLiteral("win"), { QStringLiteral("win") }, 1.0);
    QStringList notes;
    for (int i = 0; i < 8; ++i)
        notes << QStringLiteral("complete_%1").arg(i);
    add(QStringLiteral("complete"), notes, 0.9);
}

void SoundBank::add(const QString& event, const QStringList& files, qreal gain)
{
    Event e;
    e.gain = gain;
    for (const QString& file : files) {
        auto* effect = new QSoundEffect(this);
        effect->setSource(QUrl(kBase + file + QStringLiteral(".wav")));
        e.variants.push_back(effect);
    }
    m_events.insert(event, e);
}

void SoundBank::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
}

void SoundBank::setVolume(qreal volume)
{
    if (qFuzzyCompare(m_volume + 1.0, volume + 1.0))
        return;
    m_volume = volume;
    emit volumeChanged();
}

void SoundBank::play(const QString& event, int step)
{
    if (!m_enabled || m_volume <= 0.0)
        return;
    auto it = m_events.find(event);
    if (it == m_events.end() || it->variants.isEmpty())
        return;

    const qint64 now = m_clock.elapsed();
    if (now - it->lastPlayed < kCooldownMs)
        return;
    it->lastPlayed = now;

    QSoundEffect* effect;
    if (event == QLatin1String("complete")) {
        // Successive completions climb the pentatonic scale.
        effect = it->variants.at(qBound(0, step, int(it->variants.size()) - 1));
    } else {
        effect = it->variants.at(it->next);
        it->next = (it->next + 1) % it->variants.size();
    }

    const qreal jitter = 0.9 + 0.2 * QRandomGenerator::global()->generateDouble();
    effect->setVolume(qBound(0.0, m_volume * it->gain * jitter, 1.0));
    effect->play();
}
