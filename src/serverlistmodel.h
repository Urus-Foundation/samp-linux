#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include "serverinfo.h"

class ServerListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        ColLock = 0,
        ColName,
        ColMode,
        ColPlayers,
        ColPing,
        ColAddress,
        ColumnCount
    };

    explicit ServerListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setServers(const QVector<ServerInfo> &servers);
    void clear();
    void upsertServer(const ServerInfo &info); // insert or update by key()
    void removeAt(int row);

    const ServerInfo &at(int row) const;
    QVector<ServerInfo> all() const { return m_servers; }
    int indexOfKey(const QString &key) const;

private:
    QVector<ServerInfo> m_servers;
    QHash<QString, int> m_keyToRow;

    void rebuildIndex();
};
