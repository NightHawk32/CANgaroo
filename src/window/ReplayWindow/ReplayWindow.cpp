#include "ReplayWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QFileDialog>
#include <QTextStream>
#include <QDomDocument>
#include <QFileInfo>
#include <QFont>
#include <QDoubleSpinBox>
#include <QRegularExpression>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QSplitter>

#include <QComboBox>

#include <core/CanTrace.h>
#include <core/Backend.h>
#include <core/CanDbMessage.h>
#include <driver/CanInterface.h>

ReplayWindow::ReplayWindow(QWidget *parent, Backend &backend)
    : ConfigurableWidget(parent)
    , _backend(&backend)
    , _playbackIndex(0)
    , _playing(false)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);

    // Toolbar
    auto *toolbar = new QHBoxLayout();
    _btnLoad = new QPushButton(tr("Load"));
    _btnPlay = new QPushButton(tr("Play"));
    _btnStop = new QPushButton(tr("Stop"));
    _btnPlay->setEnabled(false);
    _btnStop->setEnabled(false);

    _speedSpin = new QDoubleSpinBox(this);
    _speedSpin->setRange(0.1, 10.0);
    _speedSpin->setValue(1.0);
    _speedSpin->setSingleStep(0.1);
    _speedSpin->setSuffix("x");
    _speedSpin->setToolTip(tr("Playback speed multiplier"));

    toolbar->addWidget(_btnLoad);
    toolbar->addWidget(_btnPlay);
    toolbar->addWidget(_btnStop);
    toolbar->addStretch();
    toolbar->addWidget(new QLabel(tr("Speed:"), this));
    toolbar->addWidget(_speedSpin);

    mainLayout->addLayout(toolbar);

    // File path display
    _fileLabel = new QLineEdit(this);
    _fileLabel->setReadOnly(true);
    _fileLabel->setPlaceholderText(tr("No trace file loaded"));
    _fileLabel->setFrame(false);
    mainLayout->addWidget(_fileLabel);

    // Slider + position label
    auto *sliderLayout = new QHBoxLayout();
    _slider = new QSlider(Qt::Horizontal, this);
    _slider->setEnabled(false);
    _posLabel = new QLabel("0 / 0", this);
    _posLabel->setMinimumWidth(100);
    sliderLayout->addWidget(_slider);
    sliderLayout->addWidget(_posLabel);
    mainLayout->addLayout(sliderLayout);

    // Splitter: info box (top) + filter tree (bottom)
    auto *splitter = new QSplitter(Qt::Vertical, this);

    // Info box
    QFont mono("Monospace");
    mono.setStyleHint(QFont::TypeWriter);

    _infoBox = new QPlainTextEdit(this);
    _infoBox->setFont(mono);
    _infoBox->setReadOnly(true);
    _infoBox->setPlaceholderText(tr("Trace file info..."));
    _infoBox->setMaximumHeight(80);
    splitter->addWidget(_infoBox);

    // Filter tree with select/deselect buttons
    auto *filterWidget = new QWidget(this);
    auto *filterLayout = new QVBoxLayout(filterWidget);
    filterLayout->setContentsMargins(0, 0, 0, 0);

    auto *filterBtnLayout = new QHBoxLayout();
    _btnSelectAll = new QPushButton(tr("Select All"));
    _btnDeselectAll = new QPushButton(tr("Deselect All"));
    filterBtnLayout->addWidget(_btnSelectAll);
    filterBtnLayout->addWidget(_btnDeselectAll);
    filterBtnLayout->addStretch();
    filterLayout->addLayout(filterBtnLayout);

    _filterTree = new QTreeWidget(this);
    _filterTree->setHeaderLabels({tr("Interface / CAN ID"), tr("Name"), tr("Dir"), tr("Count"), tr("Output")});
    _filterTree->header()->setStretchLastSection(false);
    _filterTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _filterTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    _filterTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _filterTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    _filterTree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    filterLayout->addWidget(_filterTree);

    splitter->addWidget(filterWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter, 1);

    // Timer for playback
    _timer = new QTimer(this);
    _timer->setTimerType(Qt::PreciseTimer);

    // Connections
    connect(_btnLoad, &QPushButton::clicked, this, &ReplayWindow::onLoadClicked);
    connect(_btnPlay, &QPushButton::clicked, this, &ReplayWindow::onPlayClicked);
    connect(_btnStop, &QPushButton::clicked, this, &ReplayWindow::onStopClicked);
    connect(_slider, &QSlider::sliderMoved, this, &ReplayWindow::onSliderMoved);
    connect(_timer, &QTimer::timeout, this, &ReplayWindow::onTimerTick);
    connect(_btnSelectAll, &QPushButton::clicked, this, &ReplayWindow::onSelectAllClicked);
    connect(_btnDeselectAll, &QPushButton::clicked, this, &ReplayWindow::onDeselectAllClicked);
    connect(_filterTree, &QTreeWidget::itemChanged, this, &ReplayWindow::onFilterItemChanged);
}

void ReplayWindow::retranslateUi()
{
    _btnLoad->setText(tr("Load"));
    _btnPlay->setText(tr("Play"));
    _btnStop->setText(tr("Stop"));
    _btnSelectAll->setText(tr("Select All"));
    _btnDeselectAll->setText(tr("Deselect All"));
    _speedSpin->setToolTip(tr("Playback speed multiplier"));
}

void ReplayWindow::onLoadClicked()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Load Trace File"),
        _traceFilePath,
        tr("All Supported (*.asc *.candump);;Vector ASC (*.asc);;Linux candump (*.candump);;All Files (*)"));
    if (filename.isEmpty()) { return; }
    loadTraceFile(filename);
}

void ReplayWindow::loadTraceFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        _infoBox->setPlainText(tr("Error: Cannot open file."));
        return;
    }

    _messages.clear();
    _messageInterfaces.clear();
    _playbackIndex = 0;
    _playing = false;
    _timer->stop();

    bool ok = false;
    if (filename.endsWith(".candump", Qt::CaseInsensitive))
    {
        ok = parseCanDump(file);
    }
    else
    {
        ok = parseVectorAsc(file);
    }
    file.close();

    if (!ok || _messages.isEmpty())
    {
        _infoBox->setPlainText(tr("Error: Failed to parse trace file or file is empty."));
        _btnPlay->setEnabled(false);
        _slider->setEnabled(false);
        _filterTree->clear();
        _enabledIds.clear();
        return;
    }

    _traceFilePath = filename;
    _fileLabel->setText(filename);

    _slider->setRange(0, _messages.size() - 1);
    _slider->setValue(0);
    _slider->setEnabled(true);
    _btnPlay->setEnabled(true);
    _btnStop->setEnabled(false);

    double duration = _messages.last().getFloatTimestamp() - _messages.first().getFloatTimestamp();
    QString info = tr("File: %1\nMessages: %2\nDuration: %3 s")
        .arg(QFileInfo(filename).fileName())
        .arg(_messages.size())
        .arg(duration, 0, 'f', 3);
    _infoBox->setPlainText(info);

    buildFilterTree();
    updatePositionLabel();
}

bool ReplayWindow::parseCanDump(QFile &file)
{
    // Format: (1234567890.123456) interface 123#DEADBEEF
    QTextStream stream(&file);
    QRegularExpression re(R"(\((\d+\.\d+)\)\s+(\S+)\s+([0-9A-Fa-f]+)#([0-9A-Fa-f]*))");

    while (!stream.atEnd())
    {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) { continue; }

        QRegularExpressionMatch match = re.match(line);
        if (!match.hasMatch()) { continue; }

        double timestamp = match.captured(1).toDouble();
        QString interface = match.captured(2);
        uint32_t canId = match.captured(3).toUInt(nullptr, 16);
        QString dataStr = match.captured(4);

        CanMessage msg;
        msg.setTimestamp(timestamp);
        bool extended = (canId > 0x7FF);
        msg.setExtended(extended);
        msg.setId(canId);
        msg.setRX(true);

        uint8_t dlc = dataStr.length() / 2;
        msg.setLength(dlc);
        for (int i = 0; i < dlc; i++)
        {
            msg.setByte(i, dataStr.mid(i * 2, 2).toUInt(nullptr, 16));
        }

        _messages.append(msg);
        _messageInterfaces.append(interface);
    }

    return !_messages.isEmpty();
}

bool ReplayWindow::parseVectorAsc(QFile &file)
{
    // Format: timestamp channel ID dir d DLC byte0 byte1 ...
    QTextStream stream(&file);

    while (!stream.atEnd())
    {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) { continue; }

        // Data lines start with a number (timestamp)
        if (!line[0].isDigit()) { continue; }

        QStringList parts = line.split(QRegularExpression("\\s+"));
        if (parts.size() < 6) { continue; }

        bool ok = false;
        double timestamp = parts[0].toDouble(&ok);
        if (!ok) { continue; }

        QString channel = parts[1];
        QString idStr = parts[2];
        bool extended = idStr.endsWith('x') || idStr.endsWith('X');
        if (extended) { idStr.chop(1); }

        uint32_t canId = idStr.toUInt(&ok, 16);
        if (!ok) { continue; }

        QString dir = parts[3];
        // parts[4] = "d" or "r" (data/remote frame)

        int dlcIdx = 5;
        if (parts.size() <= dlcIdx) { continue; }
        int dlc = parts[dlcIdx].toInt(&ok);
        if (!ok) { continue; }

        CanMessage msg;
        msg.setTimestamp(timestamp);
        msg.setExtended(extended);
        msg.setId(canId);
        msg.setRX(dir.toLower() == "rx");
        msg.setLength(dlc);

        for (int i = 0; i < dlc && (dlcIdx + 1 + i) < parts.size(); i++)
        {
            msg.setByte(i, parts[dlcIdx + 1 + i].toUInt(nullptr, 16));
        }

        _messages.append(msg);
        _messageInterfaces.append(tr("Ch %1").arg(channel));
    }

    return !_messages.isEmpty();
}

void ReplayWindow::buildFilterTree()
{
    _filterTree->blockSignals(true);
    _filterTree->clear();
    _enabledIds.clear();
    _channelCombos.clear();

    struct IdInfo {
        int count = 0;
        bool hasRx = false;
        bool hasTx = false;
    };

    // Collect interface -> (id -> info)
    QMap<QString, QMap<uint32_t, IdInfo>> ifaceIds;
    for (int i = 0; i < _messages.size(); i++)
    {
        const QString &iface = _messageInterfaces[i];
        uint32_t id = _messages[i].getId();
        IdInfo &info = ifaceIds[iface][id];
        info.count++;
        if (_messages[i].isRX())
            info.hasRx = true;
        else
            info.hasTx = true;
    }

    // Collect available CAN interfaces for the combo boxes
    CanInterfaceIdList availableInterfaces = _backend->getInterfaceList();

    // Build tree items
    for (auto ifaceIt = ifaceIds.constBegin(); ifaceIt != ifaceIds.constEnd(); ++ifaceIt)
    {
        const QString &iface = ifaceIt.key();
        const QMap<uint32_t, IdInfo> &ids = ifaceIt.value();

        auto *ifaceItem = new QTreeWidgetItem(_filterTree);
        ifaceItem->setText(0, iface);
        ifaceItem->setFlags(ifaceItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
        ifaceItem->setCheckState(0, Qt::Checked);

        // Output interface combo box
        auto *combo = new QComboBox(_filterTree);
        combo->addItem(tr("Trace only"), QVariant(static_cast<int>(-1)));
        for (CanInterfaceId id : availableInterfaces)
        {
            QString name = _backend->getInterfaceName(id);
            combo->addItem(name, QVariant(static_cast<int>(id)));
        }
        _channelCombos[iface] = combo;

        int ifaceTotal = 0;
        QSet<uint32_t> enabledSet;

        // Sort IDs numerically
        QList<uint32_t> sortedIds = ids.keys();
        std::sort(sortedIds.begin(), sortedIds.end());

        for (uint32_t id : sortedIds)
        {
            const IdInfo &info = ids[id];
            ifaceTotal += info.count;

            auto *idItem = new QTreeWidgetItem(ifaceItem);
            idItem->setText(0, QString("0x%1").arg(id, 0, 16, QChar('0')).toUpper());

            // Look up DBC message name
            CanMessage tmp;
            tmp.setId(id);
            tmp.setExtended(id > 0x7FF);
            CanDbMessage *dbMsg = _backend->findDbMessage(tmp);
            if (dbMsg)
            {
                idItem->setText(1, dbMsg->getName());
            }

            // Direction
            QString dir;
            if (info.hasRx && info.hasTx)
                dir = "RX/TX";
            else if (info.hasRx)
                dir = "RX";
            else
                dir = "TX";
            idItem->setText(2, dir);
            idItem->setTextAlignment(2, Qt::AlignCenter);

            idItem->setText(3, QString::number(info.count));
            idItem->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
            idItem->setFlags(idItem->flags() | Qt::ItemIsUserCheckable);
            idItem->setCheckState(0, Qt::Checked);
            idItem->setData(0, Qt::UserRole, id);
            idItem->setData(0, Qt::UserRole + 1, iface);

            enabledSet.insert(id);
        }

        ifaceItem->setText(3, QString::number(ifaceTotal));
        ifaceItem->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        _enabledIds[iface] = enabledSet;

        // Set combo box on interface item after it's added to the tree
        _filterTree->setItemWidget(ifaceItem, 4, combo);
    }

    _filterTree->expandAll();
    _filterTree->blockSignals(false);
}

void ReplayWindow::onFilterItemChanged(QTreeWidgetItem *item, int column)
{
    (void) column;
    _filterTree->blockSignals(true);

    if (item->childCount() > 0)
    {
        // Interface-level item: propagate to children
        Qt::CheckState state = item->checkState(0);
        QString iface = item->text(0);
        for (int i = 0; i < item->childCount(); i++)
        {
            item->child(i)->setCheckState(0, state);
        }
        // Update enabled set
        if (state == Qt::Checked)
        {
            for (int i = 0; i < item->childCount(); i++)
            {
                _enabledIds[iface].insert(item->child(i)->data(0, Qt::UserRole).toUInt());
            }
        }
        else
        {
            _enabledIds[iface].clear();
        }
    }
    else
    {
        // ID-level item
        QString iface = item->data(0, Qt::UserRole + 1).toString();
        uint32_t id = item->data(0, Qt::UserRole).toUInt();

        if (item->checkState(0) == Qt::Checked)
        {
            _enabledIds[iface].insert(id);
        }
        else
        {
            _enabledIds[iface].remove(id);
        }

        // Update parent tri-state
        QTreeWidgetItem *parent = item->parent();
        if (parent)
        {
            int checked = 0, unchecked = 0;
            for (int i = 0; i < parent->childCount(); i++)
            {
                if (parent->child(i)->checkState(0) == Qt::Checked)
                    checked++;
                else
                    unchecked++;
            }
            if (checked == parent->childCount())
                parent->setCheckState(0, Qt::Checked);
            else if (unchecked == parent->childCount())
                parent->setCheckState(0, Qt::Unchecked);
            else
                parent->setCheckState(0, Qt::PartiallyChecked);
        }
    }

    _filterTree->blockSignals(false);
}

void ReplayWindow::onSelectAllClicked()
{
    _filterTree->blockSignals(true);
    for (int i = 0; i < _filterTree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *ifaceItem = _filterTree->topLevelItem(i);
        ifaceItem->setCheckState(0, Qt::Checked);
        QString iface = ifaceItem->text(0);
        for (int j = 0; j < ifaceItem->childCount(); j++)
        {
            ifaceItem->child(j)->setCheckState(0, Qt::Checked);
            _enabledIds[iface].insert(ifaceItem->child(j)->data(0, Qt::UserRole).toUInt());
        }
    }
    _filterTree->blockSignals(false);
}

void ReplayWindow::onDeselectAllClicked()
{
    _filterTree->blockSignals(true);
    for (int i = 0; i < _filterTree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *ifaceItem = _filterTree->topLevelItem(i);
        ifaceItem->setCheckState(0, Qt::Unchecked);
        QString iface = ifaceItem->text(0);
        _enabledIds[iface].clear();
        for (int j = 0; j < ifaceItem->childCount(); j++)
        {
            ifaceItem->child(j)->setCheckState(0, Qt::Unchecked);
        }
    }
    _filterTree->blockSignals(false);
}

bool ReplayWindow::isMessageEnabled(int index) const
{
    const QString &iface = _messageInterfaces[index];
    uint32_t id = _messages[index].getId();
    auto it = _enabledIds.constFind(iface);
    return it != _enabledIds.constEnd() && it->contains(id);
}

CanInterfaceId ReplayWindow::getMappedInterface(const QString &channel) const
{
    auto it = _channelCombos.constFind(channel);
    if (it == _channelCombos.constEnd()) { return -1; }
    return static_cast<CanInterfaceId>(it.value()->currentData().toInt());
}

void ReplayWindow::onPlayClicked()
{
    if (_messages.isEmpty() || _playbackIndex >= _messages.size()) { return; }

    _playing = true;
    _btnPlay->setEnabled(false);
    _btnStop->setEnabled(true);
    _btnLoad->setEnabled(false);
    _slider->setEnabled(false);

    _traceStartTime = _messages[_playbackIndex].getFloatTimestamp();
    _elapsed.start();

    _timer->start(1);
}

void ReplayWindow::onStopClicked()
{
    _playing = false;
    _timer->stop();
    _btnPlay->setEnabled(true);
    _btnStop->setEnabled(false);
    _btnLoad->setEnabled(true);
    _slider->setEnabled(true);
}

void ReplayWindow::onSliderMoved(int value)
{
    _playbackIndex = value;
    updatePositionLabel();
}

void ReplayWindow::onTimerTick()
{
    if (!_playing || _playbackIndex >= _messages.size())
    {
        onStopClicked();
        return;
    }

    CanTrace *trace = _backend->getTrace();
    double speed = _speedSpin->value();

    // How far into the trace we should be, based on wall-clock time and speed
    double wallElapsed = _elapsed.nsecsElapsed() / 1.0e9;
    double traceDeadline = _traceStartTime + wallElapsed * speed;

    // Send all messages whose original timestamp <= deadline
    while (_playbackIndex < _messages.size())
    {
        double msgTime = _messages[_playbackIndex].getFloatTimestamp();
        if (msgTime > traceDeadline) { break; }

        if (isMessageEnabled(_playbackIndex))
        {
            CanMessage msg = _messages[_playbackIndex];
            msg.setTimestamp(_backend->currentTimeStamp());

            const QString &channel = _messageInterfaces[_playbackIndex];
            int mappedInt = _channelCombos.contains(channel) ? _channelCombos[channel]->currentData().toInt() : -1;
            CanInterface *intf = (mappedInt >= 0) ? _backend->getInterfaceById(static_cast<CanInterfaceId>(mappedInt)) : nullptr;

            if (intf && intf->isOpen())
            {
                msg.setInterfaceId(static_cast<CanInterfaceId>(mappedInt));
                msg.setRX(false);
                intf->sendMessage(msg);
            }

            bool moreToFollow = false;
            for (int next = _playbackIndex + 1; next < _messages.size(); next++)
            {
                if (_messages[next].getFloatTimestamp() > traceDeadline) { break; }
                if (isMessageEnabled(next)) { moreToFollow = true; break; }
            }

            trace->enqueueMessage(msg, moreToFollow);
        }
        _playbackIndex++;
    }

    _slider->setValue(_playbackIndex);
    updatePositionLabel();

    if (_playbackIndex >= _messages.size())
    {
        onStopClicked();
    }
}

void ReplayWindow::updatePositionLabel()
{
    _posLabel->setText(QString("%1 / %2").arg(_playbackIndex).arg(_messages.size()));
}

bool ReplayWindow::saveXML(Backend &backend, QDomDocument &xml, QDomElement &root)
{
    (void) backend;
    (void) xml;
    root.setAttribute("file", _traceFilePath);
    return true;
}

bool ReplayWindow::loadXML(Backend &backend, QDomElement &el)
{
    (void) backend;
    QString filepath = el.attribute("file");
    if (!filepath.isEmpty())
    {
        loadTraceFile(filepath);
    }
    return true;
}
