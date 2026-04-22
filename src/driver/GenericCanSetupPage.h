#ifndef GENERICCANSETUPPAGE_H
#define GENERICCANSETUPPAGE_H

#include <QWidget>

namespace Ui {
class GenericCanSetupPage;
}

class BusInterface;
class SetupDialog;
class MeasurementInterface;
class Backend;

class GenericCanSetupPage : public QWidget
{
    Q_OBJECT

public:
    explicit GenericCanSetupPage(QWidget *parent = 0);
    ~GenericCanSetupPage();

public slots:
    void onSetupDialogCreated(SetupDialog &dlg);
    void onShowInterfacePage(SetupDialog &dlg, MeasurementInterface *mi);

private slots:
    void updateUI();

private:
    Ui::GenericCanSetupPage *ui;
    MeasurementInterface *_mi;
    bool _enable_ui_updates;

    void fillBitratesList(BusInterface *intf, unsigned selectedBitrate);
    void fillSamplePointsForBitrate(BusInterface *intf, unsigned selectedBitrate, unsigned selectedSamplePoint);
    void fillFdBitrate(BusInterface *intf, unsigned selectedBitrate);
    void fillSamplePointsForFdBitrate(BusInterface *intf, unsigned selectedBitrate, unsigned selectedSamplePoint);
    void disenableUI(bool enabled);

    Backend &backend();
};

#endif // GENERICCANSETUPPAGE_H
