#pragma once

#include <core/ConfigurableWidget.h>
#include <core/Backend.h>
#include <QElapsedTimer>
#include <QSet>
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
class QComboBox;

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
    QPlainTextEdit *_infoBox;
    QTreeWidget *_filterTree;
    QTimer *_timer;

    QString _traceFilePath;
    QVector<CanMessage> _messages;
    QVector<QString> _messageInterfaces;
    int _playbackIndex;
    bool _playing;
    QElapsedTimer _elapsed;
    double _traceStartTime;

    // Filter: interface -> set of enabled CAN IDs
    QMap<QString, QSet<uint32_t>> _enabledIds;

    // Channel mapping: trace channel name -> combo box
    QMap<QString, QComboBox*> _channelCombos;

    void loadTraceFile(const QString &filename);
    bool parseCanDump(QFile &file);
    bool parseVectorAsc(QFile &file);
    void buildFilterTree();
    void updatePositionLabel();
    bool isMessageEnabled(int index) const;
    CanInterfaceId getMappedInterface(const QString &channel) const;
};
