#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>

// What the system draws around the app window: on iOS the bands behind the
// status bar and home indicator. AppWindow keeps them in the theme's colours;
// elsewhere this does nothing.
class SystemBars : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // `style` is "light" or "dark" for a theme the player picked, or "" to
    // follow the system.
    Q_INVOKABLE void update(const QColor& background, const QString& style);
};
