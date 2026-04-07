lessThan(QT_MAJOR_VERSION, 6): error("requires Qt 6")

QT += core gui
QT += widgets
QT += xml
QT += charts
QT += svg

win32:QT += serialport
unix:QT += serialport

TARGET = cangaroo
TEMPLATE = app
CONFIG += warn_on
CONFIG += link_pkgconfig

VERSION = 0.5.3
DEFINES += VERSION_STRING=\\\"$${VERSION}\\\"

TRANSLATIONS = \
    translations/i18n_de_DE.ts \
    translations/i18n_zh_cn.ts \
    translations/i18n_es_ES.ts
RC_ICONS = cangaroo.ico

DESTDIR = ../bin
MOC_DIR = ../build/moc
RCC_DIR = ../build/rcc
UI_DIR = ../build/ui
unix:OBJECTS_DIR = ../build/o/unix
win32:OBJECTS_DIR = ../build/o/win32
macx:OBJECTS_DIR = ../build/o/mac


SOURCES += main.cpp\
    mainwindow.cpp \
    window/ConditionalLoggingDialog.cpp \
    window/SettingsDialog.cpp

HEADERS  += mainwindow.h \
    window/ConditionalLoggingDialog.h \
    window/SettingsDialog.h

FORMS    += mainwindow.ui

RESOURCES = cangaroo.qrc

include($$PWD/core/core.pri)
include($$PWD/driver/driver.pri)
include($$PWD/parser/dbc/dbc.pri)
include($$PWD/parser/ldf/ldf.pri)
include($$PWD/decoders/decoders.pri)
include($$PWD/window/TraceWindow/TraceWindow.pri)
include($$PWD/window/SetupDialog/SetupDialog.pri)
include($$PWD/window/LogWindow/LogWindow.pri)
include($$PWD/window/GraphWindow/GraphWindow.pri)
include($$PWD/window/CanStatusWindow/CanStatusWindow.pri)
include($$PWD/window/RawTxWindow/RawTxWindow.pri)
include($$PWD/window/TxGeneratorWindow/TxGeneratorWindow.pri)
include($$PWD/window/ScriptWindow/ScriptWindow.pri)
include($$PWD/window/ReplayWindow/ReplayWindow.pri)
include($$PWD/helpers/helpers.pri)


unix:PKGCONFIG += python3-embed
unix:INCLUDEPATH += /usr/include/pybind11
win32 {
    # Detect Python include path dynamically via python3-config or python3
    PYTHON_INCLUDE = $$system(python3 -c "import sysconfig; print(sysconfig.get_path('include'))" 2>/dev/null)
    isEmpty(PYTHON_INCLUDE) {
        PYTHON_INCLUDE = $$system(python -c "import sysconfig; print(sysconfig.get_path('include'))" 2>/dev/null)
    }
    !isEmpty(PYTHON_INCLUDE) {
        INCLUDEPATH += $$PYTHON_INCLUDE
    }

    # Detect Python library dynamically via python3-config
    PYTHON_LDFLAGS = $$system(python3-config --ldflags --embed 2>/dev/null)
    isEmpty(PYTHON_LDFLAGS) {
        PYTHON_LDFLAGS = $$system(python3-config --ldflags 2>/dev/null)
    }
    !isEmpty(PYTHON_LDFLAGS) {
        LIBS += $$PYTHON_LDFLAGS
    }

    # Detect pybind11 include path dynamically
    PYBIND11_INCLUDE = $$system(python3 -c "import pybind11; print(pybind11.get_include())" 2>/dev/null)
    !isEmpty(PYBIND11_INCLUDE) {
        INCLUDEPATH += $$PYBIND11_INCLUDE
    }
    # Also add the Python prefix include dir as a fallback (covers MSYS2 system-installed pybind11)
    PYTHON_PREFIX = $$system(python3-config --prefix 2>/dev/null)
    !isEmpty(PYTHON_PREFIX) {
        INCLUDEPATH += $$PYTHON_PREFIX/include
    }
}

unix:PKGCONFIG += libnl-3.0
unix:PKGCONFIG += libnl-route-3.0
unix:INCLUDEPATH += /usr/include/libnl3
unix:include($$PWD/driver/SocketCanDriver/SocketCanDriver.pri)

include($$PWD/driver/CANBlastDriver/CANBlastDriver.pri)
include($$PWD/driver/SLCANDriver/SLCANDriver.pri)
include($$PWD/driver/GrIPDriver/GrIPDriver.pri)

win32:include($$PWD/driver/CandleApiDriver/CandleApiDriver.pri)

# Pass CONFIG+=peakcan to qmake to enable the PEAK PCAN driver.
# Requires the PCAN-Basic SDK extracted to src/driver/PeakCanDriver/pcan-basic-api/.
# Download: https://www.peak-system.com/fileadmin/media/files/PCAN-Basic.zip
win32:peakcan {
    DEFINES += PEAKCAN_DRIVER
    include($$PWD/driver/PeakCanDriver/PeakCanDriver.pri)
}

# Pass CONFIG+=kvaser to qmake to enable the Kvaser CANlib driver.
# Requires the Kvaser CANlib SDK (canlib.h + libcanlib / canlib32.dll).
# On Windows also set CANLIB_DIR=<path to SDK> (see KvaserDriver.pri).
kvaser {
    DEFINES += KVASER_DRIVER
    include($$PWD/driver/KvaserDriver/KvaserDriver.pri)
}

# Vector CAN driver — always enabled via Qt serialbus.
# Requires the Vector XL Driver Library installed on the target machine at runtime.
include($$PWD/driver/VectorDriver/VectorDriver.pri)

# TinyCAN driver — always enabled via Qt serialbus.
# Requires the TinyCAN driver/library installed on the target machine at runtime.
include($$PWD/driver/TinyCanDriver/TinyCanDriver.pri)

DISTFILES += \
    assets/filter-symbolic.svg
