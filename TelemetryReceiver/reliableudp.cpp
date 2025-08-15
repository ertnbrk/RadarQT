#include "reliableudp.h"
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QDebug>
#include <algorithm>
#include <cstdio>

// ReliableUdpReceiver Implementation
ReliableUdpReceiver::ReliableUdpReceiver(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
    , m_timeoutTimer(new QTimer(this))
    , m_cleanupTimer(new QTimer(this))
    , m_expectedSequenceNumber(1)
    , m_lastValidSequenceNumber(0)
    , m_interpolationEnabled(true)
    , m_maxBufferSize(1000)
    , m_packetTimeoutMs(5000)
    , m_packetsReceived(0)
    , m_packetsLost(0)
    , m_packetsInterpolated(0)
    , m_acksSent(0)
    , m_listeningPort(12345)
    , m_isListening(false)
{
    // Setup timers
    m_timeoutTimer->setInterval(1000); // Check every second
    m_cleanupTimer->setInterval(10000); // Cleanup every 10 seconds
    
    connect(m_socket, &QUdpSocket::readyRead, this, &ReliableUdpReceiver::processPendingDatagrams);
    connect(m_timeoutTimer, &QTimer::timeout, this, &ReliableUdpReceiver::checkForMissingPackets);
    connect(m_cleanupTimer, &QTimer::timeout, this, &ReliableUdpReceiver::cleanupOldPackets);
}

ReliableUdpReceiver::~ReliableUdpReceiver()
{
    stopListening();
}

bool ReliableUdpReceiver::startListening(quint16 port)
{
    QMutexLocker locker(&m_socketLock);
    
    if (m_isListening) {
        return true;
    }
    
    m_listeningPort = port;
    bool success = m_socket->bind(QHostAddress::Any, port);
    
    if (success) {
        m_isListening = true;
        m_timeoutTimer->start();
        m_cleanupTimer->start();
        emit connectionStatusChanged(true);
        qDebug() << "ReliableUDP: Listening on port" << port;
        printf("ReliableUDP: Successfully listening on port %d\n", port);
        fflush(stdout);
    } else {
        qWarning() << "ReliableUDP: Failed to bind to port" << port << ":" << m_socket->errorString();
        printf("ReliableUDP: FAILED to bind to port %d: %s\n", port, m_socket->errorString().toStdString().c_str());
        fflush(stdout);
    }
    
    return success;
}

void ReliableUdpReceiver::stopListening()
{
    QMutexLocker locker(&m_socketLock);
    
    if (m_isListening) {
        m_timeoutTimer->stop();
        m_cleanupTimer->stop();
        m_socket->close();
        m_isListening = false;
        emit connectionStatusChanged(false);
        qDebug() << "ReliableUDP: Stopped listening";
    }
}

bool ReliableUdpReceiver::isListening() const
{
    return m_isListening;
}

void ReliableUdpReceiver::processPendingDatagrams()
{
    QMutexLocker socketLocker(&m_socketLock);
    
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        
        if (datagram.isValid()) {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(datagram.data(), &parseError);
            
            if (parseError.error != QJsonParseError::NoError) {
                qWarning() << "ReliableUDP: JSON parse error:" << parseError.errorString();
                continue;
            }
            
            QJsonObject obj = doc.object();
            
            // Check if this is an ACK packet (for sender)
            if (obj.contains("type") && obj["type"].toString() == "ACK") {
                // This is handled by the sender, ignore in receiver
                continue;
            }
            
            // Parse telemetry packet
            TelemetryPacket packet = TelemetryPacket::fromJson(obj);
            printf("ReliableUDP: Received packet seq=%d, lat=%f, lon=%f\n", packet.sequenceNumber, packet.latitude, packet.longitude);
            fflush(stdout);
            
            // Send ACK if requested
            if (packet.needsAck) {
                sendAck(packet.sequenceNumber, datagram.senderAddress(), datagram.senderPort());
            }
            
            // Process the packet
            processReceivedPacket(packet);
        }
    }
}

void ReliableUdpReceiver::sendAck(quint32 sequenceNumber, const QHostAddress &sender, quint16 senderPort)
{
    AckPacket ack;
    ack.sequenceNumber = sequenceNumber;
    ack.timestamp = QDateTime::currentDateTime();
    
    QJsonDocument doc(ack.toJson());
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    QMutexLocker locker(&m_socketLock);
    qint64 sent = m_socket->writeDatagram(data, sender, senderPort);
    
    if (sent != -1) {
        m_acksSent++;
        qDebug() << "ReliableUDP: Sent ACK for sequence" << sequenceNumber;
    } else {
        qWarning() << "ReliableUDP: Failed to send ACK:" << m_socket->errorString();
    }
}

void ReliableUdpReceiver::processReceivedPacket(const TelemetryPacket &packet)
{
    QWriteLocker locker(&m_dataLock);
    
    m_packetsReceived++;
    
    // Update last valid packet
    if (packet.sequenceNumber >= m_lastValidSequenceNumber) {
        m_lastValidSequenceNumber = packet.sequenceNumber;
        m_lastValidPacket = packet;
    }
    
    // For now, just emit every packet immediately to avoid ordering issues
    emit telemetryDataReceived(packet);
    
    // Update expected sequence for next packet
    if (packet.sequenceNumber >= m_expectedSequenceNumber) {
        m_expectedSequenceNumber = packet.sequenceNumber + 1;
    }
    
    updateStatistics();
}

void ReliableUdpReceiver::checkForMissingPackets()
{
    QWriteLocker locker(&m_dataLock);
    
    // Check for packets that should have arrived by now
    QDateTime cutoffTime = QDateTime::currentDateTime().addMSecs(-m_packetTimeoutMs);
    
    // Find missing packets in the expected range
    for (quint32 seq = m_expectedSequenceNumber; seq < m_lastValidSequenceNumber; ++seq) {
        if (!m_receivedPackets.contains(seq)) {
            // This packet is missing
            m_packetsLost++;
            
            if (m_interpolationEnabled) {
                // Try to interpolate
                TelemetryPacket interpolated = interpolatePacket(seq);
                emit telemetryDataReceived(interpolated);
                m_packetsInterpolated++;
                qDebug() << "ReliableUDP: Interpolated packet" << seq;
            } else {
                // Use last valid packet
                TelemetryPacket lastPacket = m_lastValidPacket;
                lastPacket.sequenceNumber = seq;
                lastPacket.timestamp = QDateTime::currentDateTime();
                emit telemetryDataReceived(lastPacket);
                qDebug() << "ReliableUDP: Used last valid for packet" << seq;
            }
            
            m_expectedSequenceNumber = seq + 1;
        }
    }
    
    updateStatistics();
}

TelemetryPacket ReliableUdpReceiver::interpolatePacket(quint32 sequenceNumber)
{
    // Find nearest packets before and after
    TelemetryPacket beforePacket, afterPacket;
    bool hasBefore = false, hasAfter = false;
    
    // Look for packet before
    for (quint32 i = sequenceNumber - 1; i > 0 && i > sequenceNumber - 10; --i) {
        if (m_receivedPackets.contains(i)) {
            beforePacket = m_receivedPackets[i];
            hasBefore = true;
            break;
        }
    }
    
    // Look for packet after
    for (quint32 i = sequenceNumber + 1; i < sequenceNumber + 10; ++i) {
        if (m_receivedPackets.contains(i)) {
            afterPacket = m_receivedPackets[i];
            hasAfter = true;
            break;
        }
    }
    
    TelemetryPacket interpolated;
    interpolated.sequenceNumber = sequenceNumber;
    interpolated.timestamp = QDateTime::currentDateTime();
    interpolated.status = "INTERPOLATED";
    
    if (hasBefore && hasAfter) {
        // Linear interpolation
        double factor = double(sequenceNumber - beforePacket.sequenceNumber) / 
                       double(afterPacket.sequenceNumber - beforePacket.sequenceNumber);
        
        interpolated.latitude = beforePacket.latitude + 
                               factor * (afterPacket.latitude - beforePacket.latitude);
        interpolated.longitude = beforePacket.longitude + 
                                factor * (afterPacket.longitude - beforePacket.longitude);
        interpolated.speed = beforePacket.speed + 
                            factor * (afterPacket.speed - beforePacket.speed);
    } else if (hasBefore) {
        // Use previous packet data
        interpolated.latitude = beforePacket.latitude;
        interpolated.longitude = beforePacket.longitude;
        interpolated.speed = beforePacket.speed;
    } else if (hasAfter) {
        // Use next packet data
        interpolated.latitude = afterPacket.latitude;
        interpolated.longitude = afterPacket.longitude;
        interpolated.speed = afterPacket.speed;
    } else {
        // Use last valid packet
        interpolated.latitude = m_lastValidPacket.latitude;
        interpolated.longitude = m_lastValidPacket.longitude;
        interpolated.speed = m_lastValidPacket.speed;
    }
    
    return interpolated;
}

void ReliableUdpReceiver::cleanupOldPackets()
{
    QWriteLocker locker(&m_dataLock);
    
    // Remove packets older than buffer size
    if (m_receivedPackets.size() > m_maxBufferSize) {
        QList<quint32> keys = m_receivedPackets.keys();
        std::sort(keys.begin(), keys.end());
        
        int toRemove = m_receivedPackets.size() - m_maxBufferSize;
        for (int i = 0; i < toRemove; ++i) {
            m_receivedPackets.remove(keys[i]);
        }
        
        qDebug() << "ReliableUDP: Cleaned up" << toRemove << "old packets";
    }
}

void ReliableUdpReceiver::updateStatistics()
{
    emit statisticsUpdated();
}

double ReliableUdpReceiver::getPacketLossRate() const
{
    int total = m_packetsReceived + m_packetsLost;
    return total > 0 ? (double(m_packetsLost) / total) * 100.0 : 0.0;
}

// ReliableUdpSender Implementation
ReliableUdpSender::ReliableUdpSender(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
    , m_timeoutTimer(new QTimer(this))
    , m_targetPort(12345)
    , m_nextSequenceNumber(1)
    , m_ackTimeoutMs(3000)
    , m_maxRetransmissions(3)
    , m_reliabilityEnabled(true)
    , m_packetsSent(0)
    , m_acksReceived(0)
    , m_retransmissions(0)
{
    m_timeoutTimer->setInterval(1000); // Check timeouts every second
    
    connect(m_socket, &QUdpSocket::readyRead, this, &ReliableUdpSender::processIncomingAcks);
    connect(m_timeoutTimer, &QTimer::timeout, this, &ReliableUdpSender::checkForTimeouts);
    
    m_timeoutTimer->start();
}

ReliableUdpSender::~ReliableUdpSender()
{
    m_timeoutTimer->stop();
}

void ReliableUdpSender::setTarget(const QHostAddress &address, quint16 port)
{
    m_targetAddress = address;
    m_targetPort = port;
    qDebug() << "ReliableUDP Sender: Target set to" << address.toString() << ":" << port;
}

void ReliableUdpSender::sendTelemetryData(const TelemetryPacket &packet)
{
    QMutexLocker pendingLocker(&m_pendingLock);
    
    TelemetryPacket sendPacket = packet;
    sendPacket.sequenceNumber = m_nextSequenceNumber++;
    sendPacket.timestamp = QDateTime::currentDateTime();
    sendPacket.needsAck = m_reliabilityEnabled;
    
    QJsonDocument doc(sendPacket.toJson());
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    {
        QMutexLocker socketLocker(&m_socketLock);
        qint64 sent = m_socket->writeDatagram(data, m_targetAddress, m_targetPort);
        
        if (sent != -1) {
            m_packetsSent++;
            qDebug() << "ReliableUDP: Sent packet" << sendPacket.sequenceNumber;
            printf("ReliableUDP: Sent packet seq=%d to %s:%d (%lld bytes)\n", sendPacket.sequenceNumber, 
                   m_targetAddress.toString().toStdString().c_str(), m_targetPort, sent);
            fflush(stdout);
        } else {
            qWarning() << "ReliableUDP: Failed to send packet:" << m_socket->errorString();
            printf("ReliableUDP: FAILED to send packet: %s\n", m_socket->errorString().toStdString().c_str());
            fflush(stdout);
            return;
        }
    }
    
    // If reliability is enabled, track this packet for ACK
    if (m_reliabilityEnabled) {
        PendingPacket pending;
        pending.packet = sendPacket;
        pending.sentTime = QDateTime::currentDateTime();
        pending.retransmissionCount = 0;
        
        m_pendingAcks[sendPacket.sequenceNumber] = pending;
    }
    
    emit statisticsUpdated();
}

void ReliableUdpSender::processIncomingAcks()
{
    QMutexLocker socketLocker(&m_socketLock);
    
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        
        if (datagram.isValid()) {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(datagram.data(), &parseError);
            
            if (parseError.error != QJsonParseError::NoError) {
                continue;
            }
            
            QJsonObject obj = doc.object();
            
            // Check if this is an ACK packet
            if (obj.contains("type") && obj["type"].toString() == "ACK") {
                AckPacket ack = AckPacket::fromJson(obj);
                
                QMutexLocker pendingLocker(&m_pendingLock);
                if (m_pendingAcks.contains(ack.sequenceNumber)) {
                    m_pendingAcks.remove(ack.sequenceNumber);
                    m_acksReceived++;
                    emit ackReceived(ack.sequenceNumber);
                    qDebug() << "ReliableUDP: Received ACK for packet" << ack.sequenceNumber;
                }
            }
        }
    }
    
    emit statisticsUpdated();
}

void ReliableUdpSender::checkForTimeouts()
{
    QMutexLocker locker(&m_pendingLock);
    
    QDateTime cutoffTime = QDateTime::currentDateTime().addMSecs(-m_ackTimeoutMs);
    QList<quint32> timedOutPackets;
    
    for (auto it = m_pendingAcks.begin(); it != m_pendingAcks.end(); ++it) {
        if (it.value().sentTime < cutoffTime) {
            timedOutPackets.append(it.key());
        }
    }
    
    for (quint32 seq : timedOutPackets) {
        PendingPacket &pending = m_pendingAcks[seq];
        
        if (pending.retransmissionCount < m_maxRetransmissions) {
            // Retransmit
            retransmitPacket(seq);
        } else {
            // Give up
            m_pendingAcks.remove(seq);
            emit packetTimeout(seq);
            qWarning() << "ReliableUDP: Packet" << seq << "timed out after" << m_maxRetransmissions << "retries";
        }
    }
}

void ReliableUdpSender::retransmitPacket(quint32 sequenceNumber)
{
    if (!m_pendingAcks.contains(sequenceNumber)) {
        return;
    }
    
    PendingPacket &pending = m_pendingAcks[sequenceNumber];
    pending.retransmissionCount++;
    pending.sentTime = QDateTime::currentDateTime();
    
    QJsonDocument doc(pending.packet.toJson());
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    QMutexLocker socketLocker(&m_socketLock);
    qint64 sent = m_socket->writeDatagram(data, m_targetAddress, m_targetPort);
    
    if (sent != -1) {
        m_retransmissions++;
        qDebug() << "ReliableUDP: Retransmitted packet" << sequenceNumber 
                 << "(attempt" << pending.retransmissionCount << ")";
    } else {
        qWarning() << "ReliableUDP: Failed to retransmit packet:" << m_socket->errorString();
    }
    
    emit statisticsUpdated();
}