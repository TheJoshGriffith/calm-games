#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

// Small versioned JSON documents in the app's data folder, one per key.
// Used by GameSession for autosave; also exposed to QML.
class SaveStore : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static constexpr int FormatVersion = 1;

    using QObject::QObject;

    static QString directory();
    static bool exists(const QString& key);
    static QVariantMap load(const QString& key);
    static bool save(const QString& key, const QVariantMap& data);
    static void remove(const QString& key);

    Q_INVOKABLE bool has(const QString& key) const { return exists(key); }
    Q_INVOKABLE QVariantMap read(const QString& key) const { return load(key); }
    Q_INVOKABLE bool write(const QString& key, const QVariantMap& data) const { return save(key, data); }
    Q_INVOKABLE void erase(const QString& key) const { remove(key); }
};
