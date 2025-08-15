#include "radarwidget.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QFont>
#include <QFontMetrics>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

RadarWidget::RadarWidget(QWidget *parent)
    : QWidget(parent)
    , m_rangeNM(500.0)  // 500 NM range to cover the telemetry geographic area
    , m_sweepRPM(12.0)
    , m_waveRadius(0.0)
    , m_sweepEnabled(true)
    , m_radarCenter(200, 200)
    , m_radarRadius(180)
    , m_numRangeRings(4)
    , m_backgroundColor(QColor(0, 20, 0))
    , m_gridColor(QColor(0, 255, 0, 180))
    , m_sweepColor(QColor(0, 255, 0, 100))
    , m_contactColor(QColor(255, 255, 0))
    , m_hasContact(false)
    , m_radarLat(39.0)  // Center position for telemetry area (between 36-42 lat)
    , m_radarLon(35.5)  // Center position for telemetry area (between 26-45 lon)
{
    setMinimumSize(400, 400);
    setAttribute(Qt::WA_OpaquePaintEvent);
    
    // Setup sweep animation timer
    m_sweepTimer = new QTimer(this);
    connect(m_sweepTimer, &QTimer::timeout, this, &RadarWidget::updateSweep);
    m_sweepTimer->setInterval(50); // 20 FPS
    if (m_sweepEnabled) {
        m_sweepTimer->start();
    }
    
}

RadarWidget::~RadarWidget() = default;

void RadarWidget::setRange(double nauticalMiles)
{
    m_rangeNM = qMax(0.5, qMin(100.0, nauticalMiles));
    m_numRangeRings = (m_rangeNM <= 2.0) ? 4 : (m_rangeNM <= 10.0) ? 5 : 6;
    emit rangeChanged(m_rangeNM);
    update();
}

void RadarWidget::setSweepSpeed(double rpm)
{
    m_sweepRPM = qMax(1.0, qMin(60.0, rpm));
}

void RadarWidget::addTelemetryContact(const TelemetryData &data)
{
    // Calculate bearing and range from radar position
    double bearing = calculateBearing(m_radarLat, m_radarLon, data.latitude, data.longitude);
    double range = calculateRange(m_radarLat, m_radarLon, data.latitude, data.longitude);
    
    // Only show contact if within radar range
    if (range <= m_rangeNM) {
        // Update the single current contact
        m_currentContact = RadarContact(bearing, range, data.latitude, data.longitude, 1.0, "SHIP");
        m_hasContact = true;
        update();
    } else {
        // Contact is out of range, hide it
        m_hasContact = false;
        update();
    }
}

void RadarWidget::clearContact()
{
    m_hasContact = false;
    update();
}

void RadarWidget::toggleSweep(bool enabled)
{
    m_sweepEnabled = enabled;
    if (enabled && !m_sweepTimer->isActive()) {
        m_sweepTimer->start();
    } else if (!enabled && m_sweepTimer->isActive()) {
        m_sweepTimer->stop();
    }
}

void RadarWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background
    painter.fillRect(rect(), m_backgroundColor);
    
    // Draw radar components
    drawRadarBackground(painter);
    drawRangeRings(painter);
    drawBearingLines(painter);
    drawCompassRose(painter);
    drawScanningWave(painter);
    drawContact(painter);
    drawRadarInfo(painter);
}

void RadarWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // Update radar center and radius based on new size
    int minDimension = qMin(width(), height());
    m_radarRadius = (minDimension - 60) / 2;
    m_radarCenter = QPointF(width() / 2.0, height() / 2.0);
}

void RadarWidget::mousePressEvent(QMouseEvent *event)
{
    // Handle contact selection (future enhancement)
    QPointF clickPos = event->position();
    // Convert to world coordinates and check for nearby contacts
    QWidget::mousePressEvent(event);
}

void RadarWidget::wheelEvent(QWheelEvent *event)
{
    // Zoom in/out by changing range
    if (event->angleDelta().y() > 0) {
        setRange(m_rangeNM * 0.8);  // Zoom in
    } else {
        setRange(m_rangeNM * 1.25); // Zoom out
    }
    event->accept();
}

void RadarWidget::updateSweep()
{
    // Update wave radius - expand from center to edge
    double waveSpeed = m_sweepRPM * 3.0; // Wave expansion speed
    double deltaTime = m_sweepTimer->interval() / 1000.0;
    m_waveRadius += waveSpeed * deltaTime;
    
    // Reset wave when it reaches the edge
    if (m_waveRadius >= m_radarRadius) {
        m_waveRadius = 0.0;
    }
    
    update();
}


void RadarWidget::drawRadarBackground(QPainter &painter)
{
    // Draw circular radar screen with subtle gradient
    QRadialGradient gradient(m_radarCenter, m_radarRadius);
    gradient.setColorAt(0, QColor(0, 30, 0, 50));
    gradient.setColorAt(1, QColor(0, 10, 0, 100));
    
    painter.setBrush(QBrush(gradient));
    painter.setPen(QPen(m_gridColor, 2));
    painter.drawEllipse(m_radarCenter.x() - m_radarRadius, 
                       m_radarCenter.y() - m_radarRadius, 
                       m_radarRadius * 2, m_radarRadius * 2);
}

void RadarWidget::drawRangeRings(QPainter &painter)
{
    painter.setPen(QPen(m_gridColor, 1));
    painter.setBrush(Qt::NoBrush);
    
    for (int i = 1; i <= m_numRangeRings; ++i) {
        double ringRadius = (m_radarRadius * i) / m_numRangeRings;
        painter.drawEllipse(m_radarCenter.x() - ringRadius, 
                           m_radarCenter.y() - ringRadius, 
                           ringRadius * 2, ringRadius * 2);
        
        // Draw range labels
        QFont font("Arial", 8);
        painter.setFont(font);
        double range = (m_rangeNM * i) / m_numRangeRings;
        QString label = QString("%1 NM").arg(range, 0, 'f', 1);
        
        QPointF labelPos(m_radarCenter.x() + ringRadius - 30, m_radarCenter.y() - 5);
        painter.drawText(labelPos, label);
    }
}

void RadarWidget::drawBearingLines(QPainter &painter)
{
    painter.setPen(QPen(m_gridColor, 1));
    
    // Draw major bearing lines (every 30 degrees)
    for (int bearing = 0; bearing < 360; bearing += 30) {
        double radians = bearing * DEG_TO_RAD;
        QPointF start = m_radarCenter;
        QPointF end(m_radarCenter.x() + m_radarRadius * sin(radians),
                   m_radarCenter.y() - m_radarRadius * cos(radians));
        painter.drawLine(start, end);
    }
    
    // Draw minor bearing lines (every 10 degrees) - shorter
    painter.setPen(QPen(m_gridColor, 1, Qt::DotLine));
    for (int bearing = 0; bearing < 360; bearing += 10) {
        if (bearing % 30 != 0) { // Skip major bearing lines
            double radians = bearing * DEG_TO_RAD;
            QPointF start(m_radarCenter.x() + m_radarRadius * 0.9 * sin(radians),
                         m_radarCenter.y() - m_radarRadius * 0.9 * cos(radians));
            QPointF end(m_radarCenter.x() + m_radarRadius * sin(radians),
                       m_radarCenter.y() - m_radarRadius * cos(radians));
            painter.drawLine(start, end);
        }
    }
}

void RadarWidget::drawCompassRose(QPainter &painter)
{
    QFont font("Arial", 10, QFont::Bold);
    painter.setFont(font);
    painter.setPen(QPen(m_gridColor));
    
    // Draw cardinal directions
    QStringList directions = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    for (int i = 0; i < 8; ++i) {
        double bearing = i * 45;
        double radians = bearing * DEG_TO_RAD;
        
        QPointF textPos(m_radarCenter.x() + (m_radarRadius + 15) * sin(radians),
                       m_radarCenter.y() - (m_radarRadius + 15) * cos(radians));
        
        QFontMetrics metrics(font);
        QRect textRect = metrics.boundingRect(directions[i]);
        textPos.setX(textPos.x() - textRect.width() / 2);
        textPos.setY(textPos.y() + textRect.height() / 2);
        
        painter.drawText(textPos, directions[i]);
    }
}

void RadarWidget::drawScanningWave(QPainter &painter)
{
    if (!m_sweepEnabled || m_waveRadius <= 0) return;
    
    // Save the current painter state
    painter.save();
    
    // Create a clipping region to only draw within the radar circle
    QPainterPath clipPath;
    clipPath.addEllipse(m_radarCenter.x() - m_radarRadius, 
                       m_radarCenter.y() - m_radarRadius, 
                       m_radarRadius * 2, m_radarRadius * 2);
    painter.setClipPath(clipPath);
    
    // Draw multiple concentric wave circles for better effect
    for (int i = 0; i < 3; ++i) {
        double waveOffset = i * 30.0; // Offset between waves
        double currentWaveRadius = m_waveRadius - waveOffset;
        
        if (currentWaveRadius > 0 && currentWaveRadius <= m_radarRadius) {
            // Calculate alpha based on wave position (fade as it expands)
            double alpha = 1.0 - (currentWaveRadius / m_radarRadius);
            alpha = qMax(0.1, qMin(1.0, alpha));
            
            // Set wave color with calculated alpha
            QColor waveColor(0, 255, 0, static_cast<int>(alpha * 120));
            painter.setPen(QPen(waveColor, 2 + i));
            painter.setBrush(Qt::NoBrush);
            
            // Draw the wave circle
            painter.drawEllipse(m_radarCenter.x() - currentWaveRadius, 
                               m_radarCenter.y() - currentWaveRadius, 
                               currentWaveRadius * 2, currentWaveRadius * 2);
        }
    }
    
    // Draw scanning beam effect - radiating lines from center
    if (m_waveRadius > 10) {
        painter.setPen(QPen(QColor(0, 255, 0, 80), 1));
        for (int angle = 0; angle < 360; angle += 15) {
            double radians = angle * DEG_TO_RAD;
            QPointF beamEnd(m_radarCenter.x() + m_waveRadius * sin(radians),
                           m_radarCenter.y() - m_waveRadius * cos(radians));
            painter.drawLine(m_radarCenter, beamEnd);
        }
    }
    
    // Restore painter state
    painter.restore();
}

void RadarWidget::drawContact(QPainter &painter)
{
    if (!m_hasContact) return;
    
    // Convert polar coordinates to screen position
    QPointF screenPos = polarToCartesian(m_currentContact.bearing, m_currentContact.range);
    screenPos = worldToScreen(screenPos);
    
    // Draw contact with full intensity (no fading)
    QColor contactColor = m_contactColor;
    painter.setPen(QPen(contactColor, 3));
    painter.setBrush(QBrush(contactColor));
    
    // Draw contact as small circle with cross (slightly larger for visibility)
    double radius = 6;
    painter.drawEllipse(screenPos.x() - radius, screenPos.y() - radius, 
                       radius * 2, radius * 2);
    painter.drawLine(screenPos.x() - radius, screenPos.y(), 
                    screenPos.x() + radius, screenPos.y());
    painter.drawLine(screenPos.x(), screenPos.y() - radius,
                    screenPos.x(), screenPos.y() + radius);
    
    // Always draw track ID for the current contact
    QFont font("Arial", 10, QFont::Bold);
    painter.setFont(font);
    painter.drawText(screenPos + QPointF(10, -10), m_currentContact.trackId);
}

void RadarWidget::drawRadarInfo(QPainter &painter)
{
    // Draw radar information panel
    QFont font("Arial", 9);
    painter.setFont(font);
    painter.setPen(QPen(m_gridColor));
    
    QStringList info;
    info << QString("Range: %1 NM").arg(m_rangeNM, 0, 'f', 1);
    info << QString("Sweep: %1 RPM").arg(m_sweepRPM, 0, 'f', 1);
    info << QString("Contact: %1").arg(m_hasContact ? "DETECTED" : "NONE");
    info << QString("Mode: %1").arg(m_sweepEnabled ? "ACTIVE" : "STANDBY");
    
    QRect infoRect(10, 10, 120, info.size() * 20 + 10);
    painter.fillRect(infoRect, QColor(0, 0, 0, 100));
    painter.drawRect(infoRect);
    
    for (int i = 0; i < info.size(); ++i) {
        painter.drawText(15, 25 + i * 15, info[i]);
    }
}

QPointF RadarWidget::polarToCartesian(double bearing, double range) const
{
    double radians = bearing * DEG_TO_RAD;
    double x = range * sin(radians);
    double y = -range * cos(radians); // Negative because screen Y increases downward
    return QPointF(x, y);
}

QPointF RadarWidget::worldToScreen(const QPointF &worldPos) const
{
    double scale = m_radarRadius / m_rangeNM;
    return m_radarCenter + QPointF(worldPos.x() * scale, worldPos.y() * scale);
}

QPointF RadarWidget::screenToWorld(const QPointF &screenPos) const
{
    double scale = m_rangeNM / m_radarRadius;
    QPointF offset = screenPos - m_radarCenter;
    return QPointF(offset.x() * scale, offset.y() * scale);
}

double RadarWidget::calculateBearing(double lat1, double lon1, double lat2, double lon2) const
{
    double dLon = (lon2 - lon1) * DEG_TO_RAD;
    double lat1Rad = lat1 * DEG_TO_RAD;
    double lat2Rad = lat2 * DEG_TO_RAD;
    
    double y = sin(dLon) * cos(lat2Rad);
    double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(dLon);
    
    double bearing = atan2(y, x) * RAD_TO_DEG;
    return fmod(bearing + 360.0, 360.0); // Normalize to 0-360
}

double RadarWidget::calculateRange(double lat1, double lon1, double lat2, double lon2) const
{
    double dLat = (lat2 - lat1) * DEG_TO_RAD;
    double dLon = (lon2 - lon1) * DEG_TO_RAD;
    
    double a = sin(dLat/2) * sin(dLat/2) +
               cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
               sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    
    // Earth radius in nautical miles (approximately 3440.065 NM)
    double earthRadiusNM = 3440.065;
    return earthRadiusNM * c;
}