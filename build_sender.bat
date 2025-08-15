@echo off
set PATH=C:\Qt\6.9.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;%PATH%
cd TelemetrySender
qmake TelemetrySender.pro
mingw32-make
pause