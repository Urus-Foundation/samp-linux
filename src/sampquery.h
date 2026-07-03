#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>

#include "serverinfo.h"

// Implements the public SA:MP/open.mp UDP server-query protocol.
//
// Protocol overview (opcode 'i' — server info):
//   Request:  "SAMP" + 4 bytes (IPv4 octets) + 2 bytes (port LE) + 'i'
//   Response: same 11-byte header + 1 byte passworded + 2 bytes players
//             + 2 bytes maxPlayers + pascal-string hostname
//             + pascal-string gamemode + pascal-string language
//
// String encoding note:
//   The protocol spec says strings are UTF-8, but many legacy SA:MP servers
//   send Windows-1251 or CP1252 bytes without any BOM.  We attempt UTF-8
//   first (via QString::fromUtf8) and fall back to Latin-1 so the bytes are
//   at least displayed rather than dropped.  Callers may apply their own
//   codec if they know the server's region.
//
// Threading note:
//   All methods must be called from the thread that owns this object (the
//   Qt main thread in the current design).  Async DNS lookup callbacks are
//   dispatched back to the owning thread via a captured lambda queued through
//   QHostInfo::lookupHost, so no extra locking is required.
class SampQuery : public QObject
{
    Q_OBJECT
public:
    explicit SampQuery(QObject *parent = nullptr);

    // Queue a live query for the given server.  The result (success or
    // timeout) is always delivered via resultReady(), even on DNS failure.
    void queryInfo(const QString &host, quint16 port);

signals:
    void resultReady(ServerInfo info);

private slots:
    void onReadyRead();
    void onTimeoutTick();

private:
    struct PendingQuery {
        QString      host;
        quint16      port    = 0;
        QHostAddress address;
        QElapsedTimer elapsed;
    };

    static QByteArray buildPacket(const QHostAddress &addr, quint16 port, char opcode);
    static QString    keyFor(const QString &host, quint16 port);

    // Decode a pascal-prefixed string from the stream.  Falls back to
    // Latin-1 when the raw bytes are not valid UTF-8.
    static QString readPascalString(QDataStream &stream);

    void dispatchQuery(const QString &host, quint16 port, const QHostAddress &resolved);
    void emitOffline(const QString &host, quint16 port);

    QUdpSocket             *m_socket;
    QTimer                  m_timeoutTimer;
    QHash<QString, PendingQuery> m_pending;  // key = "host:port"
};
