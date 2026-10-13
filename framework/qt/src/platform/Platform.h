#pragma once

#include <QRectF>
#include <QString>

#include <functional>

// Thin platform hooks. Each platform provides its own implementation file;
// desktop and web builds use the generic one.
namespace calm::platform {

// Mix quietly with the player's music and respect the silent switch.
void configureAudioSession();

// Haptic feedback for a GameSession feedback event; `scale` (0..1) is the
// player's chosen strength.
void haptic(const QString& event, qreal scale);

// Offers `text` to the system share sheet: UIActivityViewController on iOS
// (on iPad its popover points at `anchor`, in window coordinates), the Web
// Share API in browsers that have it. Elsewhere, or when the browser refuses,
// it copies `text` to the clipboard instead and calls `copied` (on the web
// possibly later, once the browser has answered).
void share(const QString& text, const QRectF& anchor, const std::function<void()>& copied);

// Puts `text` on the clipboard.
void copyToClipboard(const QString& text);

} // namespace calm::platform
