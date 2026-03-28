/*

  Copyright (c) 2026 Schildkroet

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

#include "PeakCanDriver.h"
#include "PeakCanInterface.h"
#include <core/Backend.h>
#include <driver/GenericCanSetupPage.h>

#include <PCANBasic.h>

PeakCanDriver::PeakCanDriver(Backend &backend)
  : CanDriver(backend),
    setupPage(new GenericCanSetupPage())
{
    QObject::connect(&backend, &Backend::onSetupDialogCreated, setupPage, &GenericCanSetupPage::onSetupDialogCreated);
}

PeakCanDriver::~PeakCanDriver()
{
}

QString PeakCanDriver::getName()
{
    return "PEAK PCAN";
}

bool PeakCanDriver::update()
{
    deleteAllInterfaces();

    DWORD channelCount = 0;
    TPCANStatus status = CAN_GetValue(PCAN_NONEBUS, PCAN_ATTACHED_CHANNELS_COUNT,
                                      &channelCount, sizeof(channelCount));
    if (status != PCAN_ERROR_OK) {
        log_error(QString("PeakCanDriver: PCAN_ATTACHED_CHANNELS_COUNT failed: 0x%1")
                      .arg(status, 0, 16));
        return false;
    }

    if (channelCount == 0) {
        return true;
    }

    QVector<TPCANChannelInformation> channels(channelCount);
    status = CAN_GetValue(PCAN_NONEBUS, PCAN_ATTACHED_CHANNELS,
                          channels.data(),
                          channelCount * sizeof(TPCANChannelInformation));
    if (status != PCAN_ERROR_OK) {
        log_error(QString("PeakCanDriver: PCAN_ATTACHED_CHANNELS failed: 0x%1")
                      .arg(status, 0, 16));
        return false;
    }

    for (const auto &ch : channels) {
        if (ch.channel_condition & PCAN_CHANNEL_AVAILABLE) {
            QString name = QString("%1 (ch 0x%2)")
                               .arg(ch.device_name)
                               .arg(ch.channel_handle, 0, 16);
            createOrUpdateInterface(ch.channel_handle, name);
        }
    }

    return true;
}

PeakCanInterface *PeakCanDriver::createOrUpdateInterface(uint16_t channel, QString name)
{
    for (auto *intf : getInterfaces()) {
        PeakCanInterface *pif = dynamic_cast<PeakCanInterface*>(intf);
        if (pif && pif->getChannel() == channel) {
            pif->setName(name);
            return pif;
        }
    }

    PeakCanInterface *pif = new PeakCanInterface(this, channel, name);
    addInterface(pif);
    return pif;
}
