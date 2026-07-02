#include "sampquery.h"

#include <QHostInfo>
#include <QDataStream>

namespace {
constexpr int kTimeoutMs = 2500;
constexpr int kTimeoutCheckIntervalMs = 250;
}

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

QString SampQuery::keyFor(const QString &host, quint16 port)
{
    return host + QLatin1Char(':') + QString::number(port);
}

QByteArray SampQuery::buildPacket(const QHostAddress &addr, quint16 port, char opcode)
{
    QByteArray packet;
    packet.append("SAMP", 4);

    const QHostAddress v4(addr.toIPv4Address());
    const QStringList octets = v4.toString().split(QLatin1Char('.'));
    for (const QString &o : octets)
        packet.append(static_cast<char>(o.toUInt() & 0xFF));

    packet.append(static_cast<char>(port & 0xFF));
    packet.append(static_cast<char>((port >> 8) & 0xFF));
    packet.append(opcode);
    return packet;
}

void SampQuery::queryInfo(const QString &host, quint16 port)
{
    const QString k = keyFor(host, port);

    // Fast path: already a valid IPv4 address.
    QHostAddress direct;
    if (direct.setAddress(host) && direct.protocol() == QAbstractSocket::IPv4Protocol) {
        PendingQuery pq;
        pq.host = host;
        pq.port = port;
        pq.elapsed.start();
        m_pending.insert(k, pq);
        m_socket->writeDatagram(buildPacket(direct, port, 'i'), direct, port);
        return;
    }

    // Otherwise resolve the hostname asynchronously first.
    QHostInfo::lookupHost(host, this, [this, host, port, k](const QHostInfo &info) {
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
            ServerInfo si;
            si.address = host;
            si.port = port;
            si.online = false;
            si.queried = true;
            emit resultReady(si);
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
            ServerInfo si;
            si.address = host;
            si.port = port;
            si.online = false;
            si.queried = true;
            emit resultReady(si);
            return;
        }

        PendingQuery pq;
        pq.host = host;
        pq.port = port;
        pq.elapsed.start();
        m_pending.insert(k, pq);
        m_socket->writeDatagram(buildPacket(resolved, port, 'i'), resolved, port);
    });
}

void SampQuery::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(m_socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        if (datagram.size() < 11 || !datagram.startsWith("SAMP"))
            continue;

        const char opcode = datagram.at(10);
        if (opcode != 'i')
            continue; // we only send 'i' requests, ignore anything else

        const QByteArray payload = datagram.mid(11);
        QDataStream stream(payload);
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8 passworded = 0;
        quint16 players = 0;
        quint16 maxPlayers = 0;
        stream >> passworded >> players >> maxPlayers;

        auto readPascalString = [&stream]() -> QString {
            quint32 len = 0;
            stream >> len;
            if (stream.status() != QDataStream::Ok || len == 0 || len > 512)
                return QString();
            QByteArray buf(int(len), 0);
            const int n = stream.readRawData(buf.data(), int(len));
            if (n != int(len))
                return QString();
            return QString::fromUtf8(buf);
        };

        const QString hostname = readPascalString();
        const QString gamemode = readPascalString();
        const QString language = readPascalString();

        // Match against a pending request. We key by sender port primarily
        // since address may differ slightly (IP vs resolved hostname).
        QString foundKey;
        for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
            if (it.value().port == senderPort) {
                foundKey = it.key();
                break;
            }
        }

        ServerInfo si;
        si.port = senderPort;
        si.online = true;
        si.queried = true;
        si.passworded = (passworded != 0);
        si.players = players;
        si.maxPlayers = maxPlayers;
        si.hostname = hostname;
        si.gamemode = gamemode;
        si.language = language;

        if (!foundKey.isEmpty()) {
            const PendingQuery pq = m_pending.take(foundKey);
            si.address = pq.host;
            si.pingMs = pq.elapsed.elapsed();
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
    for (const QString &k : expired) {
        const PendingQuery pq = m_pending.take(k);
        ServerInfo si;
        si.address = pq.host;
        si.port = pq.port;
        si.online = false;
        si.queried = true;
        emit resultReady(si);
    }
}
