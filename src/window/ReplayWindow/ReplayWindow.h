/*
  Copyright (c) 2026 Schildkroet

  This file is part of CANgaroo.

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
#pragma once

#include "core/ConfigurableWidget.h"
#include "core/Backend.h"
#include <QElapsedTimer>
#include <QMap>

class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QSlider;
class QLabel;
class QTimer;
class QDoubleSpinBox;
class QTreeWidget;
class QTreeWidgetItem;
class QCheckBox;
class QComboBox;
class QProgressBar;


class ReplayWindow : public ConfigurableWidget
{
    Q_OBJECT

public:
    explicit ReplayWindow(QWidget *parent, Backend &backend);

    bool saveXML(Backend &backend, QDomDocument &xml, QDomElement &root) override;
    bool loadXML(Backend &backend, QDomElement &el) override;

protected:
    void retranslateUi() override;

private slots:
    void onLoadClicked();
    void onPlayClicked();
    void onStopClicked();
    void onSliderMoved(int value);
    void onTimerTick();
    void onSelectAllClicked();
    void onDeselectAllClicked();
    void onFilterItemChanged(QTreeWidgetItem *item, int column);
    void onBeginMeasurement();
    void onEndMeasurement();

private:
    Backend *_backend;

    QLineEdit *_fileLabel;
    QPushButton *_btnLoad;
    QPushButton *_btnPlay;
    QPushButton *_btnStop;
    QPushButton *_btnSelectAll;
    QPushButton *_btnDeselectAll;
    QDoubleSpinBox *_speedSpin;
    QSlider *_slider;
    QLabel *_posLabel;
    QProgressBar *_progressBar;
    QPlainTextEdit *_infoBox;
    QTreeWidget *_filterTree;
    QCheckBox *_cbAutoplay;
    QCheckBox *_cbLoop;
    QTimer *_timer;

    QString _traceFilePath;
    QVector<BusMessage> _messages;
    QVector<QString> _messageInterfaces;
    int _playbackIndex;
    bool _playing;
    QElapsedTimer _elapsed;
    double _traceStartTime;

    // Filter: interface -> (CAN ID -> direction flags)
    // Bit 0 = RX enabled, Bit 1 = TX enabled
    static constexpr uint8_t DirRx = 0x01;
    static constexpr uint8_t DirTx = 0x02;
    static constexpr uint32_t ErrorFrameId = 0xFFFFFFFF;
    QMap<QString, QMap<uint32_t, uint8_t>> _enabledDirs;

    // Channel mapping: trace channel name -> combo box
    QMap<QString, QComboBox*> _channelCombos;

    void loadTraceFile(const QString &filename);
    bool parseCanDump(QFile &file);
    bool parseVectorAsc(QFile &file);
    bool parsePcap(QFile &file);
    bool parsePcapNg(QFile &file);
    void buildFilterTree();
    void updatePositionLabel();
    bool isMessageEnabled(int index) const;
    BusInterfaceId getMappedInterface(const QString &channel) const;
};
