#include "serverlistmodel.h"
#include <QFont>
#include <QColor>
#include <QIcon>

ServerListModel::ServerListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ServerListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_servers.size();
}

int ServerListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant ServerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_servers.size())
        return QVariant();

    const ServerInfo &s = m_servers.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case ColLock:
            return s.passworded ? QVariant(QIcon(QStringLiteral(":/icons/lock.svg"))) : QString();
        case ColName:
            return s.hostname.isEmpty() ? tr("(unknown)") : s.hostname;
        case ColMode:
            return s.gamemode;
        case ColPlayers:
            if (!s.queried)
                return tr("...");
            if (!s.online)
                return tr("offline");
            return QString("%1/%2").arg(s.players).arg(s.maxPlayers);
        case ColPing:
            if (!s.queried)
                return tr("...");
            if (!s.online || s.pingMs < 0)
                return QStringLiteral("-");
            return QString::number(s.pingMs) + " ms";
        case ColAddress:
            return s.displayAddress();
        default:
            return QVariant();
        }
    }

    if (role == Qt::DecorationRole && index.column() == ColLock) {
        return s.passworded ? QIcon(QStringLiteral(":/icons/lock.svg")) : QVariant();
    }

    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColPlayers || index.column() == ColPing || index.column() == ColLock)
            return int(Qt::AlignCenter | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole) {
        if (!s.queried)
            return QColor("#8a8f9c");
        if (!s.online)
            return QColor("#6b6f78");
        if (index.column() == ColPing) {
            if (s.pingMs < 100) return QColor("#5fd68a");
            if (s.pingMs < 200) return QColor("#e0c34d");
            return QColor("#ff8a6b");
        }
    }

    if (role == Qt::ToolTipRole) {
        if (index.column() == ColLock)
            return s.passworded ? tr("Password protected") : tr("Open server");
        if (index.column() == ColName)
            return s.hostname;
    }

    if (role == Qt::UserRole) {
        return QVariant::fromValue(s);
    }

    return QVariant();
}

QVariant ServerListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case ColLock: return QString();
    case ColName: return tr("Server Name");
    case ColMode: return tr("Gamemode");
    case ColPlayers: return tr("Players");
    case ColPing: return tr("Ping");
    case ColAddress: return tr("Address");
    default: return QVariant();
    }
}

Qt::ItemFlags ServerListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void ServerListModel::setServers(const QVector<ServerInfo> &servers)
{
    beginResetModel();
    m_servers = servers;
    rebuildIndex();
    endResetModel();
}

void ServerListModel::clear()
{
    beginResetModel();
    m_servers.clear();
    m_keyToRow.clear();
    endResetModel();
}

void ServerListModel::rebuildIndex()
{
    m_keyToRow.clear();
    for (int i = 0; i < m_servers.size(); ++i)
        m_keyToRow.insert(m_servers.at(i).key(), i);
}

int ServerListModel::indexOfKey(const QString &key) const
{
    return m_keyToRow.value(key, -1);
}

void ServerListModel::upsertServer(const ServerInfo &info)
{
    const int row = m_keyToRow.value(info.key(), -1);
    if (row >= 0 && row < m_servers.size()) {
        m_servers[row] = info;
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        return;
    }

    const int newRow = m_servers.size();
    beginInsertRows(QModelIndex(), newRow, newRow);
    m_servers.append(info);
    m_keyToRow.insert(info.key(), newRow);
    endInsertRows();
}

void ServerListModel::removeAt(int row)
{
    if (row < 0 || row >= m_servers.size())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_servers.removeAt(row);
    rebuildIndex();
    endRemoveRows();
}

const ServerInfo &ServerListModel::at(int row) const
{
    static ServerInfo empty;
    if (row < 0 || row >= m_servers.size())
        return empty;
    return m_servers.at(row);
}
