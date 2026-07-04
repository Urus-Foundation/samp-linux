#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QDateTime>

#include "serverinfo.h"

/* Implements the public SA:MP/open.mp UDP server-query protocol.
 *
 * Supported opcodes:
 *   'i'  server info  — hostname, gamemode, language, players, passworded
 *   'r'  rules        — dynamic key-value pairs (gravity, weather, version, …)
 *
 * Disk cache:
 *   Results are persisted to QStandardPaths::CacheLocation as
 *   server_<ip>_<port>.json with a configurable TTL (default 45 s).
 *   If a valid cached entry exists, no UDP packet is sent.
 *
 * Threading:
 *   All methods must be called from the owning thread (Qt main thread).
 */
class SampQuery : public QObject
{
    Q_OBJECT
public:
    explicit SampQuery(QObject *parent = nullptr);

    /* Queue a live 'i' query.  Delivers result via resultReady(). */
    void queryInfo(const QString &host, quint16 port);

    /* Queue an 'r' rules query.  Delivers result via rulesReady(). */
    void queryRules(const QString &host, quint16 port);

signals:
    void resultReady(ServerInfo info);
    void rulesReady(ServerInfo info);   /* info.rules populated */

private slots:
    void onReadyRead();
    void onTimeoutTick();

private:
    struct PendingQuery {
        QString       host;
        quint16       port    = 0;
        char          opcode  = 'i';
        QHostAddress  address;
        QElapsedTimer elapsed;
    };

    static QByteArray buildPacket(const QHostAddress &addr, quint16 port, char opcode);
    static QString    keyFor(const QString &host, quint16 port, char opcode);
    static QString    readPascalString(QDataStream &stream);

    void dispatchQuery(const QString &host, quint16 port,
                       const QHostAddress &resolved, char opcode);
    void emitOffline(const QString &host, quint16 port);

    /* Disk cache helpers */
    static QString   cacheDir();
    static QString   cachePath(const QString &host, quint16 port);
    bool             loadFromCache(const QString &host, quint16 port, ServerInfo &out) const;
    void             saveToCache(const ServerInfo &info) const;

    QUdpSocket  *m_socket;
    QTimer       m_timeoutTimer;
    QHash<QString, PendingQuery> m_pending;  /* key = "host:port:opcode" */

    static constexpr int kTimeoutMs              = 2500;
    static constexpr int kTimeoutCheckIntervalMs = 250;
    static constexpr quint32 kMaxStringLen       = 512;
    static constexpr int kCacheTtlSecs           = 45;
};
