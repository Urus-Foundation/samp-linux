#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include "serverinfo.h"

// Persists favorite servers and a small "recently connected" list to a
// JSON file under the user's local app-data directory.
class FavoritesManager : public QObject
{
    Q_OBJECT
public:
    explicit FavoritesManager(QObject *parent = nullptr);

    QVector<ServerInfo> favorites() const { return m_favorites; }
    QVector<ServerInfo> recent() const { return m_recent; }

    bool isFavorite(const QString &address, quint16 port) const;
    void addFavorite(const ServerInfo &info);
    void removeFavorite(const QString &address, quint16 port);

    void pushRecent(const ServerInfo &info, int maxEntries = 15);

    void load();
    void save() const;

private:
    QVector<ServerInfo> m_favorites;
    QVector<ServerInfo> m_recent;

    QString filePath() const;
};
