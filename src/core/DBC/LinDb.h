#pragma once

#include <QString>
#include <QList>
#include <QMap>
#include <QSharedPointer>

class LinFrame;

using LinFrameMap = QMap<uint8_t, LinFrame*>;

class LinDb
{
public:
    LinDb();
    ~LinDb();

    // File path helpers
    void    setPath(const QString &path);
    QString path() const;
    QString fileName() const;
    QString directory() const;

    // LDF metadata
    QString     protocolVersion() const;
    double      speedBps() const;
    QString     channelName() const;
    QString     masterNode() const;
    QStringList slaveNodes() const;

    // Frame access
    LinFrame           *frameById(uint8_t id) const;
    LinFrame           *frameByName(const QString &name) const;
    const LinFrameMap  &frames() const;

    // Load from an LDF file. Returns false and sets lastError() on failure.
    bool    loadFile(const QString &path);
    QString lastError() const;

private:
    QString     _path;
    QString     _protocolVersion;
    double      _speedBps {19200.0};
    QString     _channelName;
    QString     _masterNode;
    QStringList _slaveNodes;
    LinFrameMap _frames;
    QString     _lastError;
};

using pLinDb = QSharedPointer<LinDb>;
