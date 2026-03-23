#pragma once

#include <core/ConfigurableWidget.h>
#include <core/Backend.h>
#include <QDateTime>

class PythonEngine;
class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QSplitter;

class ScriptWindow : public ConfigurableWidget
{
    Q_OBJECT

public:
    explicit ScriptWindow(QWidget *parent, Backend &backend);
    ~ScriptWindow() override;

    bool saveXML(Backend &backend, QDomDocument &xml, QDomElement &root) override;
    bool loadXML(Backend &backend, QDomElement &el) override;

protected:
    void retranslateUi() override;

private slots:
    void onRunClicked();
    void onStopClicked();
    void onClearClicked();
    void onLoadClicked();
    void onSaveClicked();
    void onScriptOutput(const QString &text);
    void onScriptError(const QString &text);
    void onScriptStarted();
    void onScriptFinished();
    void onMeasurementStarted();
    void onMeasurementStopped();

private:
    Backend *_backend;
    PythonEngine *_engine;

    QSplitter *_splitter;
    QPlainTextEdit *_editor;
    QPlainTextEdit *_console;
    QPushButton *_btnRun;
    QPushButton *_btnStop;
    QPushButton *_btnClear;
    QPushButton *_btnLoad;
    QPushButton *_btnSave;
    QCheckBox *_chkAutoRun;
    QString _scriptFilePath;
    QDateTime _lastLoadTime;

    void loadScriptFile(const QString &filename);
    void reloadIfModified();
};
