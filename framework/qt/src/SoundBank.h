#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QRandomGenerator>
#include <QSoundEffect>
#include <QVector>

// Plays the framework's soft feedback sounds for GameSession::feedback events.
// Each event has several pre-loaded variants that rotate, with a little volume
// variation and a short cooldown so rapid input never stacks into noise.
class SoundBank : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit SoundBank(QObject* parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    qreal volume() const { return m_volume; }
    void setVolume(qreal volume);

    // event: tap, soft, erase, error, complete (step picks the note), win
    Q_INVOKABLE void play(const QString& event, int step = 0);

signals:
    void enabledChanged();
    void volumeChanged();

private:
    struct Event {
        QVector<QSoundEffect*> variants;
        int next = 0;
        qint64 lastPlayed = -1000;
        qreal gain = 1.0;
    };

    void add(const QString& event, const QStringList& files, qreal gain);

    QHash<QString, Event> m_events;
    QElapsedTimer m_clock;
    bool m_enabled = true;
    qreal m_volume = 0.7;
};
