#pragma once

#include <QString>
#include <QMetaType>

// Represents a single SA:MP / open.mp server entry, whether it came from
// the master server list, the favorites file, or a direct-connect request.
struct ServerInfo {
    QString address;          // hostname or IP
    quint16 port = 7777;

    QString hostname;         // server name (from query 'i')
    QString gamemode;
    QString language;
    QString version;

    quint16 players = 0;
    quint16 maxPlayers = 0;
    qint64 pingMs = -1;       // -1 = unknown / not queried yet
    bool passworded = false;
    bool online = false;      // false until first successful query
    bool queried = false;     // true once a query attempt has completed (success or fail)

    QString nickname;         // per-favorite override, empty = use global nickname
    QString savedPassword;    // per-favorite saved server password (optional)
    QString rconPassword;     // per-favorite saved RCON password (optional, reference only)

    QString key() const { return address + QLatin1Char(':') + QString::number(port); }
    QString displayAddress() const { return address + QLatin1Char(':') + QString::number(port); }
};

Q_DECLARE_METATYPE(ServerInfo)
