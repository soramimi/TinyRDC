TARGET = TinyRDC
QT       += core gui widgets
CONFIG += c++17

# SIMD最適化フラグ
QMAKE_CXXFLAGS += -msse2 -msse4.1 -mavx
# ARM環境用（条件付き）
linux-aarch64 {
    QMAKE_CXXFLAGS += -march=armv8-a+simd
}

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

