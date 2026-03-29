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

#include "TinyCanDriver.h"
#include "TinyCanInterface.h"
#include <core/Backend.h>
#include <driver/GenericCanSetupPage.h>

#include <QCanBus>

TinyCanDriver::TinyCanDriver(Backend &backend)
  : CanDriver(backend),
    setupPage(new GenericCanSetupPage())
{
    QObject::connect(&backend, &Backend::onSetupDialogCreated,
                     setupPage, &GenericCanSetupPage::onSetupDialogCreated);
}

TinyCanDriver::~TinyCanDriver()
{
}

QString TinyCanDriver::getName()
{
    return "TinyCAN";
}

bool TinyCanDriver::update()
{
    deleteAllInterfaces();

    if (!QCanBus::instance()->plugins().contains(QStringLiteral("tinycan"))) {
        return true;
    }

    QString errorString;
    const QList<QCanBusDeviceInfo> devices = QCanBus::instance()->availableDevices(QStringLiteral("tinycan"), &errorString);

    if (!errorString.isEmpty()) {
        log_error(QString("TinyCanDriver: failed to enumerate devices: %1").arg(errorString));
        return false;
    }

    for (const QCanBusDeviceInfo &info : devices) {
        QString desc = info.description().isEmpty() ? QString("TinyCAN %1").arg(info.name()) : QString("%1 (%2)").arg(info.description(), info.name());
        createOrUpdateInterface(info.name(), desc);
    }

    return true;
}

TinyCanInterface *TinyCanDriver::createOrUpdateInterface(QString deviceName, QString description)
{
    for (auto *intf : getInterfaces()) {
        TinyCanInterface *tif = dynamic_cast<TinyCanInterface*>(intf);
        if (tif && tif->getDeviceName() == deviceName) {
            tif->setName(description);
            return tif;
        }
    }

    TinyCanInterface *tif = new TinyCanInterface(this, deviceName, description);
    addInterface(tif);
    return tif;
}
