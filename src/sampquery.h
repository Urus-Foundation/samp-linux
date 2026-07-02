#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>
#include "serverinfo.h"

// Implements the (documented, public) SA:MP/open.mp UDP server-query
// protocol. This is the same protocol every SA:MP-compatible client and
// launcher uses to ask a game server for its name, gamemode and player
// count - it is not related to the game's network traffic itself.
//
// Packet layout for a request:
//   "SAMP" + 4 bytes (IPv4 octets) + 2 bytes (port, little endian) + 1 byte opcode
//
// This class only implements the 'i' (info) opcode, which is all a
// launcher needs for a server browser.
class SampQuery : public QObject
{
    Q_OBJECT
public:
    explicit SampQuery(QObject *parent = nullptr);

    // Queries basic server info (hostname, gamemode, players, ping).
    // Result (success or failure/timeout) is always delivered via resultReady().
    void queryInfo(const QString &host, quint16 port);

signals:
    void resultReady(ServerInfo info);

private slots:
    void onReadyRead();
    void onTimeoutTick();

private:
    struct PendingQuery {
        QString host;
        quint16 port = 0;
        QElapsedTimer elapsed;
    };

    static QByteArray buildPacket(const QHostAddress &addr, quint16 port, char opcode);
    static QString keyFor(const QString &host, quint16 port);

    QUdpSocket *m_socket;
    QTimer m_timeoutTimer;
    QHash<QString, PendingQuery> m_pending; // key = "host:port"
};
