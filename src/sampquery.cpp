#include "sampquery.h"

#include <QHostInfo>
#include <QDataStream>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

/* ---------------------------------------------------------------------------
 * Construction
 * ---------------------------------------------------------------------------*/

SampQuery::SampQuery(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    m_socket->bind(QHostAddress::AnyIPv4, 0);
    connect(m_socket, &QUdpSocket::readyRead, this, &SampQuery::onReadyRead);

    m_timeoutTimer.setInterval(kTimeoutCheckIntervalMs);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &SampQuery::onTimeoutTick);
    m_timeoutTimer.start();

    /* Ensure cache directory exists. */
    QDir().mkpath(cacheDir());
}

/* ---------------------------------------------------------------------------
 * Static helpers
 * ---------------------------------------------------------------------------*/

QString SampQuery::keyFor(const QString &host, quint16 port, char opcode)
{
    return host % QLatin1Char(':') % QString::number(port)
                % QLatin1Char(':') % QChar::fromLatin1(opcode);
}

QByteArray SampQuery::buildPacket(const QHostAddress &addr, quint16 port, char opcode)
{
    QByteArray packet;
    packet.reserve(11);
    packet.append("SAMP", 4);

    const QStringList octets = addr.toIPv4Address()
                               ? QHostAddress(addr.toIPv4Address()).toString().split(QLatin1Char('.'))
                               : QStringList{"0", "0", "0", "0"};
    for (const QString &o : octets)
        packet.append(static_cast<char>(o.toUInt() & 0xFF));

    packet.append(static_cast<char>(port & 0xFF));
    packet.append(static_cast<char>((port >> 8) & 0xFF));
    packet.append(opcode);
    return packet;
}

/* Decode a length-prefixed (pascal) string.
 * The SA:MP spec says UTF-8, but old servers send Windows-1251 / CP1252.
 * We try UTF-8 first and fall back to Latin-1 on replacement characters. */
QString SampQuery::readPascalString(QDataStream &stream)
{
    quint32 len = 0;
    stream >> len;
    if (stream.status() != QDataStream::Ok || len == 0 || len > kMaxStringLen)
        return {};

    QByteArray buf(static_cast<int>(len), Qt::Uninitialized);
    if (stream.readRawData(buf.data(), buf.size()) != buf.size())
        return {};

    const QString utf8 = QString::fromUtf8(buf);
    if (utf8.contains(QChar::ReplacementCharacter))
        return QString::fromLatin1(buf);

    return utf8;
}

/* ---------------------------------------------------------------------------
 * Disk cache
 * ---------------------------------------------------------------------------*/

QString SampQuery::cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           % QLatin1String("/samplinux");
}

QString SampQuery::cachePath(const QString &host, quint16 port)
{
    /* Sanitise host so colons / dots are safe in filenames. */
    QString safe = host;
    safe.replace(QLatin1Char('.'), QLatin1Char('_'));
    safe.replace(QLatin1Char(':'), QLatin1Char('_'));
    return cacheDir() % QLatin1String("/server_")
           % safe % QLatin1Char('_') % QString::number(port)
           % QLatin1String(".json");
}

bool SampQuery::loadFromCache(const QString &host, quint16 port, ServerInfo &out) const
{
    QFile f(cachePath(host, port));
    if (!f.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;

    const QJsonObject root = doc.object();

    /* TTL check */
    const qint64 ts = root.value(QLatin1String("timestamp")).toVariant().toLongLong();
    if (QDateTime::currentSecsSinceEpoch() - ts > kCacheTtlSecs)
        return false;

    out.address    = root.value(QLatin1String("address")).toString();
    out.port       = static_cast<quint16>(root.value(QLatin1String("port")).toInt());
    out.hostname   = root.value(QLatin1String("hostname")).toString();
    out.gamemode   = root.value(QLatin1String("gamemode")).toString();
    out.language   = root.value(QLatin1String("language")).toString();
    out.version    = root.value(QLatin1String("version")).toString();
    out.players    = static_cast<quint16>(root.value(QLatin1String("players")).toInt());
    out.maxPlayers = static_cast<quint16>(root.value(QLatin1String("maxPlayers")).toInt());
    out.pingMs     = root.value(QLatin1String("pingMs")).toVariant().toLongLong();
    out.passworded = root.value(QLatin1String("passworded")).toBool();
    out.online     = root.value(QLatin1String("online")).toBool();
    out.queried    = true;
    out.rulesQueried = root.contains(QLatin1String("rules"));

    const QJsonObject rulesObj = root.value(QLatin1String("rules")).toObject();
    out.rules.clear();
    for (auto it = rulesObj.constBegin(); it != rulesObj.constEnd(); ++it)
        out.rules.insert(it.key(), it.value().toString());

    return true;
}

void SampQuery::saveToCache(const ServerInfo &info) const
{
    QJsonObject root;
    root[QLatin1String("timestamp")]  = QDateTime::currentSecsSinceEpoch();
    root[QLatin1String("address")]    = info.address;
    root[QLatin1String("port")]       = info.port;
    root[QLatin1String("hostname")]   = info.hostname;
    root[QLatin1String("gamemode")]   = info.gamemode;
    root[QLatin1String("language")]   = info.language;
    root[QLatin1String("version")]    = info.version;
    root[QLatin1String("players")]    = info.players;
    root[QLatin1String("maxPlayers")] = info.maxPlayers;
    root[QLatin1String("pingMs")]     = static_cast<qint64>(info.pingMs);
    root[QLatin1String("passworded")] = info.passworded;
    root[QLatin1String("online")]     = info.online;

    if (info.rulesQueried) {
        QJsonObject rulesObj;
        for (auto it = info.rules.constBegin(); it != info.rules.constEnd(); ++it)
            rulesObj[it.key()] = it.value();
        root[QLatin1String("rules")] = rulesObj;
    }

    QFile f(cachePath(info.address, info.port));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

void SampQuery::queryInfo(const QString &host, quint16 port)
{
    /* Check disk cache first. */
    ServerInfo cached;
    if (loadFromCache(host, port, cached)) {
        /* Emit asynchronously so callers always get the signal after return. */
        QMetaObject::invokeMethod(this, [this, cached]() {
            emit resultReady(cached);
            /* If rules are also cached, emit rulesReady too. */
            if (cached.rulesQueried)
                emit rulesReady(cached);
        }, Qt::QueuedConnection);
        return;
    }

    /* Fast path: already a dotted-decimal IPv4 address. */
    QHostAddress direct;
    if (direct.setAddress(host) && direct.protocol() == QAbstractSocket::IPv4Protocol) {
        dispatchQuery(host, port, direct, 'i');
        return;
    }

    QHostInfo::lookupHost(host, this, [this, host, port](const QHostInfo &info) {
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
            emitOffline(host, port);
            return;
        }
        QHostAddress resolved;
        for (const QHostAddress &a : info.addresses()) {
            if (a.protocol() == QAbstractSocket::IPv4Protocol) {
                resolved = a;
                break;
            }
        }
        if (resolved.isNull()) {
            emitOffline(host, port);
            return;
        }
        dispatchQuery(host, port, resolved, 'i');
    });
}

void SampQuery::queryRules(const QString &host, quint16 port)
{
    /* Check disk cache — rules are stored together with info. */
    ServerInfo cached;
    if (loadFromCache(host, port, cached) && cached.rulesQueried) {
        QMetaObject::invokeMethod(this, [this, cached]() {
            emit rulesReady(cached);
        }, Qt::QueuedConnection);
        return;
    }

    QHostAddress direct;
    if (direct.setAddress(host) && direct.protocol() == QAbstractSocket::IPv4Protocol) {
        dispatchQuery(host, port, direct, 'r');
        return;
    }

    QHostInfo::lookupHost(host, this, [this, host, port](const QHostInfo &info) {
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty())
            return;
        QHostAddress resolved;
        for (const QHostAddress &a : info.addresses()) {
            if (a.protocol() == QAbstractSocket::IPv4Protocol) {
                resolved = a;
                break;
            }
        }
        if (!resolved.isNull())
            dispatchQuery(host, port, resolved, 'r');
    });
}

/* ---------------------------------------------------------------------------
 * Private helpers
 * ---------------------------------------------------------------------------*/

void SampQuery::dispatchQuery(const QString &host, quint16 port,
                              const QHostAddress &resolved, char opcode)
{
    const QString k = keyFor(host, port, opcode);
    PendingQuery pq;
    pq.host    = host;
    pq.port    = port;
    pq.opcode  = opcode;
    pq.address = resolved;
    pq.elapsed.start();
    m_pending.insert(k, pq);
    m_socket->writeDatagram(buildPacket(resolved, port, opcode), resolved, port);
}

void SampQuery::emitOffline(const QString &host, quint16 port)
{
    ServerInfo si;
    si.address = host;
    si.port    = port;
    si.online  = false;
    si.queried = true;
    emit resultReady(si);
}

/* ---------------------------------------------------------------------------
 * Slots
 * ---------------------------------------------------------------------------*/

void SampQuery::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray   datagram(static_cast<int>(m_socket->pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress sender;
        quint16      senderPort = 0;
        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        if (datagram.size() < 11 || !datagram.startsWith("SAMP"))
            continue;

        const char opcode = datagram.at(10);
        if (opcode != 'i' && opcode != 'r')
            continue;

        /* Match pending query: prefer exact address+port, fall back to port. */
        QString foundKey;
        for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
            if (it.value().opcode  == opcode &&
                it.value().port    == senderPort &&
                it.value().address == sender)
            {
                foundKey = it.key();
                break;
            }
        }
        if (foundKey.isEmpty()) {
            for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
                if (it.value().opcode == opcode && it.value().port == senderPort) {
                    foundKey = it.key();
                    break;
                }
            }
        }

        QDataStream stream(datagram.mid(11));
        stream.setByteOrder(QDataStream::LittleEndian);

        /* ---- opcode 'i' ------------------------------------------------- */
        if (opcode == 'i') {
            quint8  passworded = 0;
            quint16 players    = 0;
            quint16 maxPlayers = 0;
            stream >> passworded >> players >> maxPlayers;

            const QString hostname = readPascalString(stream);
            const QString gamemode = readPascalString(stream);
            const QString language = readPascalString(stream);

            ServerInfo si;
            si.port       = senderPort;
            si.online     = true;
            si.queried    = true;
            si.passworded = (passworded != 0);
            si.players    = players;
            si.maxPlayers = maxPlayers;
            si.hostname   = hostname;
            si.gamemode   = gamemode;
            si.language   = language;

            if (!foundKey.isEmpty()) {
                const PendingQuery pq = m_pending.take(foundKey);
                si.address = pq.host;
                si.pingMs  = pq.elapsed.elapsed();
            } else {
                si.address = sender.toString();
            }

            saveToCache(si);
            emit resultReady(si);
            continue;
        }

        /* ---- opcode 'r' ------------------------------------------------- */
        if (opcode == 'r') {
            ServerInfo si;
            si.port      = senderPort;
            si.online    = true;
            si.queried   = true;
            si.rulesQueried = true;

            if (!foundKey.isEmpty()) {
                const PendingQuery pq = m_pending.take(foundKey);
                si.address = pq.host;
                si.pingMs  = pq.elapsed.elapsed();
            } else {
                si.address = sender.toString();
            }

            /* Parse key-value pairs until stream exhausted or error. */
            while (stream.status() == QDataStream::Ok) {
                const QString key = readPascalString(stream);
                if (key.isEmpty())
                    break;
                const QString val = readPascalString(stream);
                si.rules.insert(key, val);
            }

            /* Merge rules into existing cache entry so both 'i' and 'r'
             * data are stored together. */
            ServerInfo cached;
            if (loadFromCache(si.address, si.port, cached)) {
                cached.rules        = si.rules;
                cached.rulesQueried = true;
                saveToCache(cached);
            } else {
                saveToCache(si);
            }

            emit rulesReady(si);
            continue;
        }
    }
}

void SampQuery::onTimeoutTick()
{
    QStringList expired;
    for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
        if (it.value().elapsed.elapsed() > kTimeoutMs)
            expired << it.key();
    }
    for (const QString &k : std::as_const(expired)) {
        const PendingQuery pq = m_pending.take(k);
        if (pq.opcode == 'i')
            emitOffline(pq.host, pq.port);
        /* For 'r' timeouts we silently discard — rules are non-critical. */
    }
}
