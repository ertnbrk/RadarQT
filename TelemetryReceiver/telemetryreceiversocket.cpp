#include "telemetryreceiversocket.h"
#include <QHostAddress>
#include <QJsonParseError>

TelemetryReceiverSocket::TelemetryReceiverSocket(QObject *parent)
    : QObject(parent)
    , m_udpSocket(new QUdpSocket(this))
    , m_port(12345)
    , m_isRecording(false)
    , m_playbackTimer(new QTimer(this))
    , m_playbackIndex(0)
{
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &TelemetryReceiverSocket::processPendingDatagrams);
    connect(m_playbackTimer, &QTimer::timeout, this, &TelemetryReceiverSocket::playbackNextPacket);
}

TelemetryReceiverSocket::~TelemetryReceiverSocket()
{
    stopListening();
}

bool TelemetryReceiverSocket::startListening(quint16 port)
{
    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        return true; // Already listening
    }
    
    m_port = port;
    bool success = m_udpSocket->bind(QHostAddress::LocalHost, m_port);
    
    if (!success) {
        emit errorOccurred(QString("Failed to bind to port %1: %2").arg(m_port).arg(m_udpSocket->errorString()));
        return false;
    }
    
    return true;
}

void TelemetryReceiverSocket::stopListening()
{
    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        m_udpSocket->close();
    }
}

bool TelemetryReceiverSocket::isListening() const
{
    return m_udpSocket->state() == QAbstractSocket::BoundState;
}

void TelemetryReceiverSocket::startRecording()
{
    m_isRecording = true;
    m_recordedPackets.clear();
    emit recordingStatusChanged(true);
}

void TelemetryReceiverSocket::stopRecording()
{
    m_isRecording = false;
    emit recordingStatusChanged(false);
}

void TelemetryReceiverSocket::clearRecording()
{
    m_recordedPackets.clear();
}

void TelemetryReceiverSocket::startPlayback(int intervalMs)
{
    if (m_recordedPackets.isEmpty()) {
        emit errorOccurred("No recorded data available for playback");
        return;
    }
    
    m_playbackIndex = 0;
    m_playbackTimer->setInterval(intervalMs);
    m_playbackTimer->start();
    emit playbackStatusChanged(true);
}

void TelemetryReceiverSocket::stopPlayback()
{
    m_playbackTimer->stop();
    emit playbackStatusChanged(false);
}

void TelemetryReceiverSocket::processPendingDatagrams()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_udpSocket->pendingDatagramSize()));
        
        QHostAddress sender;
        quint16 senderPort;
        
        qint64 bytesReceived = m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        if (bytesReceived == -1) {
            emit errorOccurred(QString("Failed to read UDP datagram: %1").arg(m_udpSocket->errorString()));
            continue;
        }
        
        // Record the packet if recording is enabled
        if (m_isRecording) {
            m_recordedPackets.append(datagram);
        }
        
        // Parse and emit the telemetry data
        TelemetryData data = parseTelemetryData(datagram);
        emitTelemetryData(data);
    }
}

void TelemetryReceiverSocket::playbackNextPacket()
{
    if (m_playbackIndex >= m_recordedPackets.size()) {
        // End of playback
        stopPlayback();
        return;
    }
    
    TelemetryData data = parseTelemetryData(m_recordedPackets[m_playbackIndex]);
    emitTelemetryData(data);
    
    m_playbackIndex++;
}

TelemetryData TelemetryReceiverSocket::parseTelemetryData(const QByteArray &data)
{
    TelemetryData telemetryData;
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        emit errorOccurred(QString("JSON parse error: %1").arg(parseError.errorString()));
        return telemetryData; // Return default values
    }
    
    if (!doc.isObject()) {
        emit errorOccurred("Received JSON is not an object");
        return telemetryData;
    }
    
    QJsonObject obj = doc.object();
    
    telemetryData.latitude = obj.value("latitude").toDouble();
    telemetryData.longitude = obj.value("longitude").toDouble();
    telemetryData.speed = obj.value("speed").toDouble();
    telemetryData.status = obj.value("status").toString();
    
    return telemetryData;
}

void TelemetryReceiverSocket::emitTelemetryData(const TelemetryData &data)
{
    emit telemetryDataReceived(data);
}