#include "serverlistmodel.h"
#include "helper.h"

#include <QIcon>
#include <QColor>

ServerListModel::ServerListModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

// ---------------------------------------------------------------------------
// QAbstractTableModel interface
// ---------------------------------------------------------------------------

int ServerListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_servers.size();
}

int ServerListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ServerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_servers.size())
        return {};

    const ServerInfo &s   = m_servers.at(index.row());
    const int         col = index.column();

    switch (role) {

    // --- Display text -------------------------------------------------------
    case Qt::DisplayRole:
        switch (col) {
        case ColLock:
            // No text in the lock column; icon is provided via DecorationRole.
            return {};
        case ColName:
            return s.hostname.isEmpty() ? tr("(unknown)") : s.hostname;
        case ColMode:
            return s.gamemode;
        case ColPlayers:
            if (!s.queried) return tr("...");
            if (!s.online)  return tr("offline");
            return QStringLiteral("%1/%2").arg(s.players).arg(s.maxPlayers);
        case ColPing:
            if (!s.queried)               return tr("...");
            if (!s.online || s.pingMs < 0) return QStringLiteral("-");
            return QString::number(s.pingMs) + QLatin1String(" ms");
        case ColAddress:
            return s.displayAddress();
        default:
            return {};
        }

    // --- Icon ---------------------------------------------------------------
    case Qt::DecorationRole:
        if (col == ColLock)
            return s.passworded ? getIcon(ICON_PASSWORD) : getIcon(ICON_UNPASSWORD);
        return {};

    // --- Numeric sort key (Qt::UserRole + 1) --------------------------------
    case Qt::UserRole + 1:
        switch (col) {
        case ColLock:
            return s.passworded ? 1 : 0;
        case ColName:
            return s.hostname.isEmpty() ? tr("(unknown)") : s.hostname;
        case ColMode:
            return s.gamemode;
        case ColPlayers:
            // Offline / not-yet-queried servers sort below any live server.
            return (s.queried && s.online)
                       ? QVariant::fromValue(qint64(s.players))
                       : QVariant::fromValue(qint64(-1));
        case ColPing:
            return (s.queried && s.online && s.pingMs >= 0)
                       ? QVariant::fromValue(qint64(s.pingMs))
                       : QVariant::fromValue(qint64(INT64_MAX));
        case ColAddress:
            return s.displayAddress();
        default:
            return {};
        }

    // --- Raw ServerInfo object (Qt::UserRole) --------------------------------
    case Qt::UserRole:
        return QVariant::fromValue(s);

    // --- Alignment -----------------------------------------------------------
    case Qt::TextAlignmentRole:
        if (col == ColLock || col == ColPlayers || col == ColPing)
            return int(Qt::AlignCenter | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);

    // --- Ping colour-coding --------------------------------------------------
    case Qt::ForegroundRole:
        if (!s.queried)
            return QColor(0x8a, 0x8f, 0x9c);
        if (!s.online)
            return QColor(0x6b, 0x6f, 0x78);
        if (col == ColPing) {
            if (s.pingMs < 100) return QColor(0x5f, 0xd6, 0x8a); // green
            if (s.pingMs < 200) return QColor(0xe0, 0xc3, 0x4d); // yellow
            return QColor(0xff, 0x8a, 0x6b);                      // red
        }
        return {};

    // --- Tooltips ------------------------------------------------------------
    case Qt::ToolTipRole:
        if (col == ColLock)
            return s.passworded ? tr("Password protected") : tr("Open server");
        if (col == ColName)
            return s.hostname;
        return {};

    default:
        return {};
    }
}

QVariant ServerListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColLock:    return {};
    case ColName:    return tr("Server Name");
    case ColMode:    return tr("Gamemode");
    case ColPlayers: return tr("Players");
    case ColPing:    return tr("Ping");
    case ColAddress: return tr("Address");
    default:         return {};
    }
}

Qt::ItemFlags ServerListModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
}

// ---------------------------------------------------------------------------
// Bulk mutations
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Incremental mutations
// ---------------------------------------------------------------------------

void ServerListModel::upsertServer(const ServerInfo &info)
{
    const int row = m_keyToRow.value(info.key(), -1);
    if (row >= 0 && row < m_servers.size()) {
        m_servers[row] = info;
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        return;
    }

    const int newRow = m_servers.size();
    beginInsertRows({}, newRow, newRow);
    m_servers.append(info);
    m_keyToRow.insert(info.key(), newRow);
    endInsertRows();
}

void ServerListModel::removeAt(int row)
{
    if (row < 0 || row >= m_servers.size())
        return;
    beginRemoveRows({}, row, row);
    m_servers.removeAt(row);
    rebuildIndex();
    endRemoveRows();
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void ServerListModel::rebuildIndex()
{
    m_keyToRow.clear();
    m_keyToRow.reserve(m_servers.size());
    for (int i = 0; i < m_servers.size(); ++i)
        m_keyToRow.insert(m_servers.at(i).key(), i);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const ServerInfo &ServerListModel::at(int row) const
{
    static const ServerInfo kEmpty;
    if (row < 0 || row >= m_servers.size())
        return kEmpty;
    return m_servers.at(row);
}

int ServerListModel::indexOfKey(const QString &key) const
{
    return m_keyToRow.value(key, -1);
}
