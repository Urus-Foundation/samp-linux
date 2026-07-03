#pragma once

#include <QString>
#include <QMetaType>

// Represents a single SA:MP / open.mp server entry.
// Sources: master server list, favorites file, or direct-connect request.
struct ServerInfo {
    // Network identity
    QString address;     // hostname or IPv4
    quint16 port = 7777;

    // Fields returned by the 'i' UDP query opcode
    QString hostname;
    QString gamemode;
    QString language;
    QString version;

    quint16 players    = 0;
    quint16 maxPlayers = 0;
    qint64  pingMs     = -1;   // -1 = not yet measured
    bool    passworded = false;

    // Query state
    bool online  = false;  // true after first successful UDP reply
    bool queried = false;  // true once any query attempt has completed

    // Per-favorite overrides (empty = use global setting)
    QString nickname;
    QString savedPassword;
    QString rconPassword;

    // Unique key used in caches and the model index
    QString key()            const { return address % QLatin1Char(':') % QString::number(port); }
    QString displayAddress() const { return key(); }
};

Q_DECLARE_METATYPE(ServerInfo)
