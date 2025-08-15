#ifndef RELIABLEUDP_H
#define RELIABLEUDP_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHash>
#include <QReadWriteLock>

struct TelemetryPacket {
    quint32 sequenceNumber;
    QDateTime timestamp;
    double latitude;
    double longitude;
    double speed;
    QString status;
    bool needsAck;
    
    TelemetryPacket() : sequenceNumber(0), latitude(0), longitude(0), speed(0), needsAck(false) {}
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["seq"] = static_cast<qint64>(sequenceNumber);
        obj["timestamp"] = timestamp.toMSecsSinceEpoch();
        obj["latitude"] = latitude;
        obj["longitude"] = longitude;
        obj["speed"] = speed;
        obj["status"] = status;
        obj["needsAck"] = needsAck;
        return obj;
    }
    
    static TelemetryPacket fromJson(const QJsonObject &obj) {
        TelemetryPacket packet;
        packet.sequenceNumber = static_cast<quint32>(obj["seq"].toInt());
        packet.timestamp = QDateTime::fromMSecsSinceEpoch(obj["timestamp"].toVariant().toLongLong());
        packet.latitude = obj["latitude"].toDouble();
        packet.longitude = obj["longitude"].toDouble();
        packet.speed = obj["speed"].toDouble();
        packet.status = obj["status"].toString();
        packet.needsAck = obj["needsAck"].toBool();
        return packet;
    }
};

struct AckPacket {
    quint32 sequenceNumber;
    QDateTime timestamp;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["type"] = "ACK";
        obj["seq"] = static_cast<qint64>(sequenceNumber);
        obj["timestamp"] = timestamp.toMSecsSinceEpoch();
        return obj;
    }
    
    static AckPacket fromJson(const QJsonObject &obj) {
        AckPacket ack;
        ack.sequenceNumber = static_cast<quint32>(obj["seq"].toInt());
        ack.timestamp = QDateTime::fromMSecsSinceEpoch(obj["timestamp"].toVariant().toLongLong());
        return ack;
    }
};

// Thread-safe receiver with buffering
class ReliableUdpReceiver : public QObject
{
    Q_OBJECT

public:
    explicit ReliableUdpReceiver(QObject *parent = nullptr);
    ~ReliableUdpReceiver();
    
    bool startListening(quint16 port = 12345);
    void stopListening();
    bool isListening() const;
    
    // Reliability settings
    void setInterpolationEnabled(bool enabled) { m_interpolationEnabled = enabled; }
    void setMaxBufferSize(int size) { m_maxBufferSize = size; }
    void setPacketTimeoutMs(int timeoutMs) { m_packetTimeoutMs = timeoutMs; }
    
    // Statistics
    int getPacketsReceived() const { return m_packetsReceived; }
    int getPacketsLost() const { return m_packetsLost; }
    int getPacketsInterpolated() const { return m_packetsInterpolated; }
    double getPacketLossRate() const;

signals:
    void telemetryDataReceived(const TelemetryPacket &packet);
    void connectionStatusChanged(bool connected);
    void statisticsUpdated();

private slots:
    void processPendingDatagrams();
    void checkForMissingPackets();
    void cleanupOldPackets();

private:
    void sendAck(quint32 sequenceNumber, const QHostAddress &sender, quint16 senderPort);
    void processReceivedPacket(const TelemetryPacket &packet);
    TelemetryPacket interpolatePacket(quint32 sequenceNumber);
    void updateStatistics();
    
    QUdpSocket *m_socket;
    QTimer *m_timeoutTimer;
    QTimer *m_cleanupTimer;
    
    // Thread safety
    mutable QReadWriteLock m_dataLock;
    QMutex m_socketLock;
    
    // Buffering and reliability
    QHash<quint32, TelemetryPacket> m_receivedPackets;
    QQueue<TelemetryPacket> m_processingQueue;
    quint32 m_expectedSequenceNumber;
    quint32 m_lastValidSequenceNumber;
    TelemetryPacket m_lastValidPacket;
    
    // Settings
    bool m_interpolationEnabled;
    int m_maxBufferSize;
    int m_packetTimeoutMs;
    
    // Statistics
    std::atomic<int> m_packetsReceived;
    std::atomic<int> m_packetsLost;
    std::atomic<int> m_packetsInterpolated;
    std::atomic<int> m_acksSent;
    
    quint16 m_listeningPort;
    bool m_isListening;
};

// Thread-safe sender with reliability
class ReliableUdpSender : public QObject
{
    Q_OBJECT

public:
    explicit ReliableUdpSender(QObject *parent = nullptr);
    ~ReliableUdpSender();
    
    void setTarget(const QHostAddress &address, quint16 port);
    void sendTelemetryData(const TelemetryPacket &packet);
    
    // Reliability settings
    void setAckTimeoutMs(int timeoutMs) { m_ackTimeoutMs = timeoutMs; }
    void setMaxRetransmissions(int maxRetries) { m_maxRetransmissions = maxRetries; }
    void setReliabilityEnabled(bool enabled) { m_reliabilityEnabled = enabled; }

signals:
    void ackReceived(quint32 sequenceNumber);
    void packetTimeout(quint32 sequenceNumber);
    void statisticsUpdated();

private slots:
    void processIncomingAcks();
    void checkForTimeouts();

private:
    void retransmitPacket(quint32 sequenceNumber);
    
    QUdpSocket *m_socket;
    QTimer *m_timeoutTimer;
    
    // Target
    QHostAddress m_targetAddress;
    quint16 m_targetPort;
    
    // Pending acknowledgments
    struct PendingPacket {
        TelemetryPacket packet;
        QDateTime sentTime;
        int retransmissionCount;
    };
    
    QHash<quint32, PendingPacket> m_pendingAcks;
    quint32 m_nextSequenceNumber;
    
    // Settings
    int m_ackTimeoutMs;
    int m_maxRetransmissions;
    bool m_reliabilityEnabled;
    
    // Thread safety
    QMutex m_pendingLock;
    QMutex m_socketLock;
    
    // Statistics
    std::atomic<int> m_packetsSent;
    std::atomic<int> m_acksReceived;
    std::atomic<int> m_retransmissions;
};

#endif // RELIABLEUDP_H