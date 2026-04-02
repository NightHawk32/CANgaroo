#include "TraceFilterDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDomDocument>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <core/Backend.h>
#include <core/CanDb.h>
#include <core/MeasurementNetwork.h>
#include <core/MeasurementSetup.h>

TraceFilterDialog::TraceFilterDialog(Backend &backend, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Trace Filter"));
    setMinimumSize(500, 400);

    auto *mainLayout = new QVBoxLayout(this);

    // --- Direction ---
    auto *dirGroup = new QGroupBox(tr("Direction"), this);
    auto *dirLayout = new QHBoxLayout(dirGroup);
    m_showTx = new QCheckBox(tr("Show TX"), dirGroup);
    m_showRx = new QCheckBox(tr("Show RX"), dirGroup);
    m_showTx->setChecked(true);
    m_showRx->setChecked(true);
    dirLayout->addWidget(m_showTx);
    dirLayout->addWidget(m_showRx);
    dirLayout->addStretch();
    mainLayout->addWidget(dirGroup);

    // --- Content area: messages and interfaces side by side ---
    auto *contentLayout = new QHBoxLayout;

    // --- DBC Messages ---
    auto *msgGroup = new QGroupBox(tr("DBC Messages"), this);
    auto *msgLayout = new QVBoxLayout(msgGroup);

    auto *msgBtnLayout = new QHBoxLayout;
    auto *selectAllBtn = new QPushButton(tr("Select All"), msgGroup);
    auto *deselectAllBtn = new QPushButton(tr("Deselect All"), msgGroup);
    connect(selectAllBtn, &QPushButton::clicked, this, &TraceFilterDialog::selectAllMessages);
    connect(deselectAllBtn, &QPushButton::clicked, this, &TraceFilterDialog::deselectAllMessages);
    msgBtnLayout->addWidget(selectAllBtn);
    msgBtnLayout->addWidget(deselectAllBtn);
    msgBtnLayout->addStretch();
    msgLayout->addLayout(msgBtnLayout);

    m_messageList = new QListWidget(msgGroup);
    msgLayout->addWidget(m_messageList);
    populateMessages(backend);
    contentLayout->addWidget(msgGroup);

    // --- Interfaces ---
    auto *ifGroup = new QGroupBox(tr("Interfaces"), this);
    auto *ifLayout = new QVBoxLayout(ifGroup);
    m_interfaceList = new QListWidget(ifGroup);
    ifLayout->addWidget(m_interfaceList);
    populateInterfaces(backend);
    contentLayout->addWidget(ifGroup);

    mainLayout->addLayout(contentLayout, 1);

    // --- Buttons ---
    auto *buttonLayout = new QHBoxLayout;
    auto *clearBtn = new QPushButton(tr("Clear Filters"), this);
    connect(clearBtn, &QPushButton::clicked, this, &TraceFilterDialog::clearFilters);
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonLayout->addWidget(buttons);
    mainLayout->addLayout(buttonLayout);
}

void TraceFilterDialog::populateMessages(Backend &backend)
{
    for (auto *network : backend.getSetup().getNetworks())
    {
        for (const auto &db : network->_canDbs)
        {
            const auto &messages = db->getMessageList();
            for (auto it = messages.constBegin(); it != messages.constEnd(); ++it)
            {
                auto *item = new QListWidgetItem(it.value()->getName(), m_messageList);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Checked);
                item->setData(Qt::UserRole, it.key());
            }
        }
    }

    if (m_messageList->count() == 0)
    {
        auto *item = new QListWidgetItem(tr("(no DBC loaded)"), m_messageList);
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        item->setForeground(Qt::gray);
    }
}

void TraceFilterDialog::populateInterfaces(Backend &backend)
{
    for (CanInterfaceId id : backend.getInterfaceList())
    {
        QString name = backend.getInterfaceName(id);
        auto *item = new QListWidgetItem(name, m_interfaceList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, id);
    }

    if (m_interfaceList->count() == 0)
    {
        auto *item = new QListWidgetItem(tr("(no interfaces)"), m_interfaceList);
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        item->setForeground(Qt::gray);
    }
}

bool TraceFilterDialog::showTx() const
{
    return m_showTx->isChecked();
}

bool TraceFilterDialog::showRx() const
{
    return m_showRx->isChecked();
}

QSet<uint32_t> TraceFilterDialog::hiddenMessageIds() const
{
    QSet<uint32_t> hidden;
    for (int i = 0; i < m_messageList->count(); ++i)
    {
        auto *item = m_messageList->item(i);
        if ((item->flags() & Qt::ItemIsUserCheckable) && item->checkState() == Qt::Unchecked)
        {
            hidden.insert(item->data(Qt::UserRole).toUInt());
        }
    }
    return hidden;
}

QSet<CanInterfaceId> TraceFilterDialog::hiddenInterfaces() const
{
    QSet<CanInterfaceId> hidden;
    for (int i = 0; i < m_interfaceList->count(); ++i)
    {
        auto *item = m_interfaceList->item(i);
        if ((item->flags() & Qt::ItemIsUserCheckable) && item->checkState() == Qt::Unchecked)
        {
            hidden.insert(static_cast<CanInterfaceId>(item->data(Qt::UserRole).toUInt()));
        }
    }
    return hidden;
}

void TraceFilterDialog::setShowTx(bool show)
{
    m_showTx->setChecked(show);
}

void TraceFilterDialog::setShowRx(bool show)
{
    m_showRx->setChecked(show);
}

void TraceFilterDialog::setHiddenMessageIds(const QSet<uint32_t> &ids)
{
    for (int i = 0; i < m_messageList->count(); ++i)
    {
        auto *item = m_messageList->item(i);
        if (item->flags() & Qt::ItemIsUserCheckable)
        {
            uint32_t msgId = item->data(Qt::UserRole).toUInt();
            item->setCheckState(ids.contains(msgId) ? Qt::Unchecked : Qt::Checked);
        }
    }
}

void TraceFilterDialog::setHiddenInterfaces(const QSet<CanInterfaceId> &ids)
{
    for (int i = 0; i < m_interfaceList->count(); ++i)
    {
        auto *item = m_interfaceList->item(i);
        if (item->flags() & Qt::ItemIsUserCheckable)
        {
            auto ifId = static_cast<CanInterfaceId>(item->data(Qt::UserRole).toUInt());
            item->setCheckState(ids.contains(ifId) ? Qt::Unchecked : Qt::Checked);
        }
    }
}

void TraceFilterDialog::selectAllMessages()
{
    for (int i = 0; i < m_messageList->count(); ++i)
    {
        auto *item = m_messageList->item(i);
        if (item->flags() & Qt::ItemIsUserCheckable)
        {
            item->setCheckState(Qt::Checked);
        }
    }
}

void TraceFilterDialog::deselectAllMessages()
{
    for (int i = 0; i < m_messageList->count(); ++i)
    {
        auto *item = m_messageList->item(i);
        if (item->flags() & Qt::ItemIsUserCheckable)
        {
            item->setCheckState(Qt::Unchecked);
        }
    }
}

void TraceFilterDialog::clearFilters()
{
    m_showTx->setChecked(true);
    m_showRx->setChecked(true);

    for (int i = 0; i < m_messageList->count(); ++i)
    {
        auto *item = m_messageList->item(i);
        if (item->flags() & Qt::ItemIsUserCheckable)
        {
            item->setCheckState(Qt::Checked);
        }
    }

    for (int i = 0; i < m_interfaceList->count(); ++i)
    {
        auto *item = m_interfaceList->item(i);
        if (item->flags() & Qt::ItemIsUserCheckable)
        {
            item->setCheckState(Qt::Checked);
        }
    }
}

bool TraceFilterDialog::saveXML(QDomDocument &xml, QDomElement &root) const
{
    QDomElement el = xml.createElement("TraceFilter");

    el.setAttribute("showTx", m_showTx->isChecked() ? "1" : "0");
    el.setAttribute("showRx", m_showRx->isChecked() ? "1" : "0");

    QStringList hiddenMsgs;
    for (uint32_t id : hiddenMessageIds())
    {
        hiddenMsgs.append(QString::number(id));
    }
    if (!hiddenMsgs.isEmpty())
    {
        el.setAttribute("hiddenMessages", hiddenMsgs.join(","));
    }

    QStringList hiddenIfs;
    for (CanInterfaceId id : hiddenInterfaces())
    {
        hiddenIfs.append(QString::number(id));
    }
    if (!hiddenIfs.isEmpty())
    {
        el.setAttribute("hiddenInterfaces", hiddenIfs.join(","));
    }

    root.appendChild(el);
    return true;
}

bool TraceFilterDialog::loadXML(QDomElement &el)
{
    QDomElement filterEl = el.firstChildElement("TraceFilter");
    if (filterEl.isNull())
    {
        return true;
    }

    m_showTx->setChecked(filterEl.attribute("showTx", "1") == "1");
    m_showRx->setChecked(filterEl.attribute("showRx", "1") == "1");

    QString hiddenMsgsStr = filterEl.attribute("hiddenMessages");
    if (!hiddenMsgsStr.isEmpty())
    {
        QSet<uint32_t> ids;
        for (const QString &s : hiddenMsgsStr.split(","))
        {
            ids.insert(s.toUInt());
        }
        setHiddenMessageIds(ids);
    }

    QString hiddenIfsStr = filterEl.attribute("hiddenInterfaces");
    if (!hiddenIfsStr.isEmpty())
    {
        QSet<CanInterfaceId> ids;
        for (const QString &s : hiddenIfsStr.split(","))
        {
            ids.insert(static_cast<CanInterfaceId>(s.toUInt()));
        }
        setHiddenInterfaces(ids);
    }

    return true;
}
