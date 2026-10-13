#pragma once

#include <QString>

// Thin platform hooks. Each platform provides its own implementation file;
// desktop builds use the no-op one.
namespace calm::platform {

// Mix quietly with the player's music and respect the silent switch.
void configureAudioSession();

// Light haptic feedback for a GameSession feedback event.
void haptic(const QString& event);

} // namespace calm::platform
