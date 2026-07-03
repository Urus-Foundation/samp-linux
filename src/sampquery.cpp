#include "sampquery.h"

#include <QHostInfo>
#include <QDataStream>

namespace {
constexpr int kTimeoutMs              = 2500;
constexpr int kTimeoutCheckIntervalMs = 250;
constexpr quint32 kMaxStringLen       = 512;
} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SampQuery::SampQuery(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    m_socket->bind(QHostAddress::AnyIPv4, 0);
    connect(m_socket, &QUdpSocket::readyRead, this, &SampQuery::onReadyRead);

    m_timeoutTimer.setInterval(kTimeoutCheckIntervalMs);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &SampQuery::onTimeoutTick);
    m_timeoutTimer.start();
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

QString SampQuery::keyFor(const QString &host, quint16 port)
{
    return host % QLatin1Char(':') % QString::number(port);
}

QByteArray SampQuery::buildPacket(const QHostAddress &addr, quint16 port, char opcode)
{
    QByteArray packet;
    packet.reserve(11);
    packet.append("SAMP", 4);

    // IPv4 octets, one byte each
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

// Decode a length-prefixed (pascal) string.
// The SA:MP spec says UTF-8, but old servers frequently send Windows-1251 or
// CP1252.  We try UTF-8 first; if it produces replacement characters we fall
// back to Latin-1 so the bytes are preserved visibly rather than silently
// dropped.
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
    // If UTF-8 decoding introduced replacement characters the source bytes
    // are not valid UTF-8 — fall back to Latin-1 (covers CP1252/Windows-1251
    // for ASCII-range characters, at least preserving server names).
    if (utf8.contains(QChar::ReplacementCharacter))
        return QString::fromLatin1(buf);

    return utf8;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SampQuery::queryInfo(const QString &host, quint16 port)
{
    // Fast path: already a dotted-decimal IPv4 address.
    QHostAddress direct;
    if (direct.setAddress(host) && direct.protocol() == QAbstractSocket::IPv4Protocol) {
        dispatchQuery(host, port, direct);
        return;
    }

    // Async DNS resolution; result is queued back to this thread.
    QHostInfo::lookupHost(host, this, [this, host, port](const QHostInfo &info) {
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
            emitOffline(host, port);
            return;
        }

        // Prefer IPv4 since the SA:MP packet format only encodes 4 octets.
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

        dispatchQuery(host, port, resolved);
    });
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SampQuery::dispatchQuery(const QString &host, quint16 port, const QHostAddress &resolved)
{
    const QString k = keyFor(host, port);
    PendingQuery pq;
    pq.host    = host;
    pq.port    = port;
    pq.address = resolved;
    pq.elapsed.start();
    m_pending.insert(k, pq);
    m_socket->writeDatagram(buildPacket(resolved, port, 'i'), resolved, port);
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

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void SampQuery::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray   datagram(static_cast<int>(m_socket->pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress sender;
        quint16      senderPort = 0;
        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        // Minimum valid response: 4 magic + 4 IP + 2 port + 1 opcode = 11 bytes
        if (datagram.size() < 11 || !datagram.startsWith("SAMP"))
            continue;
        if (datagram.at(10) != 'i')
            continue;

        QDataStream stream(datagram.mid(11));
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8  passworded = 0;
        quint16 players    = 0;
        quint16 maxPlayers = 0;
        stream >> passworded >> players >> maxPlayers;

        const QString hostname  = readPascalString(stream);
        const QString gamemode  = readPascalString(stream);
        const QString language  = readPascalString(stream);

        // Match against a pending query.  Try exact address+port first so
        // that two servers on the same port (different IPs) don't collide.
        QString foundKey;
        for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
            if (it.value().port == senderPort && it.value().address == sender) {
                foundKey = it.key();
                break;
            }
        }
        // Fallback: port-only match (NAT or address mismatch cases).
        if (foundKey.isEmpty()) {
            for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
                if (it.value().port == senderPort) {
                    foundKey = it.key();
                    break;
                }
            }
        }

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

        emit resultReady(si);
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
        emitOffline(pq.host, pq.port);
    }
}
