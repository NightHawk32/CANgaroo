/*

  Copyright (c) 2024 Schildkroet

  This file is part of CANgaroo.

  cangaroo is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  cangaroo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with cangaroo.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include "../CanInterface.h"
#include <core/MeasurementInterface.h>
#include <QtSerialPort/QSerialPort>
#include <QMutex>

#include "GrIP/GrIPHandler.h"


class GrIPDriver;


struct can_config_t
{
    bool supports_canfd;
    bool supports_timing;
    uint32_t state;
    uint32_t base_freq;
    uint32_t sample_point;
    uint32_t ctrl_mode;
    uint32_t restart_ms;
};


struct can_status_t
{
    uint32_t can_state;

    uint64_t rx_count;
    int rx_errors;
    uint64_t rx_overruns;

    uint64_t tx_count;
    int tx_errors;
    uint64_t tx_dropped;
};


class GrIPInterface : public CanInterface
{
    Q_OBJECT

public:
    enum
    {
        CANIL
    };

    GrIPInterface(GrIPDriver *driver, int index, GrIPHandler *hdl, QString name, bool fd_support, uint32_t manufacturer);
    ~GrIPInterface() override;

    QString getDetailsStr() const;
    QString getName() const override;
    void setName(QString name);

    QList<CanTiming> getAvailableBitrates() override;

    void applyConfig(const MeasurementInterface &mi) override;
    bool readConfig();
    bool readConfigFromLink(struct rtnl_link *link);

    bool supportsTimingConfiguration();
    bool supportsCanFD();
    bool supportsTripleSampling();

    unsigned getBitrate() override;
    uint32_t getCapabilities() override;

    void open() override;
    void close() override;
    bool isOpen() override;

    void sendMessage(const CanMessage &msg) override;
    bool readMessage(QList<CanMessage> &msglist, unsigned int timeout_ms) override;

    bool updateStatistics() override;
    uint32_t getState() override;
    int getNumRxFrames() override;
    int getNumRxErrors() override;
    int getNumRxOverruns() override;

    int getNumTxFrames() override;
    int getNumTxErrors() override;
    int getNumTxDropped() override;

    QString getVersion() override;
    bool ShowTxMsg() override { return false; }

    int getIfIndex();

private:
    uint32_t _manufacturer;
    QString _version;

    int _idx;
    bool _isOpen;
    bool _isOffline;
    QMutex _serport_mutex;
    QString _name;

    MeasurementInterface _settings;

    can_config_t _config;
    can_status_t _status;

    qint64 _lastStateMsec;  ///< Timestamp of last open(); used to auto-clear error state after 3 s.
    qint64 _lastReadMsec;   ///< Timestamp of last readMessage() execution; used for rate-limiting.

    GrIPHandler *m_GrIPHandler;

    bool updateStatus();

private slots:
    void handleSerialError(QSerialPort::SerialPortError error);
};
