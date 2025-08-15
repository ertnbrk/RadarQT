#include <QCoreApplication>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QDebug>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    if (argc < 2) {
        std::cout << "Usage: test_udp [sender|receiver]" << std::endl;
        return 1;
    }
    
    QString mode = argv[1];
    
    if (mode == "receiver") {
        QUdpSocket *socket = new QUdpSocket();
        
        if (socket->bind(QHostAddress::Any, 12345)) {
            std::cout << "UDP Receiver listening on port 12345" << std::endl;
            
            QObject::connect(socket, &QUdpSocket::readyRead, [socket]() {
                while (socket->hasPendingDatagrams()) {
                    QByteArray datagram;
                    datagram.resize(socket->pendingDatagramSize());
                    QHostAddress sender;
                    quint16 senderPort;
                    
                    socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
                    std::cout << "Received: " << datagram.toStdString() << " from " << sender.toString().toStdString() << ":" << senderPort << std::endl;
                }
            });
        } else {
            std::cout << "Failed to bind to port 12345: " << socket->errorString().toStdString() << std::endl;
            return 1;
        }
    } 
    else if (mode == "sender") {
        QUdpSocket *socket = new QUdpSocket();
        QTimer *timer = new QTimer();
        int counter = 0;
        
        QObject::connect(timer, &QTimer::timeout, [socket, &counter]() {
            QString message = QString("Test message %1").arg(++counter);
            QByteArray data = message.toUtf8();
            
            qint64 sent = socket->writeDatagram(data, QHostAddress::LocalHost, 12345);
            if (sent != -1) {
                std::cout << "Sent: " << message.toStdString() << " (" << sent << " bytes)" << std::endl;
            } else {
                std::cout << "Failed to send: " << socket->errorString().toStdString() << std::endl;
            }
        });
        
        timer->start(1000); // Send every second
        std::cout << "UDP Sender started, sending to localhost:12345" << std::endl;
    }
    
    return app.exec();
}