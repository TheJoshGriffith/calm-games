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
    // 0 subtle, 1 gentle, 2 firm
    Q_PROPERTY(int strength READ strength WRITE setStrength NOTIFY strengthChanged)

public:
    using QObject::QObject;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    int strength() const { return m_strength; }
    void setStrength(int strength);

    Q_INVOKABLE void play(const QString& event);

signals:
    void enabledChanged();
    void strengthChanged();

private:
    bool m_enabled = true;
    int m_strength = 1;
};
