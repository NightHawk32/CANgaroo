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

#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QThread>

#include "core/Backend.h"
#include "core/BusMessage.h"
#include "core/MeasurementInterface.h"

#include "GrIP/GrIPHandler.h"


int GrIPInterface::_CanCounter = 0, GrIPInterface::_LinCounter = 0;


GrIPInterface::GrIPInterface(GrIPDriver *driver, int index, GrIPHandler *hdl, QString name, bool fd_support, uint32_t manufacturer)
    : BusInterface(reinterpret_cast<CanDriver *>(driver)),
      _manufacturer(manufacturer),
      _idx(index),
      _channel_idx(0),
      _isOpen(false),
      _isOffline(false),
      _isLin(manufacturer == CANIL_LIN),
      _name(name),
      m_GrIPHandler(hdl)
{
    _settings.setBitrate(500000);
    _settings.setSamplePoint(875);

    _config.supports_canfd = fd_support;
    _config.supports_timing = false;

    _channel_idx = (manufacturer == CANIL_CAN) ? _CanCounter++ : _LinCounter++;
    //qDebug() << (_isLin ? "LIN IDX: " : "CAN IDX: ") << _channel_idx;

    if (_isLin)
    {
        _settings.setBusType(BusType::LIN);
        _settings.setLinBaudRate(19200);
    }
    else if (fd_support)
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
    _CanCounter = 0;
    _LinCounter = 0;
}

QString GrIPInterface::getDetailsStr() const
{
    if (_manufacturer == CANIL_LIN)
    {
        return tr("CANIL with LIN support");
    }
    if (_manufacturer == CANIL_CAN)
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

    if (_manufacturer != GrIPInterface::CANIL_CAN)
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
    if (_manufacturer == CANIL_CAN)
    {
        _status.can_state = m_GrIPHandler->CanGetState(_channel_idx);
    }
    else if (_manufacturer == CANIL_LIN)
    {
        _status.can_state = m_GrIPHandler->LinGetState(_channel_idx);
    }
    return true;
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

BusType GrIPInterface::busType() const
{
    return _isLin ? BusType::LIN : BusType::CAN;
}

uint32_t GrIPInterface::getCapabilities()
{
    uint32_t retval = 0;

    if (_isLin)
        return 0;

    if (_manufacturer == GrIPInterface::CANIL_CAN)
    {
        retval =
            BusInterface::capability_auto_restart |
            BusInterface::capability_listen_only;
    }

    if (supportsCanFD())
    {
        retval |= BusInterface::capability_canfd;
    }

    if (supportsTripleSampling())
    {
        retval |= BusInterface::capability_triple_sampling;
    }

    return retval;
}

bool GrIPInterface::updateStatistics()
{
    return updateStatus();
}

void GrIPInterface::resetStatistics()
{
    _status.rx_count = 0;
    _status.rx_errors = 0;
    _status.rx_overruns = 0;
    _status.tx_count = 0;
    _status.tx_errors = 0;
    _status.tx_dropped = 0;

    BusInterface::resetStatistics();
}

uint32_t GrIPInterface::getState()
{
    switch (_status.can_state)
    {
    case CANIL_CAN_State::CAN_Active:
        return state_ok;
    case CANIL_CAN_State::CAN_ErrorWarning:
        return state_warning;
    case CANIL_CAN_State::CAN_ErrorPassiv:
        return state_passive;
    case CANIL_CAN_State::CAN_Off:
        return state_bus_off;
    case CANIL_CAN_State::CAN_Stopped:
        return state_stopped;
    default:
        return state_unknown;
    }
   return state_unknown;
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

    m_GrIPHandler->SetStatus(true);

    if (_manufacturer == CANIL_CAN)
    {
        // Disable the channel before reconfiguring to avoid spurious traffic.
        m_GrIPHandler->CanEnableChannel(_channel_idx, false);
        QThread::msleep(2);

        // Apply bit rate — use custom value if set, otherwise use the selected preset.
        const uint32_t baud = _settings.isCustomBitrate() ? _settings.customBitrate() : _settings.bitrate();

        m_GrIPHandler->CanSetConfig(_channel_idx, baud > 0 ? baud : 500000, _settings.isListenOnlyMode(), true, _settings.doAutoRestart());
        m_GrIPHandler->CanEnableChannel(_channel_idx, true);
    }
    else if (_manufacturer == CANIL_LIN)
    {
        // Disable the channel before reconfiguring to avoid spurious traffic.
        m_GrIPHandler->LinEnableChannel(_channel_idx, false);
        QThread::msleep(2);

        m_GrIPHandler->LinSetConfig(
            _channel_idx,
            _settings.linBaudRate(),
            _settings.linNodeMode() == LinNodeMode::Master,
            static_cast<uint8_t>(_settings.linProtocolVersion()),
            _settings.linTimebaseMs(),
            _settings.linJitterUs()
        );
        m_GrIPHandler->LinEnableChannel(_channel_idx, true);
    }

    _isOpen = true;
    _isOffline = false;
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

    m_GrIPHandler->SetStatus(false);

    if (_manufacturer == CANIL_CAN)
    {
        m_GrIPHandler->CanEnableChannel(_channel_idx, false);
    }
    else if (_manufacturer == CANIL_LIN)
    {
        m_GrIPHandler->LinEnableChannel(_channel_idx, false);
    }
}

bool GrIPInterface::isOpen()
{
    return _isOpen;
}

void GrIPInterface::sendMessage(const BusMessage &msg)
{
    QMutexLocker locker(&_serport_mutex);

    if (_manufacturer == CANIL_CAN)
    {
        if (!m_GrIPHandler->CanTransmit(_channel_idx, msg))
        {
            _status.tx_errors++;
        }
    }
    else if (_manufacturer == CANIL_LIN)
    {

    }
}

bool GrIPInterface::readMessage(QList<BusMessage> &msglist, unsigned int timeout_ms)
{
    Q_UNUSED(timeout_ms);

    if (_manufacturer == CANIL_CAN)
    {
        while (m_GrIPHandler->CanAvailable(_channel_idx))
        {
            auto msg = m_GrIPHandler->CanReceive(_channel_idx);
            msg.setInterfaceId(getId());

            if (!msg.isRX())
            {
                // TX echo frame
                if (!msg.isErrorFrame())
                {
                    _status.tx_count++;
                    addFrameBits(msg);
                }
                else
                {
                    _status.tx_errors++;
                }
                msglist.append(msg);
            }
            else
            {
                msglist.append(msg);
                _status.rx_count++;
                addFrameBits(msg);
            }
        }
    }
    else if (_manufacturer == CANIL_LIN)
    {
        while (m_GrIPHandler->LinAvailable(_channel_idx))
        {
            auto msg = m_GrIPHandler->LinReceive(_channel_idx);
            msg.setInterfaceId(getId());

            if (msg.isRX())
            {
                _status.rx_count++;
            }
            else
            {
                _status.tx_count++;
            }
            msglist.append(msg);
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

    return true;
}
