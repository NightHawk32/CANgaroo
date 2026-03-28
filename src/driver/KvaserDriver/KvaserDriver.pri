CONFIG += c++20

SOURCES += \
    $$PWD/KvaserInterface.cpp \
    $$PWD/KvaserDriver.cpp

HEADERS  += \
    $$PWD/KvaserInterface.h \
    $$PWD/KvaserDriver.h

unix:LIBS += -lcanlib

# Windows: point at the Kvaser CANlib SDK.
# Set CANLIB_DIR in your environment or qmake call, e.g.:
#   qmake CANLIB_DIR="C:/Program Files/Kvaser/Canlib"
win32 {
    isEmpty(CANLIB_DIR): CANLIB_DIR = $$(CANLIB_DIR)
    isEmpty(CANLIB_DIR): error("KvaserDriver: CANLIB_DIR is not set. Install the Kvaser CANlib SDK and set CANLIB_DIR.")
    INCLUDEPATH += $$CANLIB_DIR/INC
    LIBS        += $$CANLIB_DIR/Lib/canlib32.lib
}

FORMS +=
