CONFIG += c++20

SOURCES += \
    $$PWD/KvaserInterface.cpp \
    $$PWD/KvaserDriver.cpp

HEADERS  += \
    $$PWD/KvaserInterface.h \
    $$PWD/KvaserDriver.h

unix:LIBS += -lcanlib

# Windows: Kvaser CANlib SDK.
# Option 1: Extract the SDK to canlib-sdk/ next to this file.
# Option 2: Set CANLIB_DIR via environment or qmake argument.
win32 {
    isEmpty(CANLIB_DIR) {
        exists($$PWD/canlib-sdk/INC/canlib.h) {
            CANLIB_DIR = $$PWD/canlib-sdk
        } else {
            CANLIB_DIR = $$(CANLIB_DIR)
        }
    }
    isEmpty(CANLIB_DIR): error("KvaserDriver: Kvaser CANlib SDK not found. \
        Either extract it to $$PWD/canlib-sdk/ or set CANLIB_DIR.")
    INCLUDEPATH += $$CANLIB_DIR/INC
    LIBS        += $$CANLIB_DIR/Lib/canlib32.lib
}

FORMS +=
