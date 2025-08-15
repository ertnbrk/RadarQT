#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QUdpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <random>
#include "../TelemetryReceiver/reliableudp.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QSpinBox;
QT_END_NAMESPACE

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void toggleSending();
    void sendTelemetryData();
    void updateInterval();
    void updateMovementSettings();

private:
    void setupUI();
    void setupSocket();
    QJsonObject generateTelemetryData();
    
    QTimer *m_timer;
    QTimer *m_movementTimer;
    QUdpSocket *m_udpSocket;                // Legacy UDP socket
    ReliableUdpSender *m_reliableSender;    // New reliable sender
    QPushButton *m_startStopButton;
    QSpinBox *m_intervalSpinBox;
    QSpinBox *m_movementIntervalSpinBox;
    QDoubleSpinBox *m_latSpinBox;
    QDoubleSpinBox *m_lonSpinBox;
    QDoubleSpinBox *m_latIncrementSpinBox;
    QDoubleSpinBox *m_lonIncrementSpinBox;
    QDoubleSpinBox *m_speedSpinBox;
    QLabel *m_statusLabel;
    QLabel *m_packetCountLabel;
    QLabel *m_lastDataLabel;
    QLabel *m_positionLabel;
    
    bool m_isSending;
    int m_packetCount;
    quint16 m_port;
    
    // Current position and movement
    double m_currentLat;
    double m_currentLon;
    double m_latIncrement;
    double m_lonIncrement;
    double m_currentSpeed;
};

#endif // MAINWINDOW_H