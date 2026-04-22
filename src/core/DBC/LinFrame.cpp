#include "LinFrame.h"
#include "LinSignal.h"

LinFrame::LinFrame(LinDb *parent)
    : _parent(parent)
{
}

LinFrame::~LinFrame()
{
    qDeleteAll(_signals);
}

uint8_t LinFrame::id() const { return _id; }
void    LinFrame::setId(uint8_t id) { _id = id; }

QString LinFrame::name() const { return _name; }
void    LinFrame::setName(const QString &name) { _name = name; }

QString LinFrame::publisher() const { return _publisher; }
void    LinFrame::setPublisher(const QString &publisher) { _publisher = publisher; }

uint8_t LinFrame::length() const { return _length; }
void    LinFrame::setLength(uint8_t length) { _length = length; }

void LinFrame::addSignal(LinSignal *signal)
{
    _signals.append(signal);
}

LinSignal *LinFrame::findSignal(const QString &name) const
{
    for (LinSignal *s : _signals)
    {
        if (s->name() == name)
            return s;
    }
    return nullptr;
}

const LinSignalList &LinFrame::signalList() const
{
    return _signals;
}
