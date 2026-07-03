#pragma once

#include <QObject>
#include <QVector>
#include <QString>

#include "serverinfo.h"

// Persists the favorites list and a recently-connected list to
// ~/.local/share/samp-linux/favorites.json.
//
// Schema versioning:
//   The JSON root contains a "version" integer.  When an unknown version is
//   encountered the file is treated as empty (no crash, no data corruption).
//   Current version: 1.
//
// Error handling:
//   load() silently ignores missing files.  On parse errors (corrupt JSON,
//   wrong root type) it resets both lists and logs a qWarning so the issue
//   is visible in debug sessions without crashing the launcher.
class FavoritesManager : public QObject
{
    Q_OBJECT
public:
    static constexpr int kSchemaVersion = 1;
    static constexpr int kMaxRecentEntries = 15;

    explicit FavoritesManager(QObject *parent = nullptr);

    QVector<ServerInfo> favorites() const { return m_favorites; }
    QVector<ServerInfo> recent()    const { return m_recent; }

    bool isFavorite(const QString &address, quint16 port) const;
    void addFavorite(const ServerInfo &info);
    void removeFavorite(const QString &address, quint16 port);

    // Prepend info to the recent list, removing duplicates and capping at
    // kMaxRecentEntries.  Saves immediately.
    void pushRecent(const ServerInfo &info);

    void load();
    void save() const;

private:
    QVector<ServerInfo> m_favorites;
    QVector<ServerInfo> m_recent;

    static QString filePath();
};
