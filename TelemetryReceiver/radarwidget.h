#ifndef RADARWIDGET_H
#define RADARWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QDateTime>
#include <QVector>
#include <QPointF>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <cmath>
#include "telemetryreceiversocket.h"

struct RadarContact {
    QPointF position;        // Relative position (meters from radar center)
    double bearing;          // Bearing in degrees (0-360, 0=North)
    double range;           // Range in nautical miles
    QDateTime timestamp;     // When contact was detected
    double strength;        // Signal strength (0.0-1.0)
    QString trackId;        // Track identifier
    double latitude;        // Original latitude
    double longitude;       // Original longitude
    
    RadarContact() : bearing(0), range(0), strength(1.0), latitude(0), longitude(0) {}
    RadarContact(double bear, double rng, double lat, double lon, double str = 1.0, const QString& id = "")
        : bearing(bear), range(rng), strength(str), trackId(id), latitude(lat), longitude(lon), timestamp(QDateTime::currentDateTime()) {}
};

class RadarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RadarWidget(QWidget *parent = nullptr);
    ~RadarWidget();

    // Radar configuration
    void setRange(double nauticalMiles);
    double getRange() const { return m_rangeNM; }
    
    void setSweepSpeed(double rpm);
    double getSweepSpeed() const { return m_sweepRPM; }
    

public slots:
    void addTelemetryContact(const TelemetryData &data);
    void clearContact();
    void toggleSweep(bool enabled);

signals:
    void contactSelected(const RadarContact &contact);
    void rangeChanged(double nauticalMiles);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void updateSweep();

private:
    // Drawing methods
    void drawRadarBackground(QPainter &painter);
    void drawRangeRings(QPainter &painter);
    void drawBearingLines(QPainter &painter);
    void drawCompassRose(QPainter &painter);
    void drawScanningWave(QPainter &painter);
    void drawContact(QPainter &painter);
    void drawRadarInfo(QPainter &painter);
    
    // Coordinate conversion
    QPointF polarToCartesian(double bearing, double range) const;
    QPointF worldToScreen(const QPointF &worldPos) const;
    QPointF screenToWorld(const QPointF &screenPos) const;
    double calculateBearing(double lat1, double lon1, double lat2, double lon2) const;
    double calculateRange(double lat1, double lon1, double lat2, double lon2) const;
    
    // Radar parameters
    double m_rangeNM;                    // Radar range in nautical miles
    double m_sweepRPM;                   // Sweep rotation speed (RPM)
    double m_waveRadius;                 // Current wave radius
    bool m_sweepEnabled;                 // Sweep animation enabled
    
    // Visual parameters
    QPointF m_radarCenter;               // Center of radar display
    double m_radarRadius;                // Radius of radar display
    int m_numRangeRings;                 // Number of range rings
    QColor m_backgroundColor;            // Radar background color
    QColor m_gridColor;                  // Grid and text color
    QColor m_sweepColor;                 // Sweep line color
    QColor m_contactColor;               // Contact color
    
    // Animation and data
    QTimer *m_sweepTimer;                // Sweep animation timer
    RadarContact m_currentContact;       // Current single contact
    bool m_hasContact;                   // Whether we have a valid contact
    
    // Reference position (radar location)
    double m_radarLat;                   // Radar latitude
    double m_radarLon;                   // Radar longitude
    
    // Constants
    static constexpr double NAUTICAL_MILE_TO_METERS = 1852.0;
    static constexpr double DEG_TO_RAD = M_PI / 180.0;
    static constexpr double RAD_TO_DEG = 180.0 / M_PI;
};

#endif // RADARWIDGET_H