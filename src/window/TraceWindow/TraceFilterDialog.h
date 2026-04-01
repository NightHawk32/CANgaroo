#pragma once

#include <QDialog>
#include <QSet>

#include <driver/CanDriver.h>

class QCheckBox;
class QListWidget;
class Backend;
class QDomDocument;
class QDomElement;

class TraceFilterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TraceFilterDialog(Backend &backend, QWidget *parent = nullptr);

    bool showTx() const;
    bool showRx() const;
    QSet<uint32_t> hiddenMessageIds() const;
    QSet<CanInterfaceId> hiddenInterfaces() const;

    void setShowTx(bool show);
    void setShowRx(bool show);
    void setHiddenMessageIds(const QSet<uint32_t> &ids);
    void setHiddenInterfaces(const QSet<CanInterfaceId> &ids);

    bool saveXML(QDomDocument &xml, QDomElement &root) const;
    bool loadXML(QDomElement &el);

private slots:
    void clearFilters();

private:
    void populateMessages(Backend &backend);
    void populateInterfaces(Backend &backend);

    QCheckBox *m_showTx;
    QCheckBox *m_showRx;
    QListWidget *m_messageList;
    QListWidget *m_interfaceList;
};
