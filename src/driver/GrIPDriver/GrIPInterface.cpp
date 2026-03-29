/*

  Copyright (c) 2025-2026 Schildkroet

  This file is part of cangaroo.

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

#include "GrIPInterface.h"
#include <QDebug>
#include <QDateTime>

#include <core/Backend.h>
#include <core/MeasurementInterface.h>
#include <core/CanMessage.h>

#include <QString>
#include <QStringList>
#include <QThread>

#include "GrIP/GrIPHandler.h"


GrIPInterface::GrIPInterface(GrIPDriver *driver, int index, GrIPHandler *hdl, QString name, bool fd_support, uint32_t manufacturer)
    : CanInterface(reinterpret_cast<CanDriver*>(driver)),
    _manufacturer(manufacturer),
    _idx(index),
    _isOpen(false),
    _isOffline(false),
    _name(name),
    m_GrIPHandler(hdl)
{
    _settings.setBitrate(500000);
    _settings.setSamplePoint(875);

    _config.supports_canfd = fd_support;
    _config.supports_timing = false;

    if (fd_support)
    {
        _settings.setFdBitrate(2000000);
        _settings.setFdSamplePoint(750);
    }

    _status.can_state = state_bus_off;
    _status.rx_count = 0;
    _status.rx_errors = 0;
    _status.rx_overruns = 0;
    _status.tx_count = 0;
    _status.tx_errors = 0;
    _status.tx_dropped = 0;

    _lastStateMsec = QDateTime::currentMSecsSinceEpoch();
    _lastReadMsec = QDateTime::currentMSecsSinceEpoch();
}

GrIPInterface::~GrIPInterface()
{
}

QString GrIPInterface::getDetailsStr() const
{
    if (_manufacturer == CANIL)
    {
        return _config.supports_canfd
            ? tr("CANIL with CANFD support")
            : tr("CANIL with standard CAN support");
    }
    return tr("Not Supported");
}


QString GrIPInterface::getName() const
{
    return _name;
}

void GrIPInterface::setName(QString name)
{
    _name = name;
}

QList<CanTiming> GrIPInterface::getAvailableBitrates()
{
    QList<CanTiming> retval;

    if (_manufacturer != GrIPInterface::CANIL)
    {
        return retval;
    }

    const QList<unsigned> bitrates = {10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000};
    const QList<unsigned> bitrates_fd = {2000000, 5000000};
    const QList<unsigned> samplePoints = {875};
    const QList<unsigned> samplePoints_fd = {750};

    unsigned i = 0;
    for (unsigned br : bitrates)
    {
        for (unsigned br_fd : bitrates_fd)
        {
            for (unsigned sp : samplePoints)
            {
                for (unsigned sp_fd : samplePoints_fd)
                {
                    retval << CanTiming(i++, br, br_fd, sp, sp_fd);
                }
            }
        }
    }

    return retval;
}

void GrIPInterface::applyConfig(const MeasurementInterface &mi)
{
    _settings = mi;
}

bool GrIPInterface::updateStatus()
{
    return false;
}

bool GrIPInterface::readConfig()
{
    return false;
}

bool GrIPInterface::readConfigFromLink(rtnl_link *link)
{
    Q_UNUSED(link);
    return false;
}

bool GrIPInterface::supportsTimingConfiguration()
{
    return _config.supports_timing;
}

bool GrIPInterface::supportsCanFD()
{
    return _config.supports_canfd;
}

bool GrIPInterface::supportsTripleSampling()
{
    return false;
}

unsigned GrIPInterface::getBitrate()
{
    return _settings.bitrate();
}

uint32_t GrIPInterface::getCapabilities()
{
    uint32_t retval = 0;

    if (_manufacturer == GrIPInterface::CANIL)
    {
        retval =
            CanInterface::capability_auto_restart |
            CanInterface::capability_listen_only;
    }

    if (supportsCanFD())
    {
        retval |= CanInterface::capability_canfd;
    }

    if (supportsTripleSampling())
    {
        retval |= CanInterface::capability_triple_sampling;
    }

    return retval;
}

bool GrIPInterface::updateStatistics()
{
    return updateStatus();
}

uint32_t GrIPInterface::getState()
{
    return _status.can_state;
}

int GrIPInterface::getNumRxFrames()
{
    return _status.rx_count;
}

int GrIPInterface::getNumRxErrors()
{
    return _status.rx_errors;
}

int GrIPInterface::getNumTxFrames()
{
    return _status.tx_count;
}

int GrIPInterface::getNumTxErrors()
{
    return _status.tx_errors;
}

int GrIPInterface::getNumRxOverruns()
{
    return _status.rx_overruns;
}

int GrIPInterface::getNumTxDropped()
{
    return _status.tx_dropped;
}

int GrIPInterface::getIfIndex()
{
    return _idx;
}

QString GrIPInterface::getVersion()
{
    return _version;
}

void GrIPInterface::open()
{
    if (m_GrIPHandler == nullptr)
    {
        _isOpen = false;
        _isOffline = true;
        return;
    }

    // Poll for the firmware version string populated by GrIPHandler after
    // connecting. Typically available within one or two iterations.
    for (int i = 0; i < 15; i++)
    {
        _version = QString::fromStdString(m_GrIPHandler->GetVersion());
        if (!_version.isEmpty())
        {
            break;
        }
        QThread::msleep(2);
    }

    // Disable the channel before reconfiguring to avoid spurious traffic.
    m_GrIPHandler->EnableChannel(_idx, false);
    QThread::msleep(2);

    // Apply bit rate — use custom value if set, otherwise use the selected preset.
    const uint32_t baud = _settings.isCustomBitrate()
        ? _settings.customBitrate()
        : _settings.bitrate();
    m_GrIPHandler->CAN_SetBaudrate(_idx, baud > 0 ? baud : 10000);

    m_GrIPHandler->Mode(_idx, _settings.isListenOnlyMode());

    m_GrIPHandler->SetStatus(true);
    m_GrIPHandler->SetEchoTx(true);
    m_GrIPHandler->EnableChannel(_idx, true);

    _isOpen = true;
    _isOffline = false;
    _status.can_state = state_ok;
    _status.rx_count = 0;
    _status.rx_errors = 0;
    _status.rx_overruns = 0;
    _status.tx_count = 0;
    _status.tx_errors = 0;
    _status.tx_dropped = 0;
}

void GrIPInterface::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError)
    {
        _isOffline = true;
    }

    static const auto toErrorString = [](QSerialPort::SerialPortError err) -> QString
    {
        switch (err)
        {
        case QSerialPort::NoError:
            return {};
        case QSerialPort::DeviceNotFoundError:
            return QStringLiteral("Device not found");
        case QSerialPort::PermissionError:
            return QStringLiteral("Permission denied");
        case QSerialPort::OpenError:
            return QStringLiteral("Open error");
        case QSerialPort::WriteError:
            return QStringLiteral("Write error");
        case QSerialPort::ReadError:
            return QStringLiteral("Read error");
        case QSerialPort::ResourceError:
            return QStringLiteral("Resource error");
        case QSerialPort::UnsupportedOperationError:
            return QStringLiteral("Unsupported operation");
        case QSerialPort::TimeoutError:
            return {};
        case QSerialPort::NotOpenError:
            return QStringLiteral("Not open error");
        default:
            return QStringLiteral("Unknown error");
        }
    };

    const QString msg = toErrorString(error);
    if (!msg.isEmpty())
    {
        qWarning() << "Serial port error:" << msg;
    }
}

void GrIPInterface::close()
{
    _isOpen = false;
    _status.can_state = state_bus_off;

    m_GrIPHandler->EnableChannel(_idx, false);
    m_GrIPHandler->SetStatus(false);
}

bool GrIPInterface::isOpen()
{
    return _isOpen;
}

void GrIPInterface::sendMessage(const CanMessage &msg)
{
    QMutexLocker locker(&_serport_mutex);

    if (!m_GrIPHandler->CanTransmit(_idx, msg))
    {
        _status.tx_errors++;
        _status.can_state = state_tx_fail;
    }
}

bool GrIPInterface::readMessage(QList<CanMessage> &msglist, unsigned int timeout_ms)
{
    Q_UNUSED(timeout_ms);

    // Rate-limit processing to roughly once every 2 ms.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - _lastReadMsec < 1)
    {
        return false;
    }
    _lastReadMsec = now + 1;

    while (m_GrIPHandler->CanAvailable(_idx))
    {
        auto msg = m_GrIPHandler->ReceiveCan(_idx);
        if (msg.getId() != 0)
        {
            msg.setInterfaceId(getId());

            if (!msg.isRX())
            {
                // TX echo frame
                if (!msg.isErrorFrame())
                {
                    _status.tx_count++;
                    _status.can_state = state_tx_success;
                }
                else
                {
                    _status.tx_errors++;
                    _status.can_state = state_tx_fail;
                }

                if (msg.isShow())
                {
                    msglist.append(msg);
                }
            }
            else
            {
                msglist.append(msg);
                _status.rx_count++;
            }
        }
    }

    QThread::msleep(1);

    if (_isOffline)
    {
        if (_isOpen)
        {
            close();
        }
        return false;
    }

    // Clear any error state after 3 seconds of stable operation.
    if (QDateTime::currentMSecsSinceEpoch() - _lastStateMsec > 3000)
    {
        _status.can_state = state_ok;
    }

    return true;
}
