#include "Haptics.h"

#include "platform/Platform.h"

#include <algorithm>

void Haptics::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
}

void Haptics::setStrength(int strength)
{
    strength = std::clamp(strength, 0, 2);
    if (m_strength == strength)
        return;
    m_strength = strength;
    emit strengthChanged();
}

void Haptics::play(const QString& event)
{
    if (!m_enabled)
        return;
    static constexpr qreal kScale[] = { 0.45, 0.75, 1.0 };
    calm::platform::haptic(event, kScale[m_strength]);
}
