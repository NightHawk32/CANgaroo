#pragma once

#include <core/Backend.h>
#include <core/ConfigurableWidget.h>
#include <core/MeasurementSetup.h>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QTableWidget;
class QDomDocument;
class QDomElement;
class CanDbMessage;

class RawTxWindow : public ConfigurableWidget
{
    Q_OBJECT

public:
    explicit RawTxWindow(QWidget *parent, Backend &backend);

    bool saveXML(Backend &backend, QDomDocument &xml, QDomElement &root) override;
    bool loadXML(Backend &backend, QDomElement &el) override;

protected:
    void retranslateUi() override;

public slots:
    void setMessage(const CanMessage &msg, const QString &name, CanInterfaceId interfaceId, CanDbMessage *dbMsg = nullptr);

signals:
    void messageUpdated(const CanMessage &msg);
    void interfaceSelected(CanInterfaceId interfaceId);

private slots:
    void onFieldChanged();

private:
    Backend &_backend;
    CanMessage _can_msg;
    CanDbMessage *_currentDbMsg;
    CanInterfaceId _slavedInterfaceId;
    bool _settingMessage;

    QLineEdit *_editId;
    QComboBox *_comboDlc;
    QComboBox *_comboInterface;
    QCheckBox *_cbExtended;
    QCheckBox *_cbRTR;
    QCheckBox *_cbFD;
    QCheckBox *_cbBRS;
    QTableWidget *_dataTable;
    QTableWidget *_signalTable;

    static constexpr int DataCols = 8;
    static constexpr int MaxDataRows = 8; // 8 rows × 8 cols = 64 bytes

    void updateDataGrid();
    void updateSignalTable();
    void populateDlcCombo(bool canfd);
    int currentDlc() const;
};
