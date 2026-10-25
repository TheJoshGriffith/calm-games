#include "SystemBars.h"

#include "platform/Platform.h"

void SystemBars::update(const QColor& background, const QString& style)
{
    calm::platform::setSystemBars(background, style);
}
