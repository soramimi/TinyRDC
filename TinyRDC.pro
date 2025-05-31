TARGET = TinyRDC
QT += core gui widgets
CONFIG += c++17

INCLUDEPATH += /usr/include/freerdp3
INCLUDEPATH += /usr/include/winpr3

LIBS += -lfreerdp3 -lfreerdp-client3 -lwinpr3

SOURCES += \
    ConnectionDialog.cpp \
    Global.cpp \
    MySettings.cpp \
    MyView.cpp \
    joinpath.cpp \
    main.cpp \
    MainWindow.cpp

HEADERS += \
    ConnectionDialog.h \
    Global.h \
    MainWindow.h \
    MySettings.h \
    MyView.h \
    joinpath.h

FORMS += \
    ConnectionDialog.ui \
    MainWindow.ui

