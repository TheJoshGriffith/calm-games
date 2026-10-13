#include "Haptics.h"

#include "platform/Platform.h"

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
    calm::platform::haptic(event);
}
