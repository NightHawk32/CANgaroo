CONFIG += c++20

# Qt serialbus module provides the tinycan plugin backend.
# The TinyCAN driver/library must be installed on the target machine.
QT += serialbus

SOURCES += \
    $$PWD/TinyCanInterface.cpp \
    $$PWD/TinyCanDriver.cpp

HEADERS += \
    $$PWD/TinyCanInterface.h \
    $$PWD/TinyCanDriver.h

FORMS +=
