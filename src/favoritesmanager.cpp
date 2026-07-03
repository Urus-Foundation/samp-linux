#include "favoritesmanager.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcFavorites, "samplinux.favorites")

// ---------------------------------------------------------------------------
// JSON serialization helpers (file-local)
// ---------------------------------------------------------------------------

namespace {

QJsonObject serverToJson(const ServerInfo &s)
{
    return {
        { "address",       s.address       },
        { "port",          s.port          },
        { "hostname",      s.hostname      },
        { "nickname",      s.nickname      },
        { "savedPassword", s.savedPassword },
        { "rconPassword",  s.rconPassword  },
    };
}

ServerInfo serverFromJson(const QJsonObject &o)
{
    ServerInfo s;
    s.address       = o.value("address").toString();
    s.port          = static_cast<quint16>(o.value("port").toInt(7777));
    s.hostname      = o.value("hostname").toString();
    s.nickname      = o.value("nickname").toString();
    s.savedPassword = o.value("savedPassword").toString();
    s.rconPassword  = o.value("rconPassword").toString();
    return s;
}

QVector<ServerInfo> arrayToServers(const QJsonValue &arrayValue)
{
    QVector<ServerInfo> result;
    for (const QJsonValue &v : arrayValue.toArray()) {
        if (v.isObject())
            result.append(serverFromJson(v.toObject()));
    }
    return result;
}

} // namespace

// ---------------------------------------------------------------------------
// FavoritesManager
// ---------------------------------------------------------------------------

FavoritesManager::FavoritesManager(QObject *parent)
    : QObject(parent)
{
    load();
}

// The old code used "sampqt" as the app data subdirectory (legacy naming).
// We now use the real application name so the path is
// ~/.local/share/samp-linux/favorites.json.
QString FavoritesManager::filePath()
{
    // QStandardPaths::AppDataLocation uses QCoreApplication::applicationName(),
    // which is set to "samp-linux" in main.cpp.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir % QLatin1String("/favorites.json");
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

void FavoritesManager::pushRecent(const ServerInfo &info)
{
    // Remove existing entry for this server so we can put it at the front.
    for (int i = 0; i < m_recent.size(); ++i) {
        if (m_recent.at(i).address == info.address && m_recent.at(i).port == info.port) {
            m_recent.removeAt(i);
            break;
        }
    }
    m_recent.prepend(info);
    while (m_recent.size() > kMaxRecentEntries)
        m_recent.removeLast();
    save();
}

void FavoritesManager::load()
{
    m_favorites.clear();
    m_recent.clear();

    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return; // file simply doesn't exist yet — that's fine

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(lcFavorites) << "favorites.json parse error:" << parseError.errorString()
                               << "— starting with empty lists";
        return;
    }
    if (!doc.isObject()) {
        qCWarning(lcFavorites) << "favorites.json root is not a JSON object — starting with empty lists";
        return;
    }

    const QJsonObject root = doc.object();

    // Version guard: ignore files from future schema versions rather than
    // silently misreading them.
    const int version = root.value("version").toInt(1);
    if (version > kSchemaVersion) {
        qCWarning(lcFavorites) << "favorites.json has schema version" << version
                               << "but we only understand up to" << kSchemaVersion
                               << "— starting with empty lists";
        return;
    }

    m_favorites = arrayToServers(root.value("favorites"));
    m_recent    = arrayToServers(root.value("recent"));
}

void FavoritesManager::save() const
{
    QJsonArray favArray;
    for (const ServerInfo &s : m_favorites)
        favArray.append(serverToJson(s));

    QJsonArray recentArray;
    for (const ServerInfo &s : m_recent)
        recentArray.append(serverToJson(s));

    QJsonObject root;
    root["version"]   = kSchemaVersion;
    root["favorites"] = favArray;
    root["recent"]    = recentArray;

    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcFavorites) << "Could not write favorites.json to" << filePath();
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}
