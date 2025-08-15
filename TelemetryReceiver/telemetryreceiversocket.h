#ifndef TELEMETRYRECEIVERSOCKET_H
#define TELEMETRYRECEIVERSOCKET_H

#include <QObject>
#include <QUdpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>

struct TelemetryData {
    double latitude;
    double longitude;
    double speed;
    QString status;
    
    TelemetryData() : latitude(0.0), longitude(0.0), speed(0.0), status("OK") {}
    TelemetryData(double lat, double lon, double spd, const QString &st) 
        : latitude(lat), longitude(lon), speed(spd), status(st) {}
};

Q_DECLARE_METATYPE(TelemetryData)

class TelemetryReceiverSocket : public QObject
{
    Q_OBJECT

public:
    explicit TelemetryReceiverSocket(QObject *parent = nullptr);
    ~TelemetryReceiverSocket();
    
    bool startListening(quint16 port = 12345);
    void stopListening();
    bool isListening() const;
    
    // Recording functionality
    void startRecording();
    void stopRecording();
    void clearRecording();
    bool isRecording() const { return m_isRecording; }
    int getRecordedPacketCount() const { return m_recordedPackets.size(); }
    
    // Playback functionality
    void startPlayback(int intervalMs = 500);
    void stopPlayback();
    bool isPlayingBack() const { return m_playbackTimer->isActive(); }

signals:
    void telemetryDataReceived(const TelemetryData &data);
    void errorOccurred(const QString &error);
    void recordingStatusChanged(bool recording);
    void playbackStatusChanged(bool playing);

private slots:
    void processPendingDatagrams();
    void playbackNextPacket();

private:
    TelemetryData parseTelemetryData(const QByteArray &data);
    void emitTelemetryData(const TelemetryData &data);
    
    QUdpSocket *m_udpSocket;
    quint16 m_port;
    
    // Recording
    bool m_isRecording;
    QVector<QByteArray> m_recordedPackets;
    
    // Playback
    QTimer *m_playbackTimer;
    int m_playbackIndex;
};

#endif // TELEMETRYRECEIVERSOCKET_H