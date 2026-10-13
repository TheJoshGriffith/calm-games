#include "Settings.h"

#include <algorithm>

namespace {
const QString kTheme = QStringLiteral("app/theme");
const QString kSound = QStringLiteral("app/soundEnabled");
const QString kVolume = QStringLiteral("app/volume");
const QString kHaptics = QStringLiteral("app/hapticsEnabled");
const QString kReduceMotion = QStringLiteral("app/reduceMotion");
} // namespace

Settings::Settings(QObject* parent)
    : QObject(parent)
{
}

QString Settings::theme() const
{
    return m_store.value(kTheme, QStringLiteral("system")).toString();
}

void Settings::setTheme(const QString& theme)
{
    if (theme == this->theme())
        return;
    if (theme != QLatin1String("system") && theme != QLatin1String("light") && theme != QLatin1String("dark"))
        return;
    m_store.setValue(kTheme, theme);
    emit themeChanged();
}

bool Settings::soundEnabled() const
{
    return m_store.value(kSound, true).toBool();
}

void Settings::setSoundEnabled(bool enabled)
{
    if (enabled == soundEnabled())
        return;
    m_store.setValue(kSound, enabled);
    emit soundEnabledChanged();
}

qreal Settings::volume() const
{
    return m_store.value(kVolume, 0.7).toReal();
}

void Settings::setVolume(qreal volume)
{
    volume = std::clamp(volume, 0.0, 1.0);
    if (qFuzzyCompare(volume + 1.0, this->volume() + 1.0))
        return;
    m_store.setValue(kVolume, volume);
    emit volumeChanged();
}

bool Settings::hapticsEnabled() const
{
    return m_store.value(kHaptics, true).toBool();
}

void Settings::setHapticsEnabled(bool enabled)
{
    if (enabled == hapticsEnabled())
        return;
    m_store.setValue(kHaptics, enabled);
    emit hapticsEnabledChanged();
}

bool Settings::reduceMotion() const
{
    return m_store.value(kReduceMotion, false).toBool();
}

void Settings::setReduceMotion(bool reduce)
{
    if (reduce == reduceMotion())
        return;
    m_store.setValue(kReduceMotion, reduce);
    emit reduceMotionChanged();
}
