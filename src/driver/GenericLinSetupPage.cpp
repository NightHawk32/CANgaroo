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

#include "GenericLinSetupPage.h"
#include "ui_GenericLinSetupPage.h"
#include "core/Backend.h"
#include "driver/BusInterface.h"
#include "core/MeasurementInterface.h"
#include "window/SetupDialog/SetupDialog.h"

GenericLinSetupPage::GenericLinSetupPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::GenericLinSetupPage),
      _mi(nullptr),
      _enableUiUpdates(false)
{
    ui->setupUi(this);

    populateBaudrates();
    populateProtocolVersions();
    populateNodeModes();

    connect(ui->cbBaudrate,         &QComboBox::currentIndexChanged, this, [this]() { updateUI(); });
    connect(ui->cbProtocolVersion,  &QComboBox::currentIndexChanged, this, [this]() { updateUI(); });
    connect(ui->cbNodeMode,         &QComboBox::currentIndexChanged, this, [this]() { updateUI(); });
    connect(ui->cbChecksumClassic,  &QCheckBox::stateChanged,        this, [this]() { updateUI(); });
    connect(ui->cbWakeupOnBus,      &QCheckBox::stateChanged,        this, [this]() { updateUI(); });
}

GenericLinSetupPage::~GenericLinSetupPage()
{
    delete ui;
}

void GenericLinSetupPage::onSetupDialogCreated(SetupDialog &dlg)
{
    dlg.addPage(this);
    connect(&dlg, &SetupDialog::onShowInterfacePage, this, &GenericLinSetupPage::onShowInterfacePage);
}

void GenericLinSetupPage::onShowInterfacePage(SetupDialog &dlg, MeasurementInterface *mi)
{
    if (mi->busType() != BusType::LIN)
        return;

    _mi = mi;

    BusInterface *intf = backend().getInterfaceById(_mi->canInterface());
    ui->laDriver->setText(intf->getDriver()->getName());
    ui->laInterface->setText(QString("%1 - [ID: %2]").arg(intf->getName()).arg(intf->getId()));
    ui->laInterfaceDetails->setText(intf->getDetailsStr());

    _enableUiUpdates = false;

    // Baud rate
    {
        int idx = ui->cbBaudrate->findData(_mi->linBaudRate());
        ui->cbBaudrate->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    // Protocol version
    ui->cbProtocolVersion->setCurrentIndex(static_cast<int>(_mi->linProtocolVersion()));

    // Node mode
    ui->cbNodeMode->setCurrentIndex(static_cast<int>(_mi->linNodeMode()));

    ui->cbChecksumClassic->setChecked(_mi->linChecksumClassic());
    ui->cbWakeupOnBus->setChecked(_mi->linWakeupOnBus());

    _enableUiUpdates = true;
    dlg.displayPage(this);
}

void GenericLinSetupPage::updateUI()
{
    if (!_enableUiUpdates || !_mi)
        return;

    _mi->setLinBaudRate(ui->cbBaudrate->currentData().toUInt());
    _mi->setLinProtocolVersion(static_cast<LinProtocolVersion>(ui->cbProtocolVersion->currentIndex()));
    _mi->setLinNodeMode(static_cast<LinNodeMode>(ui->cbNodeMode->currentIndex()));
    _mi->setLinChecksumClassic(ui->cbChecksumClassic->isChecked());
    _mi->setLinWakeupOnBus(ui->cbWakeupOnBus->isChecked());
}

void GenericLinSetupPage::populateBaudrates()
{
    static const unsigned baudrates[] = { 1000, 2400, 4800, 9600, 19200, 20000 };
    ui->cbBaudrate->clear();
    for (unsigned br : baudrates)
        ui->cbBaudrate->addItem(QString::number(br), br);
}

void GenericLinSetupPage::populateProtocolVersions()
{
    ui->cbProtocolVersion->clear();
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 1.3"), static_cast<int>(LinProtocolVersion::V1_3));
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 2.0"), static_cast<int>(LinProtocolVersion::V2_0));
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 2.1"), static_cast<int>(LinProtocolVersion::V2_1));
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 2.2"), static_cast<int>(LinProtocolVersion::V2_2));
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 2.2A"), static_cast<int>(LinProtocolVersion::V2_2A));
}

void GenericLinSetupPage::populateNodeModes()
{
    ui->cbNodeMode->clear();
    ui->cbNodeMode->addItem(tr("Master"), static_cast<int>(LinNodeMode::Master));
    ui->cbNodeMode->addItem(tr("Slave"),  static_cast<int>(LinNodeMode::Slave));
}

Backend &GenericLinSetupPage::backend()
{
    return Backend::instance();
}
