#pragma once

#include <stdint.h>
#include <QString>
#include <QList>

class LinDb;
class LinSignal;

using LinSignalList = QList<LinSignal*>;

class LinFrame
{
public:
    explicit LinFrame(LinDb *parent);
    ~LinFrame();

    uint8_t id() const;
    void setId(uint8_t id);

    QString name() const;
    void setName(const QString &name);

    QString publisher() const;
    void setPublisher(const QString &publisher);

    uint8_t length() const;
    void setLength(uint8_t length);

    void addSignal(LinSignal *signal);
    LinSignal *findSignal(const QString &name) const;
    const LinSignalList &signalList() const;

private:
    LinDb        *_parent;
    uint8_t       _id     {0};
    QString       _name;
    QString       _publisher;
    uint8_t       _length {0};
    LinSignalList _signals;
};
