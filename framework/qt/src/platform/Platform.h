#pragma once

#include <QString>

// Thin platform hooks. Each platform provides its own implementation file;
// desktop builds use the no-op one.
namespace calm::platform {

// Mix quietly with the player's music and respect the silent switch.
void configureAudioSession();

// Haptic feedback for a GameSession feedback event; `scale` (0..1) is the
// player's chosen strength.
void haptic(const QString& event, qreal scale);

} // namespace calm::platform
