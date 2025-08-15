#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QSplitter>
#include <QGridLayout>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_radarWidget(nullptr)
    , m_receiver(new TelemetryReceiverSocket(this))
    , m_reliableReceiver(new ReliableUdpReceiver(this))
    , m_packetCount(0)
{
    qRegisterMetaType<TelemetryData>("TelemetryData");
    qRegisterMetaType<TelemetryPacket>("TelemetryPacket");
    
    setupUI();
    setupStatusBar();
    
    // Connect receiver signals
    connect(m_receiver, &TelemetryReceiverSocket::telemetryDataReceived,
            this, &MainWindow::onTelemetryDataReceived);
    connect(m_receiver, &TelemetryReceiverSocket::errorOccurred,
            this, &MainWindow::onSocketError);
    connect(m_receiver, &TelemetryReceiverSocket::recordingStatusChanged,
            this, &MainWindow::onRecordingStatusChanged);
    connect(m_receiver, &TelemetryReceiverSocket::playbackStatusChanged,
            this, &MainWindow::onPlaybackStatusChanged);
    
    // Connect reliable receiver signals
    connect(m_reliableReceiver, &ReliableUdpReceiver::telemetryDataReceived,
            this, &MainWindow::onReliableTelemetryReceived);
    connect(m_reliableReceiver, &ReliableUdpReceiver::connectionStatusChanged,
            this, &MainWindow::onConnectionStatusChanged);
    connect(m_reliableReceiver, &ReliableUdpReceiver::statisticsUpdated,
            this, &MainWindow::onNetworkStatisticsUpdated);
    
    // Start listening with reliable receiver
    if (m_reliableReceiver->startListening(12345)) {
        m_connectionStatusLabel->setText("Status: Reliable UDP listening on port 12345");
        m_connectionStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_connectionStatusLabel->setText("Status: Failed to start reliable UDP listener");
        m_connectionStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    setWindowTitle("Ship Radar - Telemetry Receiver");
    setMinimumSize(1200, 800);
    
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    // Create main layout
    auto *mainLayout = new QHBoxLayout(m_centralWidget);
    
    // Create splitter for resizable panels
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    
    // Left panel - Radar display
    m_radarWidget = new RadarWidget(this);
    m_radarWidget->setMinimumSize(600, 600);
    splitter->addWidget(m_radarWidget);
    
    // Connect radar signals
    connect(m_radarWidget, &RadarWidget::rangeChanged,
            this, &MainWindow::onRadarRangeChanged);
    
    // Right panel - Controls and data display
    auto *rightPanel = new QWidget(this);
    rightPanel->setFixedWidth(400);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    
    // Radar controls group
    auto *radarGroup = new QGroupBox("Radar Controls", this);
    auto *radarLayout = new QGridLayout(radarGroup);
    
    radarLayout->addWidget(new QLabel("Range (NM):", this), 0, 0);
    m_rangeSpinBox = new QDoubleSpinBox(this);
    m_rangeSpinBox->setRange(50.0, 1000.0);
    m_rangeSpinBox->setValue(500.0);
    m_rangeSpinBox->setSingleStep(0.5);
    m_rangeSpinBox->setSuffix(" NM");
    connect(m_rangeSpinBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onRadarRangeChanged);
    radarLayout->addWidget(m_rangeSpinBox, 0, 1);
    
    radarLayout->addWidget(new QLabel("Sweep Speed:", this), 1, 0);
    m_sweepSpeedSlider = new QSlider(Qt::Horizontal, this);
    m_sweepSpeedSlider->setRange(1, 60);
    m_sweepSpeedSlider->setValue(12);
    connect(m_sweepSpeedSlider, &QSlider::valueChanged,
            this, &MainWindow::onSweepSpeedChanged);
    radarLayout->addWidget(m_sweepSpeedSlider, 1, 1);
    
    m_sweepEnabledCheckBox = new QCheckBox("Sweep Enabled", this);
    m_sweepEnabledCheckBox->setChecked(true);
    connect(m_sweepEnabledCheckBox, &QCheckBox::toggled,
            this, &MainWindow::onSweepToggled);
    radarLayout->addWidget(m_sweepEnabledCheckBox, 2, 0, 1, 2);
    
    rightLayout->addWidget(radarGroup);
    
    // Telemetry data group
    auto *dataGroup = new QGroupBox("Current Contact Data", this);
    auto *dataLayout = new QGridLayout(dataGroup);
    
    dataLayout->addWidget(new QLabel("Coordinates:", this), 0, 0);
    m_coordinatesLabel = new QLabel("N/A", this);
    m_coordinatesLabel->setStyleSheet("font-family: monospace; font-weight: bold; color: #00FF00;");
    dataLayout->addWidget(m_coordinatesLabel, 0, 1);
    
    dataLayout->addWidget(new QLabel("Bearing:", this), 1, 0);
    m_bearingLabel = new QLabel("N/A", this);
    m_bearingLabel->setStyleSheet("font-family: monospace; font-weight: bold; color: #00FF00;");
    dataLayout->addWidget(m_bearingLabel, 1, 1);
    
    dataLayout->addWidget(new QLabel("Range:", this), 2, 0);
    m_rangeLabel = new QLabel("N/A", this);
    m_rangeLabel->setStyleSheet("font-family: monospace; font-weight: bold; color: #00FF00;");
    dataLayout->addWidget(m_rangeLabel, 2, 1);
    
    dataLayout->addWidget(new QLabel("Speed:", this), 3, 0);
    m_speedLabel = new QLabel("0.0 kts", this);
    m_speedLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #00FF00;");
    dataLayout->addWidget(m_speedLabel, 3, 1);
    
    m_speedProgressBar = new QProgressBar(this);
    m_speedProgressBar->setRange(0, 50);
    m_speedProgressBar->setValue(0);
    m_speedProgressBar->setStyleSheet("QProgressBar { background-color: #001100; border: 1px solid #00FF00; } QProgressBar::chunk { background-color: #00FF00; }");
    dataLayout->addWidget(m_speedProgressBar, 4, 0, 1, 2);
    
    dataLayout->addWidget(new QLabel("Status:", this), 5, 0);
    m_statusLabel = new QLabel("N/A", this);
    m_statusLabel->setStyleSheet("font-weight: bold; color: #00FF00;");
    dataLayout->addWidget(m_statusLabel, 5, 1);
    
    rightLayout->addWidget(dataGroup);
    
    // Recording controls group
    auto *recordingGroup = new QGroupBox("Recording & Playback", this);
    auto *recordingLayout = new QVBoxLayout(recordingGroup);
    
    m_recordButton = new QPushButton("Start Recording", this);
    m_recordButton->setStyleSheet("QPushButton { background-color: #ff4444; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    connect(m_recordButton, &QPushButton::clicked, this, &MainWindow::toggleRecording);
    recordingLayout->addWidget(m_recordButton);
    
    auto *playbackControlsLayout = new QHBoxLayout();
    playbackControlsLayout->addWidget(new QLabel("Interval (ms):", this));
    
    m_playbackIntervalSpinBox = new QSpinBox(this);
    m_playbackIntervalSpinBox->setRange(100, 5000);
    m_playbackIntervalSpinBox->setValue(500);
    m_playbackIntervalSpinBox->setSingleStep(100);
    m_playbackIntervalSpinBox->setStyleSheet("QSpinBox { background-color: #001100; color: #00FF00; border: 1px solid #00FF00; }");
    playbackControlsLayout->addWidget(m_playbackIntervalSpinBox);
    
    recordingLayout->addLayout(playbackControlsLayout);
    
    m_playbackButton = new QPushButton("Start Playback", this);
    m_playbackButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    m_playbackButton->setEnabled(false);
    connect(m_playbackButton, &QPushButton::clicked, this, &MainWindow::startPlayback);
    recordingLayout->addWidget(m_playbackButton);
    
    m_stopPlaybackButton = new QPushButton("Stop Playback", this);
    m_stopPlaybackButton->setStyleSheet("QPushButton { background-color: #ff9800; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    m_stopPlaybackButton->setEnabled(false);
    connect(m_stopPlaybackButton, &QPushButton::clicked, this, &MainWindow::stopPlayback);
    recordingLayout->addWidget(m_stopPlaybackButton);
    
    m_clearButton = new QPushButton("Clear Recording", this);
    m_clearButton->setStyleSheet("QPushButton { background-color: #666666; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::clearRecording);
    recordingLayout->addWidget(m_clearButton);
    
    rightLayout->addWidget(recordingGroup);
    rightLayout->addStretch();
    
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1); // Map view gets most space
    splitter->setStretchFactor(1, 0); // Right panel fixed width
    
    mainLayout->addWidget(splitter);
}

void MainWindow::setupStatusBar()
{
    m_connectionStatusLabel = new QLabel("Status: Initializing...", this);
    m_packetCountLabel = new QLabel("Packets: 0", this);
    m_recordingStatusLabel = new QLabel("", this);
    m_networkStatsLabel = new QLabel("Network: Ready", this);
    m_packetLossLabel = new QLabel("Loss: 0%", this);
    m_interpolationLabel = new QLabel("Interpolated: 0", this);
    
    statusBar()->addWidget(m_connectionStatusLabel);
    statusBar()->addPermanentWidget(m_networkStatsLabel);
    statusBar()->addPermanentWidget(m_packetLossLabel);
    statusBar()->addPermanentWidget(m_interpolationLabel);
    statusBar()->addPermanentWidget(m_recordingStatusLabel);
    statusBar()->addPermanentWidget(m_packetCountLabel);
}

void MainWindow::onTelemetryDataReceived(const TelemetryData &data)
{
    m_lastData = data;
    m_packetCount++;
    
    updateTelemetryDisplay(data);
    m_radarWidget->addTelemetryContact(data);
    
    m_packetCountLabel->setText(QString("Packets: %1").arg(m_packetCount));
}

void MainWindow::onSocketError(const QString &error)
{
    QMessageBox::warning(this, "Socket Error", error);
    m_connectionStatusLabel->setText("Status: Error - " + error);
    m_connectionStatusLabel->setStyleSheet("color: red; font-weight: bold;");
}

void MainWindow::onRecordingStatusChanged(bool recording)
{
    if (recording) {
        m_recordButton->setText("Stop Recording");
        m_recordButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px; }");
        m_recordingStatusLabel->setText("Recording...");
        m_recordingStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    } else {
        m_recordButton->setText("Start Recording");
        m_recordButton->setStyleSheet("QPushButton { background-color: #ff4444; color: white; font-weight: bold; padding: 8px; }");
        m_recordingStatusLabel->setText("");
        
        // Enable playback if we have recorded data
        m_playbackButton->setEnabled(m_receiver->getRecordedPacketCount() > 0);
    }
}

void MainWindow::onPlaybackStatusChanged(bool playing)
{
    m_playbackButton->setEnabled(!playing && m_receiver->getRecordedPacketCount() > 0);
    m_stopPlaybackButton->setEnabled(playing);
    
    if (playing) {
        m_recordingStatusLabel->setText("Playing back...");
        m_recordingStatusLabel->setStyleSheet("color: blue; font-weight: bold;");
    } else if (!m_receiver->isRecording()) {
        m_recordingStatusLabel->setText("");
    }
}

void MainWindow::toggleRecording()
{
    if (m_receiver->isRecording()) {
        m_receiver->stopRecording();
    } else {
        m_receiver->startRecording();
    }
}

void MainWindow::startPlayback()
{
    m_receiver->startPlayback(m_playbackIntervalSpinBox->value());
}

void MainWindow::stopPlayback()
{
    m_receiver->stopPlayback();
}

void MainWindow::clearRecording()
{
    m_receiver->clearRecording();
    m_playbackButton->setEnabled(false);
    m_radarWidget->clearContact();
}

void MainWindow::onRadarRangeChanged(double range)
{
    m_rangeSpinBox->blockSignals(true);
    m_rangeSpinBox->setValue(range);
    m_rangeSpinBox->blockSignals(false);
}

void MainWindow::onSweepSpeedChanged(int rpm)
{
    m_radarWidget->setSweepSpeed(rpm);
}

void MainWindow::onSweepToggled(bool enabled)
{
    m_radarWidget->toggleSweep(enabled);
}

void MainWindow::onReliableTelemetryReceived(const TelemetryPacket &packet)
{
    try {
        qDebug() << "Received telemetry packet:" << packet.sequenceNumber;
        
        // Convert TelemetryPacket to TelemetryData for compatibility
        TelemetryData data;
        data.latitude = packet.latitude;
        data.longitude = packet.longitude;
        data.speed = packet.speed;
        data.status = packet.status;
        
        m_lastData = data;
        m_packetCount++;
        
        qDebug() << "Updating telemetry display...";
        updateTelemetryDisplay(data);
        
        qDebug() << "Adding telemetry contact to radar...";
        if (m_radarWidget) {
            m_radarWidget->addTelemetryContact(data);
        }
        
        qDebug() << "Updating packet count label...";
        if (m_packetCountLabel) {
            m_packetCountLabel->setText(QString("Packets: %1").arg(m_packetCount));
        }
        
        qDebug() << "Telemetry packet processing completed successfully";
    } catch (const std::exception& e) {
        qDebug() << "Exception in onReliableTelemetryReceived:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in onReliableTelemetryReceived";
    }
}

void MainWindow::onConnectionStatusChanged(bool connected)
{
    if (connected) {
        m_connectionStatusLabel->setText("Status: Reliable UDP Connected");
        m_connectionStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_connectionStatusLabel->setText("Status: Reliable UDP Disconnected");
        m_connectionStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}

void MainWindow::onNetworkStatisticsUpdated()
{
    int received = m_reliableReceiver->getPacketsReceived();
    int lost = m_reliableReceiver->getPacketsLost();
    int interpolated = m_reliableReceiver->getPacketsInterpolated();
    double lossRate = m_reliableReceiver->getPacketLossRate();
    
    m_networkStatsLabel->setText(QString("Rx: %1").arg(received));
    m_packetLossLabel->setText(QString("Loss: %1%").arg(lossRate, 0, 'f', 1));
    m_interpolationLabel->setText(QString("Interp: %1").arg(interpolated));
    
    // Update colors based on loss rate
    if (lossRate < 1.0) {
        m_packetLossLabel->setStyleSheet("color: green; font-weight: bold;");
    } else if (lossRate < 5.0) {
        m_packetLossLabel->setStyleSheet("color: orange; font-weight: bold;");
    } else {
        m_packetLossLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}


void MainWindow::updateTelemetryDisplay(const TelemetryData &data)
{
    m_coordinatesLabel->setText(QString("%1°, %2°")
                               .arg(data.latitude, 0, 'f', 6)
                               .arg(data.longitude, 0, 'f', 6));
    
    // Calculate bearing and range from radar center (center of telemetry area)
    double radarLat = 39.0;  // Center of 36-42 latitude range
    double radarLon = 35.5;  // Center of 26-45 longitude range
    
    double dLon = (data.longitude - radarLon) * M_PI / 180.0;
    double lat1Rad = radarLat * M_PI / 180.0;
    double lat2Rad = data.latitude * M_PI / 180.0;
    
    double y = sin(dLon) * cos(lat2Rad);
    double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(dLon);
    double bearing = atan2(y, x) * 180.0 / M_PI;
    bearing = fmod(bearing + 360.0, 360.0);
    
    double dLat = (data.latitude - radarLat) * M_PI / 180.0;
    double a = sin(dLat/2) * sin(dLat/2) + cos(lat1Rad) * cos(lat2Rad) * sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double rangeNM = 3440.065 * c; // Earth radius in nautical miles
    
    m_bearingLabel->setText(QString("%1°").arg(bearing, 0, 'f', 1));
    m_rangeLabel->setText(QString("%1 NM").arg(rangeNM, 0, 'f', 2));
    
    // Convert km/h to knots for maritime display
    double speedKnots = data.speed * 0.539957;
    m_speedLabel->setText(QString("%1 kts").arg(speedKnots, 0, 'f', 1));
    m_speedProgressBar->setValue(static_cast<int>(speedKnots));
    
    m_statusLabel->setText(data.status);
    if (data.status == "OK") {
        m_statusLabel->setStyleSheet("color: #00FF00; font-weight: bold;");
    } else {
        m_statusLabel->setStyleSheet("color: #FF0000; font-weight: bold;");
    }
}

