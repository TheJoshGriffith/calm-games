#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSettings>

// App-wide preferences shared by every Calm game, persisted with QSettings.
// Game-specific options live on the game's own session class.
class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "system", "light" or "dark"
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY soundEnabledChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool hapticsEnabled READ hapticsEnabled WRITE setHapticsEnabled NOTIFY hapticsEnabledChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY reduceMotionChanged)

public:
    explicit Settings(QObject* parent = nullptr);

    QString theme() const;
    void setTheme(const QString& theme);
    bool soundEnabled() const;
    void setSoundEnabled(bool enabled);
    qreal volume() const;
    void setVolume(qreal volume);
    bool hapticsEnabled() const;
    void setHapticsEnabled(bool enabled);
    bool reduceMotion() const;
    void setReduceMotion(bool reduce);

signals:
    void themeChanged();
    void soundEnabledChanged();
    void volumeChanged();
    void hapticsEnabledChanged();
    void reduceMotionChanged();

private:
    QSettings m_store;
};
