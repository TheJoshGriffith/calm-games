#pragma once

#include <QObject>
#include <QQmlEngine>

// Light haptic feedback for GameSession::feedback events: implemented on iOS
// (UIKit feedback generators); a no-op on desktop, Android to follow.
class Haptics : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    using QObject::QObject;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    Q_INVOKABLE void play(const QString& event);

signals:
    void enabledChanged();

private:
    bool m_enabled = true;
};
