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

#include "ScatterVisualization.h"
#include <QVBoxLayout>
#include <float.h>
#include <QMouseEvent>
#include <QEvent>
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsDropShadowEffect>
#include <QPen>
#include <QBrush>
#include <QDateTime>
#include <QTimer>
#include <QtCharts/QValueAxis>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QChartView>
#include <core/ThemeManager.h>

ScatterVisualization::ScatterVisualization(QWidget *parent, Backend &backend)
    : VisualizationWidget(parent, backend), _windowDuration(0), _autoScroll(true), _isUpdatingRange(false)
{
    _updateTimer = new QTimer(this);
    connect(_updateTimer, &QTimer::timeout, this, &ScatterVisualization::onActivated);
    _updateTimer->start(100); // 10Hz sync
    _chart = new QChart();
    _chart->legend()->setVisible(true);
    _chart->legend()->setAlignment(Qt::AlignBottom);
    _chart->setTitle("Scatter (Distribution View)");

    _chartView = new QChartView(_chart);
    _chartView->setRenderHint(QPainter::Antialiasing);
    _chartView->setRubberBand(QChartView::HorizontalRubberBand);
    _chartView->setMouseTracking(true);
    _chartView->viewport()->setMouseTracking(true);
    _chartView->viewport()->installEventFilter(this);
    _chartView->installEventFilter(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(_chartView);
    layout->setContentsMargins(0, 0, 0, 0);

    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("Time [s]");
    _chart->addAxis(axisX, Qt::AlignBottom);
    connect(axisX, &QValueAxis::rangeChanged, this, &ScatterVisualization::updateAxes);
    connect(axisX, &QValueAxis::rangeChanged, this, &ScatterVisualization::onAxisRangeChanged);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Value");
    _chart->addAxis(axisY, Qt::AlignLeft);

    // Create cursor line
    _cursorLine = new QGraphicsLineItem(_chart);
    QPen cursorPen(palette().color(QPalette::WindowText), 1, Qt::DashLine);
    _cursorLine->setPen(cursorPen);
    _cursorLine->setZValue(1000);
    _cursorLine->hide();

    // Create Tooltip Box
    _tooltipBox = new QGraphicsRectItem(_chart);
    _tooltipBox->setBrush(QBrush(palette().color(QPalette::ToolTipBase)));
    _tooltipBox->setPen(QPen(palette().color(QPalette::ToolTipText), 1));
    _tooltipBox->setZValue(2000);
    _tooltipBox->hide();

    _tooltipText = new QGraphicsTextItem(_tooltipBox);
    _tooltipText->setDefaultTextColor(palette().color(QPalette::ToolTipText));
    _tooltipText->setTextInteractionFlags(Qt::NoTextInteraction);
    _tooltipText->setZValue(2001);

    // Add shadow effect to tooltip box
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(10);
    shadow->setOffset(3, 3);
    shadow->setColor(QColor(0, 0, 0, 80));
    _tooltipBox->setGraphicsEffect(shadow);

    setMouseTracking(true);
    applyTheme(ThemeManager::instance().currentTheme());
}

ScatterVisualization::~ScatterVisualization()
{
}

void ScatterVisualization::addMessage(const CanMessage &msg)
{
    double timestamp = msg.getFloatTimestamp();

    if (_startTime < 0) {
        setGlobalStartTime(timestamp);
    }

    double t = timestamp - _startTime;

    for (CanDbSignal *signal : _signals) {
        if (signal->isPresentInMessage(msg)) {
            double value = signal->extractPhysicalFromMessage(msg);
            if (_seriesMap.contains(signal)) {
                _pointBuffers[signal].append(QPointF(t, value));
                _signalBusMap[signal] = msg.getInterfaceId();
                _bufferDirty = true;
            }
        }
    }
}

void ScatterVisualization::flushBuffers()
{
    if (!_bufferDirty) return;

    for (auto it = _pointBuffers.begin(); it != _pointBuffers.end(); ++it) {
        QVector<QPointF> &buf = it.value();
        if (buf.isEmpty()) continue;

        if (buf.size() > TRIM_THRESHOLD) {
            int excess = buf.size() - MAX_POINTS;
            buf.remove(0, excess);
        }

        QScatterSeries *series = _seriesMap.value(it.key());
        if (series) {
            series->replace(buf);
        }
    }
    _bufferDirty = false;
}

void ScatterVisualization::onActivated()
{
    flushBuffers();

    if (!_autoScroll || _chart->axes(Qt::Horizontal).isEmpty()) return;

    double latestMsgT = 0;
    for (auto it = _pointBuffers.constBegin(); it != _pointBuffers.constEnd(); ++it) {
        const QVector<QPointF> &buf = it.value();
        if (!buf.isEmpty()) {
            latestMsgT = qMax(latestMsgT, buf.last().x());
        }
    }

    double t = latestMsgT;

    _isUpdatingRange = true;
    QAbstractAxis *axisX = _chart->axes(Qt::Horizontal).first();
    if (_windowDuration > 0) {
        double windowSize = static_cast<double>(_windowDuration);
        if (t > windowSize) {
            axisX->setRange(t - windowSize, t);
        } else {
            axisX->setRange(0, windowSize);
        }
    } else {
        axisX->setRange(0, qMax(10.0, t));
    }
    _isUpdatingRange = false;

    updateAxes();
}

void ScatterVisualization::setSignalColor(CanDbSignal *signal, const QColor &color)
{
    VisualizationWidget::setSignalColor(signal, color);
    if (_seriesMap.contains(signal)) {
        _seriesMap[signal]->setColor(color);
        _seriesMap[signal]->setBrush(QBrush(color));
    }
    if (_tracers.contains(signal)) {
        _tracers[signal]->setBrush(color);
    }
}

void ScatterVisualization::updateAxes()
{
    if (_chart->axes(Qt::Vertical).isEmpty() || _chart->axes(Qt::Horizontal).isEmpty() || _pointBuffers.isEmpty()) return;

    QValueAxis *axisX = qobject_cast<QValueAxis*>(_chart->axes(Qt::Horizontal).first());
    if (!axisX) return;
    double minX = axisX->min();
    double maxX = axisX->max();

    double minY = DBL_MAX;
    double maxY = -DBL_MAX;
    bool hasData = false;

    for (auto it = _pointBuffers.constBegin(); it != _pointBuffers.constEnd(); ++it) {
        const QVector<QPointF> &pts = it.value();
        if (pts.isEmpty()) continue;

        // Binary search for first point with x >= minX
        int lo = 0, hi = pts.size();
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (pts[mid].x() < minX) lo = mid + 1; else hi = mid;
        }
        int start = lo;

        // Binary search for first point with x > maxX
        lo = start; hi = pts.size();
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (pts[mid].x() <= maxX) lo = mid + 1; else hi = mid;
        }
        int end = lo;

        for (int i = start; i < end; ++i) {
            double y = pts[i].y();
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
            hasData = true;
        }
    }

    if (hasData) {
        double range = maxY - minY;
        if (range < 0.1) {
            minY -= 0.5;
            maxY += 0.5;
        } else {
            minY -= range * 0.1;
            maxY += range * 0.1;
        }
        _chart->axes(Qt::Vertical).first()->setRange(minY, maxY);
    }
}

void ScatterVisualization::clear()
{
    for (auto series : _seriesMap.values()) {
        series->clear();
    }
    for (auto &buf : _pointBuffers) {
        buf.clear();
    }
    _bufferDirty = false;
    _startTime = -1;
}

void ScatterVisualization::clearSignals()
{
    for (auto series : _seriesMap.values()) {
        _chart->removeSeries(series);
        delete series;
    }
    for (auto tracer : _tracers.values()) {
        delete tracer;
    }
    _tracers.clear();
    _seriesMap.clear();
    _pointBuffers.clear();
    _bufferDirty = false;
    _signals.clear();
    _signalBusMap.clear();
    _startTime = -1;
}

void ScatterVisualization::wheelEvent(QWheelEvent *event)
{
    _autoScroll = false;
    QValueAxis *axisX = qobject_cast<QValueAxis*>(_chart->axes(Qt::Horizontal).first());
    if (axisX) {
        double min = axisX->min();
        double max = axisX->max();
        double center = min + (max - min) * 0.5;
        double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.2;
        double newRange = (max - min) * factor;
        _isUpdatingRange = true;
        axisX->setRange(center - newRange * 0.5, center + newRange * 0.5);
        _isUpdatingRange = false;
    }
    event->accept();
}

bool ScatterVisualization::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == _chartView || watched == _chartView->viewport()) && event->type() == QEvent::MouseMove) {
        emit mouseMoved(static_cast<QMouseEvent*>(event));
    }
    return VisualizationWidget::eventFilter(watched, event);
}

void ScatterVisualization::zoomIn()
{
    _autoScroll = false;
    QValueAxis *axisX = qobject_cast<QValueAxis*>(_chart->axes(Qt::Horizontal).first());
    if (axisX) {
        double min = axisX->min();
        double max = axisX->max();
        double newRange = (max - min) * 0.8;
        double center = min + (max - min) * 0.5;
        _isUpdatingRange = true;
        axisX->setRange(center - newRange * 0.5, center + newRange * 0.5);
        _isUpdatingRange = false;
    }
}

void ScatterVisualization::zoomOut()
{
    _autoScroll = false;
    QValueAxis *axisX = qobject_cast<QValueAxis*>(_chart->axes(Qt::Horizontal).first());
    if (axisX) {
        double min = axisX->min();
        double max = axisX->max();
        double newRange = (max - min) * 1.2;
        double center = min + (max - min) * 0.5;
        _isUpdatingRange = true;
        axisX->setRange(center - newRange * 0.5, center + newRange * 0.5);
        _isUpdatingRange = false;
    }
}

void ScatterVisualization::resetZoom()
{
    _autoScroll = true;
}

void ScatterVisualization::setWindowDuration(int seconds)
{
    _windowDuration = seconds;
    _autoScroll = true;
}

void ScatterVisualization::addSignal(CanDbSignal *signal)
{
    if (_seriesMap.contains(signal)) return;

    VisualizationWidget::addSignal(signal);

    QScatterSeries *series = new QScatterSeries();
    series->setName(signal->name());
    series->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    series->setMarkerSize(7.2);
    
    // Assign a color from a list if chart doesn't provide a distinct one yet
    static const QList<Qt::GlobalColor> palette = { Qt::red, Qt::blue, Qt::green, Qt::magenta, Qt::darkYellow, Qt::cyan };
    QColor color = palette[_seriesMap.size() % palette.size()];
    series->setColor(color);
    
    _chart->addSeries(series);
    
    // Explicitly set color to ensure brush is visible
    series->setBrush(QBrush(color));
    series->setPen(QPen(Qt::NoPen)); // Clean scatter look
    
    if (!_chart->axes(Qt::Horizontal).isEmpty()) {
        series->attachAxis(_chart->axes(Qt::Horizontal).first());
    }
    if (!_chart->axes(Qt::Vertical).isEmpty()) {
        series->attachAxis(_chart->axes(Qt::Vertical).first());
    }

    _seriesMap[signal] = series;
    _pointBuffers[signal].reserve(MAX_POINTS);

    QGraphicsEllipseItem *tracer = new QGraphicsEllipseItem(-4, -4, 8, 8, _chart);
    tracer->setBrush(series->color());
    tracer->setPen(QPen(Qt::white, 1));
    tracer->setZValue(1500);
    tracer->hide();
    _tracers[signal] = tracer;
}

void ScatterVisualization::onAxisRangeChanged(qreal min, qreal max)
{
    Q_UNUSED(min);
    Q_UNUSED(max);
    if (!_isUpdatingRange) {
        _autoScroll = false;
    }
}

void ScatterVisualization::applyTheme(ThemeManager::Theme theme)
{
    const ThemeColors& colors = ThemeManager::instance().colors();
    
    _chart->setBackgroundBrush(colors.graphBackground);
    _chart->setTitleBrush(colors.windowText);
    _chart->legend()->setLabelColor(colors.windowText);

    for (auto axis : _chart->axes()) {
        axis->setLabelsColor(colors.graphAxisText);
        axis->setTitleBrush(colors.graphAxisText);
        if (auto vAxis = qobject_cast<QValueAxis*>(axis)) {
            vAxis->setGridLineColor(colors.graphGrid);
        }
    }

    _cursorLine->setPen(QPen(colors.graphCursor, 1, Qt::DashLine));
    _tooltipBox->setBrush(QBrush(colors.toolTipBase));
    _tooltipBox->setPen(QPen(colors.toolTipText, 1));
    _tooltipText->setDefaultTextColor(colors.toolTipText);

    if (theme == ThemeManager::Dark) {
        _chart->setTheme(QChart::ChartThemeDark);
    } else {
        _chart->setTheme(QChart::ChartThemeLight);
    }
    
    _chart->setBackgroundBrush(colors.graphBackground);
}
