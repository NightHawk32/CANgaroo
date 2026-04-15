#pragma once

#include <QWidget>

namespace Ui {
class GenericLinSetupPage;
}

class BusInterface;
class SetupDialog;
class MeasurementInterface;
class Backend;

class GenericLinSetupPage : public QWidget
{
    Q_OBJECT

public:
    explicit GenericLinSetupPage(QWidget *parent = nullptr);
    ~GenericLinSetupPage();

public slots:
    void onSetupDialogCreated(SetupDialog &dlg);
    void onShowInterfacePage(SetupDialog &dlg, MeasurementInterface *mi);

private slots:
    void updateUI();

private:
    Ui::GenericLinSetupPage *ui;
    MeasurementInterface *_mi;
    bool _enableUiUpdates;

    void populateBaudrates();
    void populateProtocolVersions();
    void populateNodeModes();

    Backend &backend();
};
