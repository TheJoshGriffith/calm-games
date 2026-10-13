#include "SaveStore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QString pathFor(const QString& key)
{
    QString safe = key;
    for (QChar& c : safe)
        if (!c.isLetterOrNumber() && c != QLatin1Char('-') && c != QLatin1Char('_'))
            c = QLatin1Char('_');
    return SaveStore::directory() + QLatin1Char('/') + safe + QStringLiteral(".json");
}

} // namespace

QString SaveStore::directory()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/saves");
    QDir().mkpath(dir);
    return dir;
}

bool SaveStore::exists(const QString& key)
{
    return QFile::exists(pathFor(key));
}

QVariantMap SaveStore::load(const QString& key)
{
    QFile file(pathFor(key));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    // Future format changes migrate here, keyed on the stored version.
    if (root.value(QStringLiteral("version")).toInt() != FormatVersion)
        return {};
    return root.value(QStringLiteral("data")).toObject().toVariantMap();
}

bool SaveStore::save(const QString& key, const QVariantMap& data)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), FormatVersion);
    root.insert(QStringLiteral("data"), QJsonObject::fromVariantMap(data));
    QSaveFile file(pathFor(key));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

void SaveStore::remove(const QString& key)
{
    QFile::remove(pathFor(key));
}
