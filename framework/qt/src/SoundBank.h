#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QRandomGenerator>
#include <QSoundEffect>
#include <QVariantList>
#include <QVector>

// Plays the framework's soft feedback sounds for GameSession::feedback events.
// Each event has several pre-loaded variants that rotate, with a little volume
// variation and a short cooldown so rapid input never stacks into noise.
// Sounds come in themes (water, wind chimes, ...), each a folder of the same
// 17 files made by tools/make_sounds.py.
class SoundBank : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    // [{ id, name }] in display order
    Q_PROPERTY(QVariantList themes READ themes CONSTANT)

public:
    explicit SoundBank(QObject* parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    qreal volume() const { return m_volume; }
    void setVolume(qreal volume);
    QString theme() const { return m_theme; }
    void setTheme(const QString& theme);
    QVariantList themes() const;
    // Theme ids in display order; the first is the default.
    static QStringList themeIds();

    // event: tap, soft, erase, error, complete (step picks the note), win
    Q_INVOKABLE void play(const QString& event, int step = 0);
    // Plays a short sample of the current theme, waiting for it to load if a
    // theme was only just selected.
    Q_INVOKABLE void preview();

signals:
    void enabledChanged();
    void volumeChanged();
    void themeChanged();

private:
    struct Event {
        QStringList files;
        QVector<QSoundEffect*> variants;
        int next = 0;
        qint64 lastPlayed = -1000;
        qreal gain = 1.0;
    };

    void add(const QString& event, const QStringList& files, qreal gain);
    void loadTheme();
    void playEffect(QSoundEffect* effect, qreal gain);

    QHash<QString, Event> m_events;
    QElapsedTimer m_clock;
    qint64 m_lastAny = -1000;
    bool m_enabled = true;
    qreal m_volume = 0.7;
    QString m_theme = themeIds().constFirst();
};
