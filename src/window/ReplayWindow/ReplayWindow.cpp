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

#include <QCheckBox>
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

    _cbAutoplay = new QCheckBox(tr("Autoplay"), this);
    _cbAutoplay->setToolTip(tr("Automatically start/stop replay with measurement"));

    toolbar->addWidget(_btnLoad);
    toolbar->addWidget(_btnPlay);
    toolbar->addWidget(_btnStop);
    toolbar->addWidget(_cbAutoplay);
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

    // Info box
    QFont mono("Monospace");
    mono.setStyleHint(QFont::TypeWriter);

    _infoBox = new QPlainTextEdit(this);
    _infoBox->setFont(mono);
    _infoBox->setReadOnly(true);
    _infoBox->setPlaceholderText(tr("Trace file info..."));
    _infoBox->setMaximumHeight(80);
    mainLayout->addWidget(_infoBox);

    // Filter tree with select/deselect buttons
    auto *filterBtnLayout = new QHBoxLayout();
    _btnSelectAll = new QPushButton(tr("Select All"));
    _btnDeselectAll = new QPushButton(tr("Deselect All"));
    filterBtnLayout->addWidget(_btnSelectAll);
    filterBtnLayout->addWidget(_btnDeselectAll);
    filterBtnLayout->addStretch();
    mainLayout->addLayout(filterBtnLayout);

    _filterTree = new QTreeWidget(this);
    _filterTree->setHeaderLabels({tr("Interface / CAN ID"), tr("Name"), tr("RX"), tr("TX"), tr("Count"), tr("Output")});
    _filterTree->header()->setStretchLastSection(false);
    _filterTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _filterTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    _filterTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _filterTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    _filterTree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    _filterTree->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    mainLayout->addWidget(_filterTree, 1);

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
    connect(_backend, &Backend::beginMeasurement, this, &ReplayWindow::onBeginMeasurement);
    connect(_backend, &Backend::endMeasurement, this, &ReplayWindow::onEndMeasurement);
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
        _enabledDirs.clear();
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
    // Formats:
    //   Classic:  (timestamp) interface ID#DATA
    //   CANFD:    (timestamp) interface ID##FlagsDATA
    //   RTR:      (timestamp) interface ID#RD  (R followed by optional DLC digit)
    //   Error:    (timestamp) interface ID#DATA  (ID has error flag 0x20000000)
    QTextStream stream(&file);
    QRegularExpression re(R"(\((\d+\.\d+)\)\s+(\S+)\s+([0-9A-Fa-f]+)(##?)(.*)$)");

    while (!stream.atEnd())
    {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) { continue; }

        QRegularExpressionMatch match = re.match(line);
        if (!match.hasMatch()) { continue; }

        double timestamp = match.captured(1).toDouble();
        QString interface = match.captured(2);
        uint32_t rawId = match.captured(3).toUInt(nullptr, 16);
        QString separator = match.captured(4);
        QString payload = match.captured(5);

        CanMessage msg;
        msg.setTimestamp(timestamp);

        // Error frame: error flag bit set in ID
        if (rawId & 0x20000000) {
            msg.setErrorFrame(true);
            msg.setId(rawId & ~0x20000000);
            msg.setLength(0);
            _messages.append(msg);
            _messageInterfaces.append(interface);
            continue;
        }

        bool extended = (rawId > 0x7FF);
        msg.setExtended(extended);
        msg.setId(rawId);
        msg.setRX(true);

        if (separator == "##") {
            // CANFD: first char is flags byte, rest is data
            msg.setFD(true);
            if (!payload.isEmpty()) {
                uint8_t flags = QString(payload[0]).toUInt(nullptr, 16);
                msg.setBRS((flags & 0x1) != 0);
                QString dataStr = payload.mid(1);
                int dlc = dataStr.length() / 2;
                msg.setLength(dlc);
                for (int i = 0; i < dlc; i++) {
                    msg.setByte(i, dataStr.mid(i * 2, 2).toUInt(nullptr, 16));
                }
            }
        } else if (payload.startsWith('R') || payload.startsWith('r')) {
            // RTR: #R or #R8 (R followed by optional DLC)
            msg.setRTR(true);
            QString dlcStr = payload.mid(1);
            int dlc = dlcStr.isEmpty() ? 0 : dlcStr.toInt();
            msg.setLength(dlc);
        } else {
            // Classic CAN
            int dlc = payload.length() / 2;
            msg.setLength(dlc);
            for (int i = 0; i < dlc; i++) {
                msg.setByte(i, payload.mid(i * 2, 2).toUInt(nullptr, 16));
            }
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
        if (parts.size() < 3) { continue; }

        bool ok = false;
        double timestamp = parts[0].toDouble(&ok);
        if (!ok) { continue; }

        QString channel = parts[1];

        // Error frame: "timestamp channel ErrorFrame"
        if (parts[2].compare("ErrorFrame", Qt::CaseInsensitive) == 0)
        {
            CanMessage msg;
            msg.setTimestamp(timestamp);
            msg.setErrorFrame(true);
            msg.setLength(0);
            msg.setId(0);
            msg.setInterfaceId(channel.toInt());
            _messages.append(msg);
            _messageInterfaces.append(tr("Ch %1").arg(channel));
            continue;
        }

        if (parts.size() < 6) { continue; }

                // CANFD format: timestamp CANFD channel Rx/Tx ID flags 0 0 DLC DataLength data...
        if (parts[1].compare("CANFD", Qt::CaseInsensitive) == 0)
        {
            if (parts.size() < 10) { continue; }

            QString channel = parts[2];
            QString dir = parts[3];
            QString idStr = parts[4];
            bool extended = idStr.endsWith('x') || idStr.endsWith('X');
            if (extended) { idStr.chop(1); }

            uint32_t canId = idStr.toUInt(&ok, 16);
            if (!ok) { continue; }

            int flags = parts[5].toInt(&ok);
            if (!ok) { flags = 0; }

            // parts[6] and parts[7] are reserved (0 0)
            int dlc = parts[8].toInt(&ok);
            if (!ok) { continue; }
            int dataLength = parts[9].toInt(&ok);
            if (!ok) { dataLength = dlc; }

            CanMessage msg;
            msg.setTimestamp(timestamp);
            msg.setFD(true);
            msg.setBRS((flags & 0x1) != 0);
            msg.setExtended(extended);
            msg.setId(canId);
            msg.setRX(dir.toLower() == "rx");
            msg.setLength(dataLength);
            msg.setInterfaceId(channel.toInt());

            for (int i = 0; i < dataLength && (10 + i) < parts.size(); i++)
            {
                msg.setByte(i, parts[10 + i].toUInt(nullptr, 16));
            }

            _messages.append(msg);
            _messageInterfaces.append(tr("Ch %1").arg(channel));
            continue;
        }

        QString idStr = parts[2];
        bool extended = idStr.endsWith('x') || idStr.endsWith('X');
        if (extended) { idStr.chop(1); }

        uint32_t canId = idStr.toUInt(&ok, 16);
        if (!ok) { continue; }

        QString dir = parts[3];
        // parts[4] = "d" or "r" (data/remote frame)
        bool isRTR = (parts[4].toLower() == "r");

        int dlcIdx = 5;
        int dlc = parts[dlcIdx].toInt(&ok);
        if (!ok) { continue; }

        CanMessage msg;
        msg.setTimestamp(timestamp);
        msg.setExtended(extended);
        msg.setId(canId);
        msg.setRX(dir.toLower() == "rx");
        msg.setRTR(isRTR);
        msg.setLength(dlc);
        msg.setInterfaceId(channel.toInt());

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
    _enabledDirs.clear();
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
        uint32_t id = _messages[i].isErrorFrame() ? ErrorFrameId : _messages[i].getId();
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
        QMap<uint32_t, uint8_t> enabledMap;

        // Sort IDs numerically
        QList<uint32_t> sortedIds = ids.keys();
        std::sort(sortedIds.begin(), sortedIds.end());

        for (uint32_t id : sortedIds)
        {
            const IdInfo &info = ids[id];
            ifaceTotal += info.count;

            auto *idItem = new QTreeWidgetItem(ifaceItem);

            if (id == ErrorFrameId)
            {
                idItem->setText(0, tr("ERROR"));
            }
            else
            {
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
            }

            // RX / TX checkboxes
            idItem->setFlags(idItem->flags() | Qt::ItemIsUserCheckable);
            idItem->setCheckState(0, Qt::Checked);
            uint8_t dirs = 0;
            if (info.hasRx)
            {
                idItem->setCheckState(2, Qt::Checked);
                dirs |= DirRx;
            }
            if (info.hasTx)
            {
                idItem->setCheckState(3, Qt::Checked);
                dirs |= DirTx;
            }

            idItem->setText(4, QString::number(info.count));
            idItem->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
            idItem->setData(0, Qt::UserRole, id);
            idItem->setData(0, Qt::UserRole + 1, iface);

            enabledMap[id] = dirs;
        }

        ifaceItem->setText(4, QString::number(ifaceTotal));
        ifaceItem->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
        _enabledDirs[iface] = enabledMap;

        // Set combo box on interface item after it's added to the tree
        _filterTree->setItemWidget(ifaceItem, 5, combo);
    }

    _filterTree->expandAll();
    _filterTree->blockSignals(false);
}

void ReplayWindow::onFilterItemChanged(QTreeWidgetItem *item, int column)
{
    _filterTree->blockSignals(true);

    if (item->childCount() > 0)
    {
        // Interface-level item: propagate column 0 to children
        Qt::CheckState state = item->checkState(0);
        QString iface = item->text(0);
        for (int i = 0; i < item->childCount(); i++)
        {
            item->child(i)->setCheckState(0, state);
        }
        if (state == Qt::Checked)
        {
            for (int i = 0; i < item->childCount(); i++)
            {
                auto *child = item->child(i);
                uint32_t id = child->data(0, Qt::UserRole).toUInt();
                uint8_t dirs = 0;
                if (child->data(2, Qt::CheckStateRole).isValid() && child->checkState(2) == Qt::Checked) dirs |= DirRx;
                if (child->data(3, Qt::CheckStateRole).isValid() && child->checkState(3) == Qt::Checked) dirs |= DirTx;
                _enabledDirs[iface][id] = dirs;
            }
        }
        else
        {
            _enabledDirs[iface].clear();
        }
    }
    else
    {
        // ID-level item
        QString iface = item->data(0, Qt::UserRole + 1).toString();
        uint32_t id = item->data(0, Qt::UserRole).toUInt();

        if (item->checkState(0) == Qt::Checked)
        {
            uint8_t dirs = 0;
            if (item->data(2, Qt::CheckStateRole).isValid() && item->checkState(2) == Qt::Checked) dirs |= DirRx;
            if (item->data(3, Qt::CheckStateRole).isValid() && item->checkState(3) == Qt::Checked) dirs |= DirTx;
            _enabledDirs[iface][id] = dirs;
        }
        else if (column == 0)
        {
            _enabledDirs[iface].remove(id);
        }
        else
        {
            // RX/TX checkbox changed — update direction flags
            uint8_t dirs = 0;
            if (item->data(2, Qt::CheckStateRole).isValid() && item->checkState(2) == Qt::Checked) dirs |= DirRx;
            if (item->data(3, Qt::CheckStateRole).isValid() && item->checkState(3) == Qt::Checked) dirs |= DirTx;
            _enabledDirs[iface][id] = dirs;
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
            auto *child = ifaceItem->child(j);
            child->setCheckState(0, Qt::Checked);
            uint32_t id = child->data(0, Qt::UserRole).toUInt();
            uint8_t dirs = 0;
            if (child->data(2, Qt::CheckStateRole).isValid()) { child->setCheckState(2, Qt::Checked); dirs |= DirRx; }
            if (child->data(3, Qt::CheckStateRole).isValid()) { child->setCheckState(3, Qt::Checked); dirs |= DirTx; }
            _enabledDirs[iface][id] = dirs;
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
        _enabledDirs[iface].clear();
        for (int j = 0; j < ifaceItem->childCount(); j++)
        {
            auto *child = ifaceItem->child(j);
            child->setCheckState(0, Qt::Unchecked);
            if (child->data(2, Qt::CheckStateRole).isValid()) child->setCheckState(2, Qt::Unchecked);
            if (child->data(3, Qt::CheckStateRole).isValid()) child->setCheckState(3, Qt::Unchecked);
        }
    }
    _filterTree->blockSignals(false);
}

bool ReplayWindow::isMessageEnabled(int index) const
{
    const QString &iface = _messageInterfaces[index];
    uint32_t id = _messages[index].isErrorFrame() ? ErrorFrameId : _messages[index].getId();
    auto ifaceIt = _enabledDirs.constFind(iface);
    if (ifaceIt == _enabledDirs.constEnd()) return false;
    auto idIt = ifaceIt->constFind(id);
    if (idIt == ifaceIt->constEnd()) return false;
    uint8_t flag = _messages[index].isRX() ? DirRx : DirTx;
    return (*idIt & flag) != 0;
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

void ReplayWindow::onBeginMeasurement()
{
    if (_cbAutoplay->isChecked() && !_messages.isEmpty() && !_playing)
    {
        _playbackIndex = 0;
        _slider->setValue(0);
        onPlayClicked();
    }
}

void ReplayWindow::onEndMeasurement()
{
    if (_cbAutoplay->isChecked() && _playing)
    {
        onStopClicked();
        _playbackIndex = 0;
        _slider->setValue(0);
        updatePositionLabel();
    }
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

            bool sentOnInterface = false;
            if (intf && intf->isOpen() && !msg.isErrorFrame())
            {
                msg.setInterfaceId(static_cast<CanInterfaceId>(mappedInt));
                msg.setRX(false);
                intf->sendMessage(msg);
                sentOnInterface = true;
            }

            if (!sentOnInterface)
            {
                if (mappedInt >= 0) {
                    msg.setInterfaceId(static_cast<CanInterfaceId>(mappedInt));
                }

                bool moreToFollow = false;
                for (int next = _playbackIndex + 1; next < _messages.size(); next++)
                {
                    if (_messages[next].getFloatTimestamp() > traceDeadline) { break; }
                    if (isMessageEnabled(next)) { moreToFollow = true; break; }
                }

                trace->enqueueMessage(msg, moreToFollow);
            }
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
    root.setAttribute("autoplay", _cbAutoplay->isChecked() ? "1" : "0");
    return true;
}

bool ReplayWindow::loadXML(Backend &backend, QDomElement &el)
{
    (void) backend;
    _cbAutoplay->setChecked(el.attribute("autoplay", "0") == "1");
    QString filepath = el.attribute("file");
    if (!filepath.isEmpty())
    {
        loadTraceFile(filepath);
    }
    return true;
}
