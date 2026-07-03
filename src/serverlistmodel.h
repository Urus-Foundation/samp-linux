#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include <QHash>

#include "serverinfo.h"

// Table model backing both the Internet and Favorites server lists.
//
// Columns:
//   ColLock    — padlock icon when the server is password-protected
//   ColName    — server hostname
//   ColMode    — gamemode string
//   ColPlayers — "current / max" or status string
//   ColPing    — round-trip time in ms, colour-coded via ForegroundRole
//   ColAddress — "host:port"
//
// Sorting:
//   Qt::UserRole + 1 carries a numeric sort key so that proxy models can
//   sort Players and Ping columns numerically (offline servers sort last).
//
// Index consistency:
//   upsertServer() keeps an internal QHash<key→row> so the model never has
//   to scan m_servers linearly.  Call rebuildIndex() any time m_servers is
//   replaced in bulk.
class ServerListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        ColLock    = 0,
        ColName,
        ColMode,
        ColPlayers,
        ColPing,
        ColAddress,
        ColumnCount
    };

    explicit ServerListModel(QObject *parent = nullptr);

    // QAbstractTableModel interface
    int     rowCount   (const QModelIndex &parent = {}) const override;
    int     columnCount(const QModelIndex &parent = {}) const override;
    QVariant data      (const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Bulk replace
    void setServers(const QVector<ServerInfo> &servers);
    void clear();

    // Incremental updates
    void upsertServer(const ServerInfo &info);  // insert or update by key()
    void removeAt(int row);

    // Accessors
    const ServerInfo &at(int row) const;
    QVector<ServerInfo> all() const { return m_servers; }
    int indexOfKey(const QString &key) const;

private:
    QVector<ServerInfo>     m_servers;
    QHash<QString, int>     m_keyToRow;

    void rebuildIndex();
};
