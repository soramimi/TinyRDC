TARGET = TinyRDC
QT += core gui widgets
CONFIG += c++17

INCLUDEPATH += /usr/include/freerdp3
INCLUDEPATH += /usr/include/winpr3

LIBS += -lfreerdp3 -lfreerdp-client3 -lwinpr3

SOURCES += \
    ConnectionDialog.cpp \
    MyView.cpp \
    main.cpp \
    MainWindow.cpp

HEADERS += \
    ConnectionDialog.h \
    MainWindow.h \
    MyView.h

FORMS += \
    ConnectionDialog.ui \
    MainWindow.ui

