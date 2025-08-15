QT += core widgets network

CONFIG += c++11
win32:CONFIG += console

TARGET = TelemetryReceiver
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    radarwidget.cpp \
    telemetryreceiversocket.cpp \
    reliableudp.cpp

HEADERS += \
    mainwindow.h \
    radarwidget.h \
    telemetryreceiversocket.h \
    reliableudp.h

FORMS += \
    mainwindow.ui

# Windows specific
win32 {
    DEFINES += _USE_MATH_DEFINES
}

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target