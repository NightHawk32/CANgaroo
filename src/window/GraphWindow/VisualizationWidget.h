/*

  Copyright (c) 2026 Jayachandran Dharuman

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

#include <QWidget>
#include "core/BusMessage.h"
#include "core/Backend.h"
#include "core/ThemeManager.h"
#include "GraphSignal.h"
#include <QMetaType>

class ThemeManager;

struct DecodedSignalData {
    int interfaceId;
    QVector<double> timestamps;
    QVector<double> values;
};

class VisualizationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VisualizationWidget(QWidget *parent, Backend &backend) : QWidget(parent), _backend(backend) {
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &VisualizationWidget::applyTheme);
    }
    virtual ~VisualizationWidget() {}

    virtual void applyTheme(ThemeManager::Theme theme) { Q_UNUSED(theme); }

    virtual void addMessage(const BusMessage &msg) = 0;
    virtual void addDecodedData(const QMap<GraphSignal*, DecodedSignalData>& newPoints) = 0;
    virtual void clear() = 0;

    virtual void zoomIn() {}
    virtual void zoomOut() {}
    virtual void resetZoom() {}
    virtual void setWindowDuration(int seconds) { Q_UNUSED(seconds); }
    virtual int getWindowDuration() const { return 0; }
    virtual void addSignal(GraphSignal *signal, const BusInterfaceIdList &interfaces = {}) {
        if (!_signals.contains(signal)) {
            _signals.append(signal);
        }
        if (!interfaces.isEmpty()) {
            _signalInterfaces[signal] = interfaces;
        }
    }
    virtual void removeSignal(GraphSignal *signal) {
        _signals.removeAll(signal);
        _signalInterfaces.remove(signal);
    }
    virtual void clearSignals() { _signals.clear(); _signalInterfaces.clear(); _signalBuffers.clear(); }
    virtual QList<GraphSignal*> getSignals() const { return _signals; }

    virtual BusInterfaceIdList getSignalInterfaces(GraphSignal *signal) const {
        return _signalInterfaces.value(signal);
    }

    virtual void setGlobalStartTime(double t) { _startTime = t; }

    virtual void setSignalColor(GraphSignal *signal, const QColor &color) {
        _signalColors[signal] = color;
    }

    QColor getSignalColor(GraphSignal *signal) const {
        return _signalColors.value(signal, Qt::white);
    }

    virtual void onActivated() {}
    virtual void setActive(bool active) { Q_UNUSED(active); }

    struct GraphSignalBuffer {
        QVector<double> timestamps;
        QVector<double> values;
    };

protected:
    Backend &_backend;
    QList<GraphSignal*> _signals;
    QMap<GraphSignal*, QColor> _signalColors;
    QMap<GraphSignal*, BusInterfaceIdList> _signalInterfaces;
    QMap<GraphSignal*, GraphSignalBuffer> _signalBuffers;
    QMap<GraphSignal*, int> _syncIndices;
    double _startTime = -1.0;
};

Q_DECLARE_METATYPE(DecodedSignalData)
Q_DECLARE_METATYPE(QList<GraphSignal*>)

typedef QMap<GraphSignal*, BusInterfaceIdList> SignalInterfaceMap;
Q_DECLARE_METATYPE(SignalInterfaceMap)
