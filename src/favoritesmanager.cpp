#include "favoritesmanager.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {

QJsonObject toJson(const ServerInfo &s)
{
    QJsonObject o;
    o["address"] = s.address;
    o["port"] = s.port;
    o["hostname"] = s.hostname;
    o["nickname"] = s.nickname;
    o["savedPassword"] = s.savedPassword;
    o["rconPassword"] = s.rconPassword;
    return o;
}

ServerInfo fromJson(const QJsonObject &o)
{
    ServerInfo s;
    s.address = o.value("address").toString();
    s.port = static_cast<quint16>(o.value("port").toInt(7777));
    s.hostname = o.value("hostname").toString();
    s.nickname = o.value("nickname").toString();
    s.savedPassword = o.value("savedPassword").toString();
    s.rconPassword = o.value("rconPassword").toString();
    return s;
}

} // namespace

FavoritesManager::FavoritesManager(QObject *parent)
    : QObject(parent)
{
    load();
}

QString FavoritesManager::filePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QLatin1String("/favorites.json");
}

bool FavoritesManager::isFavorite(const QString &address, quint16 port) const
{
    for (const ServerInfo &s : m_favorites) {
        if (s.address == address && s.port == port)
            return true;
    }
    return false;
}

void FavoritesManager::addFavorite(const ServerInfo &info)
{
    if (isFavorite(info.address, info.port))
        return;
    m_favorites.append(info);
    save();
}

void FavoritesManager::removeFavorite(const QString &address, quint16 port)
{
    for (int i = 0; i < m_favorites.size(); ++i) {
        if (m_favorites.at(i).address == address && m_favorites.at(i).port == port) {
            m_favorites.removeAt(i);
            save();
            return;
        }
    }
}

void FavoritesManager::pushRecent(const ServerInfo &info, int maxEntries)
{
    for (int i = 0; i < m_recent.size(); ++i) {
        if (m_recent.at(i).address == info.address && m_recent.at(i).port == info.port) {
            m_recent.removeAt(i);
            break;
        }
    }
    m_recent.prepend(info);
    while (m_recent.size() > maxEntries)
        m_recent.removeLast();
    save();
}

void FavoritesManager::load()
{
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();

    m_favorites.clear();
    for (const QJsonValue &v : root.value("favorites").toArray())
        m_favorites.append(fromJson(v.toObject()));

    m_recent.clear();
    for (const QJsonValue &v : root.value("recent").toArray())
        m_recent.append(fromJson(v.toObject()));
}

void FavoritesManager::save() const
{
    QJsonArray favArr;
    for (const ServerInfo &s : m_favorites)
        favArr.append(toJson(s));

    QJsonArray recentArr;
    for (const ServerInfo &s : m_recent)
        recentArr.append(toJson(s));

    QJsonObject root;
    root["favorites"] = favArr;
    root["recent"] = recentArr;

    QFile f(filePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}
