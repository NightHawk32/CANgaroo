CONFIG += c++20

SOURCES += \
    $$PWD/PeakCanInterface.cpp \
    $$PWD/PeakCanDriver.cpp

HEADERS += \
    $$PWD/PeakCanInterface.h \
    $$PWD/PeakCanDriver.h

# PCAN-Basic SDK — extract the downloaded zip to pcan-basic-api/ next to this file.
# Download: https://www.peak-system.com/fileadmin/media/files/PCAN-Basic.zip
PCAN_API_DIR = $$PWD/pcan-basic-api

!exists($$PCAN_API_DIR/Include/PCANBasic.h) {
    error("PeakCanDriver: PCAN-Basic SDK not found at $$PCAN_API_DIR. \
           Download and extract PCAN-Basic.zip to $$PWD/pcan-basic-api/")
}

INCLUDEPATH += $$PCAN_API_DIR/Include
LIBS        += $$PCAN_API_DIR/x64/VC_LIB/PCANBasic.lib

message("PeakCanDriver INCLUDEPATH: $$PCAN_API_DIR/Include")
message("PeakCanDriver LIBS:        $$PCAN_API_DIR/x64/VC_LIB/PCANBasic.lib")

FORMS +=
