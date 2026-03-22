#pragma once

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <atomic>
#include <chrono>
#include <thread>

#include <core/CanMessage.h>

class Backend;

class PythonEngine : public QObject
{
    Q_OBJECT

public:
    explicit PythonEngine(Backend &backend, QObject *parent = nullptr);
    ~PythonEngine() override;

    void runScript(const QString &code);
    void stopScript();
    bool isRunning() const;

    Backend &backend() { return _backend; }

    void enqueueMessage(const CanMessage &msg);

    QMutex &msgQueueMutex() { return _msgQueueMutex; }
    QWaitCondition &msgQueueCondition() { return _msgQueueCondition; }
    QQueue<CanMessage> &msgQueue() { return _msgQueue; }
    bool stopRequested() const { return _stopRequested.load(); }

signals:
    void scriptOutput(const QString &text);
    void scriptError(const QString &text);
    void scriptStarted();
    void scriptFinished();

private:
    Backend &_backend;
    std::unique_ptr<std::thread> _workerThread;
    std::atomic<bool> _running{false};
    std::atomic<bool> _stopRequested{false};

    QMutex _msgQueueMutex;
    QWaitCondition _msgQueueCondition;
    QQueue<CanMessage> _msgQueue;

    void workerFunc(std::string code);
};
