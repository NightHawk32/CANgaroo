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

#include "VisualizationWidget.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QDateTime>
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>

#ifdef QT_CHARTS_USE_NAMESPACE
QT_CHARTS_USE_NAMESPACE
#endif

class TimeSeriesVisualization : public VisualizationWidget
{
    Q_OBJECT
public:
    explicit TimeSeriesVisualization(QWidget *parent, Backend &backend);
    ~TimeSeriesVisualization() override;

void addMessage(const BusMessage &msg) override;
void addDecodedData(const QMap<GraphSignal*, DecodedSignalData>& newPoints) override;
void clear() override;
void addSignal(GraphSignal *signal, const BusInterfaceIdList &interfaces = {}) override;
void clearSignals() override;
void setSignalColor(GraphSignal *signal, const QColor &color) override;
void zoomIn() override;
void zoomOut() override;
void resetZoom() override;
void setWindowDuration(int seconds) override;
    int getWindowDuration() const override { return _windowDuration; }
public slots:
void onActivated() override;
void setActive(bool active) override;
void applyTheme(ThemeManager::Theme theme) override;

    // Exposed for GraphWindow management
    QChartView* chartView() const { return _chartView; }
    QChart* chart() const { return _chart; }
    QGraphicsLineItem* cursorLine() const { return _cursorLine; }
    QGraphicsRectItem* tooltipBox() const { return _tooltipBox; }
    QGraphicsTextItem* tooltipText() const { return _tooltipText; }
    QMap<GraphSignal*, QLineSeries*> seriesMap() const { return _seriesMap; }
    QMap<GraphSignal*, QGraphicsEllipseItem*> tracers() const { return _tracers; }
    int getBusId(GraphSignal* sig) const { return _signalBusMap.value(sig, 0); }

signals:
    void mouseMoved(QMouseEvent *event);

protected:
void wheelEvent(QWheelEvent *event) override;
bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QChartView *_chartView;
    QChart *_chart;
    QMap<GraphSignal*, QLineSeries*> _seriesMap;
    QTimer *_updateTimer;
    int _windowDuration; // in seconds, 0 = all
    bool _autoScroll;
    bool _isUpdatingRange;

    void updateYAxisRange();

    QGraphicsLineItem *_cursorLine;
    QMap<GraphSignal*, QGraphicsEllipseItem*> _tracers;
    QGraphicsRectItem *_tooltipBox;
    QGraphicsTextItem *_tooltipText;
    QMap<GraphSignal*, int> _signalBusMap;

private slots:
    void onAxisRangeChanged(qreal min, qreal max);
};
