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

#include "VectorDriver.h"
#include "VectorInterface.h"
#include "core/Backend.h"
#include "driver/GenericCanSetupPage.h"

#include <QCanBus>

VectorDriver::VectorDriver(Backend &backend)
  : CanDriver(backend),
    setupPage(new GenericCanSetupPage())
{
    QObject::connect(&backend, &Backend::onSetupDialogCreated,
                     setupPage, &GenericCanSetupPage::onSetupDialogCreated);
}

VectorDriver::~VectorDriver()
{
}

QString VectorDriver::getName()
{
    return "Vector";
}

bool VectorDriver::update()
{
    deleteAllInterfaces();

    if (!QCanBus::instance()->plugins().contains(QStringLiteral("vectorcan"))) {
        return true;
    }

    QString errorString;
    const QList<QCanBusDeviceInfo> devices =
        QCanBus::instance()->availableDevices(QStringLiteral("vectorcan"), &errorString);

    if (!errorString.isEmpty()) {
        log_error(QString("VectorDriver: failed to enumerate devices: %1").arg(errorString));
        return false;
    }

    for (const QCanBusDeviceInfo &info : devices) {
        QString desc = QString("%1 (ch %2)").arg(info.description()).arg(info.channel());
        createOrUpdateInterface(info.name(), desc);
    }

    return true;
}

VectorInterface *VectorDriver::createOrUpdateInterface(QString deviceName, QString description)
{
    for (auto *intf : getInterfaces()) {
        VectorInterface *vif = dynamic_cast<VectorInterface*>(intf);
        if (vif && vif->getDeviceName() == deviceName) {
            vif->setName(description);
            return vif;
        }
    }

    VectorInterface *vif = new VectorInterface(this, deviceName, description);
    addInterface(vif);
    return vif;
}
