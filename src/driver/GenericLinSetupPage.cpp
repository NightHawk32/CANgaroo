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
#include "core/DBC/LinDb.h"
#include "core/MeasurementInterface.h"
#include "core/MeasurementNetwork.h"
#include "driver/BusInterface.h"
#include "window/SetupDialog/SetupDialog.h"

GenericLinSetupPage::GenericLinSetupPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::GenericLinSetupPage),
      _mi(nullptr),
      _network(nullptr),
      _enableUiUpdates(false)
{
    ui->setupUi(this);

    populateBaudrates();
    populateProtocolVersions();

    connect(ui->cbBaudrate,        &QComboBox::currentIndexChanged, this, [this]() { updateUI(); });
    connect(ui->cbProtocolVersion, &QComboBox::currentIndexChanged, this, [this]() { updateUI(); });
    connect(ui->cbChecksumClassic, &QCheckBox::stateChanged,        this, [this]() { updateUI(); });
    connect(ui->cbWakeupOnBus,     &QCheckBox::stateChanged,        this, [this]() { updateUI(); });

    connect(ui->cbLdfSelect,      &QComboBox::currentIndexChanged, this, &GenericLinSetupPage::onLdfSelected);
    connect(ui->cbScheduleTable,  &QComboBox::currentIndexChanged, this, [this]() { updateUI(); });
    connect(ui->rbMaster,         &QRadioButton::toggled,          this, [this]() { updateUI(); });
    connect(ui->rbSlave,          &QRadioButton::toggled,          this, [this]() { updateUI(); });
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

    _mi      = mi;
    _network = dlg.currentNetwork();

    BusInterface *intf = backend().getInterfaceById(_mi->busInterface());
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

    ui->cbChecksumClassic->setChecked(_mi->linChecksumClassic());
    ui->cbWakeupOnBus->setChecked(_mi->linWakeupOnBus());

    // Populate and enable/disable the LDF group
    populateLdfCombo();

    _enableUiUpdates = true;
    dlg.displayPage(this);
}

void GenericLinSetupPage::updateUI()
{
    if (!_enableUiUpdates || !_mi)
        return;

    _mi->setLinBaudRate(ui->cbBaudrate->currentData().toUInt());
    _mi->setLinProtocolVersion(static_cast<LinProtocolVersion>(ui->cbProtocolVersion->currentIndex()));
    _mi->setLinChecksumClassic(ui->cbChecksumClassic->isChecked());
    _mi->setLinWakeupOnBus(ui->cbWakeupOnBus->isChecked());

    // LDF group
    if (ui->gbLdfConfig->isEnabled())
    {
        int ldfIdx = ui->cbLdfSelect->currentIndex();
        if (_network && ldfIdx >= 0 && ldfIdx < _network->_linDbs.size())
        {
            const auto &ldb = _network->_linDbs.at(ldfIdx);
            _mi->setLinLdfPath(ldb->path());
            _mi->setLinTimebaseMs(static_cast<uint8_t>(qBound(0.0, ldb->masterTimebaseMs(), 255.0)));
            _mi->setLinJitterUs(static_cast<uint16_t>(qBound(0.0, ldb->masterJitterMs() * 1000.0, 65535.0)));
        }
        else
        {
            _mi->setLinLdfPath(QString());
        }

        _mi->setLinScheduleTable(ui->cbScheduleTable->currentText());
        _mi->setLinNodeMode(ui->rbMaster->isChecked() ? LinNodeMode::Master : LinNodeMode::Slave);
    }
}

void GenericLinSetupPage::onLdfSelected(int index)
{
    updateLdfInfo(index);
    updateUI();
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
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 1.3"),  static_cast<int>(LinProtocolVersion::V1_3));
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 2.0"),  static_cast<int>(LinProtocolVersion::V2_0));
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 2.1"),  static_cast<int>(LinProtocolVersion::V2_1));
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 2.2"),  static_cast<int>(LinProtocolVersion::V2_2));
    ui->cbProtocolVersion->addItem(QStringLiteral("LIN 2.2A"), static_cast<int>(LinProtocolVersion::V2_2A));
}

void GenericLinSetupPage::populateLdfCombo()
{
    const bool hasLdfs = _network && !_network->_linDbs.isEmpty();
    ui->gbLdfConfig->setEnabled(hasLdfs);

    _enableUiUpdates = false;

    ui->cbLdfSelect->clear();
    ui->cbScheduleTable->clear();
    ui->laLdfBaudrate->setText(QStringLiteral("-"));
    ui->laLdfProtocol->setText(QStringLiteral("-"));
    ui->laLdfTimebase->setText(QStringLiteral("-"));
    ui->laLdfJitter->setText(QStringLiteral("-"));

    if (!hasLdfs)
    {
        _enableUiUpdates = true;
        return;
    }

    // Populate LDF dropdown
    int selectIdx = 0;
    for (int i = 0; i < _network->_linDbs.size(); ++i)
    {
        const auto &ldb = _network->_linDbs.at(i);
        ui->cbLdfSelect->addItem(ldb->fileName());
        if (ldb->path() == _mi->linLdfPath())
            selectIdx = i;
    }
    ui->cbLdfSelect->setCurrentIndex(selectIdx);

    _enableUiUpdates = true;

    // Trigger info update for the selected LDF
    updateLdfInfo(selectIdx);

    // Restore node mode radio buttons
    ui->rbMaster->setChecked(_mi->linNodeMode() != LinNodeMode::Slave);
    ui->rbSlave->setChecked(_mi->linNodeMode() == LinNodeMode::Slave);
}

void GenericLinSetupPage::updateLdfInfo(int ldfIndex)
{
    if (!_network || ldfIndex < 0 || ldfIndex >= _network->_linDbs.size())
    {
        ui->laLdfBaudrate->setText(QStringLiteral("-"));
        ui->laLdfProtocol->setText(QStringLiteral("-"));
        ui->laLdfTimebase->setText(QStringLiteral("-"));
        ui->laLdfJitter->setText(QStringLiteral("-"));
        ui->cbScheduleTable->clear();
        return;
    }

    const auto &ldb = _network->_linDbs.at(ldfIndex);

    ui->laLdfBaudrate->setText(QString::number(ldb->speedBps()) + QStringLiteral(" bps"));
    ui->laLdfProtocol->setText(QStringLiteral("LIN ") + ldb->protocolVersion());
    ui->laLdfTimebase->setText(QString::number(ldb->masterTimebaseMs()) + QStringLiteral(" ms"));
    ui->laLdfJitter->setText(QString::number(ldb->masterJitterMs()) + QStringLiteral(" ms"));

    // Sync cbProtocolVersion with the LDF-specified protocol
    {
        int pvIdx = ui->cbProtocolVersion->findText(QStringLiteral("LIN ") + ldb->protocolVersion());
        if (pvIdx >= 0)
            ui->cbProtocolVersion->setCurrentIndex(pvIdx);
    }

    // Sync cbBaudrate with the LDF-specified speed
    const unsigned ldfBaud = static_cast<unsigned>(ldb->speedBps());
    int brIdx = ui->cbBaudrate->findData(ldfBaud);
    if (brIdx < 0)
    {
        ui->cbBaudrate->addItem(QString::number(ldfBaud), ldfBaud);
        brIdx = ui->cbBaudrate->count() - 1;
    }
    ui->cbBaudrate->setCurrentIndex(brIdx);

    // Populate schedule table dropdown
    const bool wasUpdating = _enableUiUpdates;
    _enableUiUpdates = false;

    ui->cbScheduleTable->clear();
    for (const QString &name : ldb->scheduleTableNames())
        ui->cbScheduleTable->addItem(name);

    // Restore saved selection
    int tblIdx = ui->cbScheduleTable->findText(_mi->linScheduleTable());
    ui->cbScheduleTable->setCurrentIndex(tblIdx >= 0 ? tblIdx : 0);

    _enableUiUpdates = wasUpdating;
}

Backend &GenericLinSetupPage::backend()
{
    return Backend::instance();
}
