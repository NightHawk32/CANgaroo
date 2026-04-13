CONFIG += c++20

# Qt serialbus module provides the vectorcan plugin backend.
# The Vector XL Driver Library must be installed on the target machine.
QT += serialbus
DEFINES += VECTOR_DRIVER

SOURCES += \
    $$PWD/VectorInterface.cpp \
    $$PWD/VectorDriver.cpp

HEADERS += \
    $$PWD/VectorInterface.h \
    $$PWD/VectorDriver.h

FORMS +=
