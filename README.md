# Ship Telemetry & Radar System

A professional-grade ship telemetry and radar visualization system built with Qt/C++. Features real-time position tracking, reliable UDP+ACK communication protocol, and authentic maritime radar display with wave-based scanning animation.

## 🚢 Features

### Real-time Ship Tracking
- **Live position updates** with configurable movement patterns
- **Smooth interpolation** for missing data packets
- **Bearing and range calculation** from radar center
- **Speed conversion** between km/h and nautical knots

### Advanced Radar Display
- **Circular maritime radar** with authentic green-on-black styling
- **Wave-based scanning animation** emanating from center
- **Range rings** with nautical mile markings
- **Compass rose** with cardinal directions (N, NE, E, SE, S, SW, W, NW)
- **Single contact tracking** showing current ship position
- **Configurable range** (50-1000 NM) with mouse wheel zoom

### Reliable UDP+ACK Protocol
- **Hybrid UDP protocol** with acknowledgment mechanism
- **Sequence numbering** for packet ordering
- **Automatic retransmission** (up to 3 attempts)
- **Thread-safe buffering** with non-blocking I/O
- **Packet loss detection** and interpolation
- **Real-time network statistics** with loss rate monitoring

## 🎛️ System Components

### TelemetrySender (Ship Simulator)
Simulates a ship transmitting its position and telemetry data.

**Configuration Options:**
- **Starting Position**: Latitude/Longitude with 6-decimal precision
- **Movement Pattern**: Configurable lat/lon increments per movement
- **Speed Control**: 0-100 km/h ship speed
- **Timing Control**: 
  - Send interval: 100ms - 10s
  - Movement interval: 1-60 seconds
- **Reliability Settings**: ACK timeout, max retransmissions

### TelemetryReceiver (Radar Station)
Receives and displays ship telemetry on a professional radar interface.

**Radar Controls:**
- **Range**: 50-1000 nautical miles
- **Sweep Speed**: 1-60 RPM wave animation
- **Contact Display**: Real-time position with bearing/range data
- **Recording/Playback**: Capture and replay telemetry sessions

## 🔧 Technical Architecture

### Communication Protocol
```
UDP + ACK Hybrid Protocol
├── Sequence Numbers (packet ordering)
├── Timestamps (millisecond precision)
├── ACK Mechanism (reliability)
├── Timeout & Retransmission (3s/3 attempts)
└── JSON Payload Format
```

### Data Structures
```cpp
struct TelemetryPacket {
    quint32 sequenceNumber;
    QDateTime timestamp;
    double latitude;
    double longitude;
    double speed;
    QString status;
    bool needsAck;
};
```

### Thread Safety
- **QReadWriteLock** for data protection
- **QMutex** for socket operations
- **Atomic counters** for statistics
- **Non-blocking I/O** operations

### Loss Recovery Mechanisms
1. **Packet Loss Detection**: Missing sequence number identification
2. **Linear Interpolation**: Calculate missing positions from adjacent packets
3. **Last Valid Data**: Fallback to previous known position
4. **Timeout Management**: Configurable packet timeout (5s default)

## 🚀 Getting Started

### Prerequisites
- **Qt 5.15+** or **Qt 6.x**
- **CMake 3.16+** or **qmake**
- **C++17** compatible compiler
- **Network permissions** for UDP communication

### Building the Project

#### Using CMake
```bash
# Clone or extract the project
cd SensorPanel

# Build Receiver
cd TelemetryReceiver
mkdir build && cd build
cmake ..
make

# Build Sender
cd ../../TelemetrySender
mkdir build && cd build
cmake ..
make
```

#### Using qmake
```bash
# Build Receiver
cd TelemetryReceiver
qmake TelemetryReceiver.pro
make

# Build Sender
cd ../TelemetrySender
qmake TelemetrySender.pro
make
```

### Running the System

1. **Start Receiver** (Radar Station)
   ```bash
   cd TelemetryReceiver
   ./TelemetryReceiver
   ```

2. **Start Sender** (Ship Simulator)
   ```bash
   cd TelemetrySender
   ./TelemetrySender
   ```

3. **Configure Ship Movement**
   - Set starting position (latitude/longitude)
   - Configure movement increments
   - Set speed and transmission intervals
   - Click "Start Sending"

4. **Monitor Radar Display**
   - Observe ship position on radar
   - Monitor network statistics
   - Adjust radar range and sweep speed

## 📊 Network Statistics

### Real-time Monitoring
- **Packets Received**: Total successful receptions
- **Packet Loss Rate**: Percentage with color coding
  - 🟢 < 1%: Excellent (Green)
  - 🟠 1-5%: Acceptable (Orange)
  - 🔴 > 5%: Poor (Red)
- **Interpolated Packets**: Missing data estimations
- **Retransmissions**: Failed delivery attempts

### Performance Tuning
```cpp
// Receiver Configuration
receiver->setInterpolationEnabled(true);
receiver->setMaxBufferSize(1000);
receiver->setPacketTimeoutMs(5000);

// Sender Configuration
sender->setAckTimeoutMs(3000);
sender->setMaxRetransmissions(3);
sender->setReliabilityEnabled(true);
```

## 🎯 Usage Examples

### Basic Ship Tracking
```cpp
// Configure starting position (Istanbul area)
latitude: 41.000000°
longitude: 29.000000°
speed: 25 km/h

// Set movement pattern (northeast)
latitude increment: +0.01°
longitude increment: +0.01°
movement interval: 3 seconds
```

### Network Simulation
```cpp
// High-frequency updates
send interval: 500ms
movement interval: 2s

// Reliability testing
ACK timeout: 2s
max retransmissions: 5
```

## 🔧 Configuration Parameters

### Sender Parameters
| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Latitude | -90° to +90° | 39.0° | Ship starting latitude |
| Longitude | -180° to +180° | 35.5° | Ship starting longitude |
| Speed | 0-100 km/h | 25 km/h | Ship speed |
| Lat Increment | -1° to +1° | 0.01° | Latitude change per movement |
| Lon Increment | -1° to +1° | 0.01° | Longitude change per movement |
| Send Interval | 100ms-10s | 1s | Telemetry transmission rate |
| Movement Interval | 1-60s | 3s | Position update frequency |

### Receiver Parameters
| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Radar Range | 50-1000 NM | 500 NM | Maximum detection range |
| Sweep Speed | 1-60 RPM | 12 RPM | Wave animation speed |
| Buffer Size | 100-10000 | 1000 | Packet buffer capacity |
| Packet Timeout | 1-30s | 5s | Missing packet timeout |
| Interpolation | On/Off | On | Enable position interpolation |

## 🐛 Troubleshooting

### Common Issues

**No Connection**
- Check firewall settings for UDP port 12345
- Ensure both applications run on same machine
- Verify network interface availability

**High Packet Loss**
- Reduce send interval (increase time between packets)
- Check system network load
- Increase ACK timeout duration

**Interpolation Errors**
- Verify movement increments are reasonable
- Check for large position jumps
- Monitor sequence number continuity

**Performance Issues**
- Reduce radar sweep speed
- Limit buffer size
- Disable debug logging in release builds

### Debug Mode
Enable verbose logging by uncommenting debug statements:
```cpp
qDebug() << "ReliableUDP: Packet" << sequenceNumber << "received";
```

## 📚 API Reference

### ReliableUdpReceiver
```cpp
// Core functionality
bool startListening(quint16 port = 12345);
void setInterpolationEnabled(bool enabled);
void setMaxBufferSize(int size);

// Statistics
int getPacketsReceived() const;
double getPacketLossRate() const;

// Signals
void telemetryDataReceived(const TelemetryPacket &packet);
void statisticsUpdated();
```

### ReliableUdpSender
```cpp
// Configuration
void setTarget(const QHostAddress &address, quint16 port);
void setReliabilityEnabled(bool enabled);
void sendTelemetryData(const TelemetryPacket &packet);

// Signals
void ackReceived(quint32 sequenceNumber);
void packetTimeout(quint32 sequenceNumber);
```

### RadarWidget
```cpp
// Display control
void setRange(double nauticalMiles);
void setSweepSpeed(double rpm);
void toggleSweep(bool enabled);

// Data input
void addTelemetryContact(const TelemetryData &data);
void clearContact();
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Qt Framework for cross-platform GUI development
- Maritime radar display standards for authentic visualization
- UDP protocol specifications for reliable communication design

## 📞 Support

For technical support or questions:
- Create an issue in the repository
- Check troubleshooting section above
- Review Qt documentation for platform-specific issues

---

**Built with ❤️ for maritime simulation and telemetry applications**