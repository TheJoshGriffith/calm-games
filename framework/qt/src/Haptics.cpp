#include "Haptics.h"

void Haptics::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
}

void Haptics::play(const QString& event)
{
    if (!m_enabled)
        return;
    // Desktop builds have no haptic hardware. Mobile implementations map
    // "tap"/"soft" to a light impact, "complete" to medium, "win" to success.
    Q_UNUSED(event);
}
