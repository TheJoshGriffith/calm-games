#include "SoundBank.h"

#include "platform/Platform.h"

#include <QUrl>

namespace {
const QString kBase = QStringLiteral("qrc:/qt/qml/Calm/sounds/");
constexpr qint64 kCooldownMs = 45;

struct ThemeInfo {
    const char* id;
    const char* name;
};

// Display order; the first is the default. Ids are folder names under sounds/.
constexpr ThemeInfo kThemes[] = {
    { "water", QT_TRANSLATE_NOOP("SoundBank", "Water") },
    { "chimes", QT_TRANSLATE_NOOP("SoundBank", "Wind chimes") },
    { "felt", QT_TRANSLATE_NOOP("SoundBank", "Felt") },
    { "kalimba", QT_TRANSLATE_NOOP("SoundBank", "Kalimba") },
    { "driftwood", QT_TRANSLATE_NOOP("SoundBank", "Driftwood") },
};
} // namespace

QStringList SoundBank::themeIds()
{
    QStringList ids;
    for (const ThemeInfo& theme : kThemes)
        ids << QString::fromLatin1(theme.id);
    return ids;
}

QVariantList SoundBank::themes() const
{
    QVariantList list;
    for (const ThemeInfo& theme : kThemes)
        list << QVariantMap { { QStringLiteral("id"), QString::fromLatin1(theme.id) },
                              { QStringLiteral("name"), tr(theme.name) } };
    return list;
}

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
    loadTheme();
}

void SoundBank::add(const QString& event, const QStringList& files, qreal gain)
{
    Event e;
    e.gain = gain;
    e.files = files;
    for (qsizetype i = 0; i < files.size(); ++i)
        e.variants.push_back(new QSoundEffect(this));
    m_events.insert(event, e);
}

void SoundBank::loadTheme()
{
    const QString base = kBase + m_theme + QLatin1Char('/');
    for (Event& e : m_events) {
        for (qsizetype i = 0; i < e.files.size(); ++i)
            e.variants[i]->setSource(QUrl(base + e.files[i] + QStringLiteral(".wav")));
    }
}

void SoundBank::setTheme(const QString& theme)
{
    if (theme == m_theme || !themeIds().contains(theme))
        return;
    m_theme = theme;
    loadTheme();
    emit themeChanged();
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
    playEffect(effect, it->gain * jitter);
}

void SoundBank::preview()
{
    if (!m_enabled || m_volume <= 0.0)
        return;
    const Event& e = m_events[QStringLiteral("complete")];
    playEffect(e.variants.at(2), e.gain);
}

void SoundBank::playEffect(QSoundEffect* effect, qreal gain)
{
    effect->setVolume(qBound(0.0, m_volume * gain, 1.0));
    if (effect->status() == QSoundEffect::Ready || effect->status() == QSoundEffect::Error) {
        effect->play();
        return;
    }
    // Still loading (a theme was just picked): play once it's ready.
    connect(effect, &QSoundEffect::statusChanged, effect, [effect] {
        if (effect->status() == QSoundEffect::Ready)
            effect->play();
    }, Qt::SingleShotConnection);
}
