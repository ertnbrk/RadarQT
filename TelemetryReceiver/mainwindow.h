#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QStatusBar>
#include <QTimer>
#include "telemetryreceiversocket.h"
#include "radarwidget.h"
#include "reliableudp.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QProgressBar;
class QPushButton;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onTelemetryDataReceived(const TelemetryData &data);
    void onSocketError(const QString &error);
    void onRecordingStatusChanged(bool recording);
    void onPlaybackStatusChanged(bool playing);
    void toggleRecording();
    void startPlayback();
    void stopPlayback();
    void clearRecording();
    void onRadarRangeChanged(double range);
    void onSweepSpeedChanged(int rpm);
    void onSweepToggled(bool enabled);
    void onReliableTelemetryReceived(const TelemetryPacket &packet);
    void onConnectionStatusChanged(bool connected);
    void onNetworkStatisticsUpdated();

private:
    void setupUI();
    void setupStatusBar();
    void updateTelemetryDisplay(const TelemetryData &data);
    
    // UI Components
    QWidget *m_centralWidget;
    RadarWidget *m_radarWidget;
    
    QLabel *m_speedLabel;
    QLabel *m_statusLabel;
    QLabel *m_coordinatesLabel;
    QLabel *m_bearingLabel;
    QLabel *m_rangeLabel;
    QProgressBar *m_speedProgressBar;
    
    // Radar controls
    QDoubleSpinBox *m_rangeSpinBox;
    QSlider *m_sweepSpeedSlider;
    QCheckBox *m_sweepEnabledCheckBox;
    
    // Control buttons
    QPushButton *m_recordButton;
    QPushButton *m_playbackButton;
    QPushButton *m_stopPlaybackButton;
    QPushButton *m_clearButton;
    QSpinBox *m_playbackIntervalSpinBox;
    
    // Status bar labels
    QLabel *m_connectionStatusLabel;
    QLabel *m_packetCountLabel;
    QLabel *m_recordingStatusLabel;
    
    // Telemetry receivers
    TelemetryReceiverSocket *m_receiver;        // Legacy receiver
    ReliableUdpReceiver *m_reliableReceiver;    // New reliable receiver
    
    // Network statistics labels
    QLabel *m_networkStatsLabel;
    QLabel *m_packetLossLabel;
    QLabel *m_interpolationLabel;
    
    // Statistics
    int m_packetCount;
    
    // Last received data
    TelemetryData m_lastData;
};

#endif // MAINWINDOW_H