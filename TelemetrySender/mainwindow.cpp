#include "mainwindow.h"
#include <QHostAddress>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_movementTimer(new QTimer(this))
    , m_udpSocket(new QUdpSocket(this))
    , m_reliableSender(new ReliableUdpSender(this))
    , m_isSending(false)
    , m_packetCount(0)
    , m_port(12345)
    , m_currentLat(39.0)      // Default starting position
    , m_currentLon(35.5)
    , m_latIncrement(0.01)    // Default movement increment
    , m_lonIncrement(0.01)
    , m_currentSpeed(25.0)    // Default speed in km/h
{
    setupUI();
    setupSocket();
    
    connect(m_timer, &QTimer::timeout, this, &MainWindow::sendTelemetryData);
    connect(m_movementTimer, &QTimer::timeout, this, &MainWindow::updateMovementSettings);
    connect(m_startStopButton, &QPushButton::clicked, this, &MainWindow::toggleSending);
    connect(m_intervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateInterval);
    
    m_timer->setInterval(1000);        // Send data every 1 second
    m_movementTimer->setInterval(3000); // Move position every 3 seconds
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    setWindowTitle("Ship Telemetry Sender");
    setMinimumSize(500, 600);
    
    auto *mainLayout = new QVBoxLayout(this);
    
    // Position Configuration Group
    auto *positionGroup = new QGroupBox("Ship Position Configuration", this);
    auto *positionLayout = new QGridLayout(positionGroup);
    
    positionLayout->addWidget(new QLabel("Current Latitude:", this), 0, 0);
    m_latSpinBox = new QDoubleSpinBox(this);
    m_latSpinBox->setRange(-90.0, 90.0);
    m_latSpinBox->setValue(m_currentLat);
    m_latSpinBox->setDecimals(6);
    m_latSpinBox->setSingleStep(0.001);
    m_latSpinBox->setSuffix("°");
    positionLayout->addWidget(m_latSpinBox, 0, 1);
    
    positionLayout->addWidget(new QLabel("Current Longitude:", this), 1, 0);
    m_lonSpinBox = new QDoubleSpinBox(this);
    m_lonSpinBox->setRange(-180.0, 180.0);
    m_lonSpinBox->setValue(m_currentLon);
    m_lonSpinBox->setDecimals(6);
    m_lonSpinBox->setSingleStep(0.001);
    m_lonSpinBox->setSuffix("°");
    positionLayout->addWidget(m_lonSpinBox, 1, 1);
    
    positionLayout->addWidget(new QLabel("Speed (km/h):", this), 2, 0);
    m_speedSpinBox = new QDoubleSpinBox(this);
    m_speedSpinBox->setRange(0.0, 100.0);
    m_speedSpinBox->setValue(m_currentSpeed);
    m_speedSpinBox->setDecimals(1);
    m_speedSpinBox->setSingleStep(1.0);
    m_speedSpinBox->setSuffix(" km/h");
    positionLayout->addWidget(m_speedSpinBox, 2, 1);
    
    mainLayout->addWidget(positionGroup);
    
    // Movement Configuration Group
    auto *movementGroup = new QGroupBox("Movement Configuration", this);
    auto *movementLayout = new QGridLayout(movementGroup);
    
    movementLayout->addWidget(new QLabel("Latitude Increment:", this), 0, 0);
    m_latIncrementSpinBox = new QDoubleSpinBox(this);
    m_latIncrementSpinBox->setRange(-1.0, 1.0);
    m_latIncrementSpinBox->setValue(m_latIncrement);
    m_latIncrementSpinBox->setDecimals(6);
    m_latIncrementSpinBox->setSingleStep(0.001);
    m_latIncrementSpinBox->setSuffix("°");
    movementLayout->addWidget(m_latIncrementSpinBox, 0, 1);
    
    movementLayout->addWidget(new QLabel("Longitude Increment:", this), 1, 0);
    m_lonIncrementSpinBox = new QDoubleSpinBox(this);
    m_lonIncrementSpinBox->setRange(-1.0, 1.0);
    m_lonIncrementSpinBox->setValue(m_lonIncrement);
    m_lonIncrementSpinBox->setDecimals(6);
    m_lonIncrementSpinBox->setSingleStep(0.001);
    m_lonIncrementSpinBox->setSuffix("°");
    movementLayout->addWidget(m_lonIncrementSpinBox, 1, 1);
    
    movementLayout->addWidget(new QLabel("Movement Interval:", this), 2, 0);
    m_movementIntervalSpinBox = new QSpinBox(this);
    m_movementIntervalSpinBox->setRange(1, 60);
    m_movementIntervalSpinBox->setValue(3);
    m_movementIntervalSpinBox->setSuffix(" seconds");
    movementLayout->addWidget(m_movementIntervalSpinBox, 2, 1);
    
    mainLayout->addWidget(movementGroup);
    
    // Control group
    auto *controlGroup = new QGroupBox("Transmission Control", this);
    auto *controlLayout = new QGridLayout(controlGroup);
    
    controlLayout->addWidget(new QLabel("Send Interval:", this), 0, 0);
    m_intervalSpinBox = new QSpinBox(this);
    m_intervalSpinBox->setRange(100, 10000);
    m_intervalSpinBox->setValue(1000);
    m_intervalSpinBox->setSingleStep(100);
    m_intervalSpinBox->setSuffix(" ms");
    controlLayout->addWidget(m_intervalSpinBox, 0, 1);
    
    m_startStopButton = new QPushButton("Start Sending", this);
    m_startStopButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 10px; }");
    controlLayout->addWidget(m_startStopButton, 1, 0, 1, 2);
    
    mainLayout->addWidget(controlGroup);
    
    // Status group
    auto *statusGroup = new QGroupBox("Status", this);
    auto *statusLayout = new QVBoxLayout(statusGroup);
    
    m_statusLabel = new QLabel("Status: Stopped", this);
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #f44336; }");
    statusLayout->addWidget(m_statusLabel);
    
    m_packetCountLabel = new QLabel("Packets sent: 0", this);
    statusLayout->addWidget(m_packetCountLabel);
    
    m_positionLabel = new QLabel("Position: Not moving", this);
    m_positionLabel->setStyleSheet("QLabel { font-weight: bold; color: #2196F3; }");
    statusLayout->addWidget(m_positionLabel);
    
    mainLayout->addWidget(statusGroup);
    
    // Last data group
    auto *dataGroup = new QGroupBox("Current Telemetry Data", this);
    auto *dataLayout = new QVBoxLayout(dataGroup);
    
    m_lastDataLabel = new QLabel("No data sent yet", this);
    m_lastDataLabel->setWordWrap(true);
    m_lastDataLabel->setStyleSheet("QLabel { font-family: monospace; background-color: #f5f5f5; padding: 10px; }");
    dataLayout->addWidget(m_lastDataLabel);
    
    mainLayout->addWidget(dataGroup);
    
    // Connect the UI controls
    connect(m_latSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            [this](double value) { m_currentLat = value; });
    connect(m_lonSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            [this](double value) { m_currentLon = value; });
    connect(m_speedSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            [this](double value) { m_currentSpeed = value; });
    connect(m_latIncrementSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            [this](double value) { m_latIncrement = value; });
    connect(m_lonIncrementSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            [this](double value) { m_lonIncrement = value; });
    connect(m_movementIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            [this](int value) { m_movementTimer->setInterval(value * 1000); });
}

void MainWindow::setupSocket()
{
    // Setup reliable sender target
    m_reliableSender->setTarget(QHostAddress::LocalHost, m_port);
    m_reliableSender->setReliabilityEnabled(true);
    m_reliableSender->setAckTimeoutMs(3000);
    m_reliableSender->setMaxRetransmissions(3);
    
    qRegisterMetaType<TelemetryPacket>("TelemetryPacket");
}

void MainWindow::toggleSending()
{
    if (m_isSending) {
        m_timer->stop();
        m_movementTimer->stop();
        m_isSending = false;
        m_startStopButton->setText("Start Sending");
        m_startStopButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 10px; }");
        m_statusLabel->setText("Status: Stopped");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #f44336; }");
        m_positionLabel->setText("Position: Not moving");
        m_positionLabel->setStyleSheet("QLabel { font-weight: bold; color: #2196F3; }");
    } else {
        // Update current position from UI
        m_currentLat = m_latSpinBox->value();
        m_currentLon = m_lonSpinBox->value();
        m_currentSpeed = m_speedSpinBox->value();
        m_latIncrement = m_latIncrementSpinBox->value();
        m_lonIncrement = m_lonIncrementSpinBox->value();
        
        m_timer->start();
        m_movementTimer->start();
        m_isSending = true;
        m_startStopButton->setText("Stop Sending");
        m_startStopButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; padding: 10px; }");
        m_statusLabel->setText("Status: Sending");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
        m_positionLabel->setText("Position: Moving");
        m_positionLabel->setStyleSheet("QLabel { font-weight: bold; color: #FF9800; }");
    }
}

void MainWindow::updateInterval()
{
    m_timer->setInterval(m_intervalSpinBox->value());
}

void MainWindow::updateMovementSettings()
{
    // Update position based on increments
    m_currentLat += m_latIncrement;
    m_currentLon += m_lonIncrement;
    
    // Update UI to show current position
    m_latSpinBox->blockSignals(true);
    m_lonSpinBox->blockSignals(true);
    m_latSpinBox->setValue(m_currentLat);
    m_lonSpinBox->setValue(m_currentLon);
    m_latSpinBox->blockSignals(false);
    m_lonSpinBox->blockSignals(false);
    
    // Update position status
    m_positionLabel->setText(QString("Position: Moving (Lat: %1°, Lon: %2°)")
                            .arg(m_currentLat, 0, 'f', 6)
                            .arg(m_currentLon, 0, 'f', 6));
}

void MainWindow::sendTelemetryData()
{
    // Create reliable telemetry packet
    TelemetryPacket packet;
    packet.latitude = m_currentLat;
    packet.longitude = m_currentLon;
    packet.speed = m_currentSpeed;
    packet.status = "OK";
    packet.timestamp = QDateTime::currentDateTime();
    packet.needsAck = true;  // Request acknowledgment
    
    // Send via reliable UDP
    m_reliableSender->sendTelemetryData(packet);
    
    m_packetCount++;
    m_packetCountLabel->setText(QString("Packets sent: %1").arg(m_packetCount));
    
    // Display last data in readable format
    QString lastDataText = QString("Sequence: %1\nLatitude: %2°\nLongitude: %3°\nSpeed: %4 km/h\nStatus: %5\nTimestamp: %6")
                              .arg(packet.sequenceNumber)
                              .arg(packet.latitude, 0, 'f', 6)
                              .arg(packet.longitude, 0, 'f', 6)
                              .arg(packet.speed, 0, 'f', 1)
                              .arg(packet.status)
                              .arg(packet.timestamp.toString("hh:mm:ss.zzz"));
    
    m_lastDataLabel->setText(lastDataText);
}

QJsonObject MainWindow::generateTelemetryData()
{
    QJsonObject data;
    data["latitude"] = m_currentLat;
    data["longitude"] = m_currentLon;
    data["speed"] = m_currentSpeed;
    data["status"] = "OK";  // Ship status is always OK for simulation
    
    return data;
}