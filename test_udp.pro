QT += core network
QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = test_udp
TEMPLATE = app

SOURCES += test_udp.cpp