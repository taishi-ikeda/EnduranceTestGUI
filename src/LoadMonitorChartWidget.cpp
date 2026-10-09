#include "LoadMonitorChartWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>

#include "I18n.h"

namespace
{
const QColor kTargetCpuColor(0, 120, 215);     // blue
const QColor kSystemCpuColor(220, 80, 20);     // orange
const QColor kMemoryColor(60, 160, 60);        // green
const QColor kLoadInjectionBandColor(220, 40, 40, 40);  // translucent red
}  // namespace

LoadMonitorChartWidget::LoadMonitorChartWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void LoadMonitorChartWidget::setLoadMonitor(LoadMonitor *monitor)
{
    if (m_monitor == monitor)
        return;
    if (m_monitor)
        disconnect(m_monitor, nullptr, this, nullptr);
    m_monitor = monitor;
    if (m_monitor)
        connect(m_monitor, &LoadMonitor::sampleAdded, this, qOverload<>(&QWidget::update));
    update();
}

void LoadMonitorChartWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    paintChart(painter, size());
}

QPixmap LoadMonitorChartWidget::renderToPixmap(const QSize &size) const
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    paintChart(painter, size);
    return pixmap;
}

void LoadMonitorChartWidget::paintChart(QPainter &painter, const QSize &size) const
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(QRect(QPoint(0, 0), size), Qt::white);

    const QList<LoadSample> empty;
    const QList<LoadSample> &samples = m_monitor ? m_monitor->samples() : empty;

    // 「CPUのusageとメモリ使用率を別グラフで表示してください」: two stacked
    // plot areas (CPU% on top, memory below) instead of overlaying memory on
    // a secondary axis of a single combined chart -- each metric gets a
    // full-height axis of its own.
    const int marginLeft = 36;
    const int marginRight = 8;
    const int titleHeight = 16;
    const int gapBetweenCharts = 10;
    const int topPadding = 2;
    const int bottomPadding = 4;
    const int plotWidth = size.width() - marginLeft - marginRight;

    const int availableHeight = size.height() - topPadding - bottomPadding - titleHeight * 2 - gapBetweenCharts;
    const int cpuPlotHeight = qMax(1, availableHeight / 2);
    const int memPlotHeight = qMax(1, availableHeight - cpuPlotHeight);

    const QRect cpuTitleRect(marginLeft, topPadding, plotWidth, titleHeight);
    const QRect cpuPlotRect(marginLeft, cpuTitleRect.bottom(), plotWidth, cpuPlotHeight);
    const QRect memTitleRect(marginLeft, cpuPlotRect.bottom() + gapBetweenCharts, plotWidth, titleHeight);
    const QRect memPlotRect(marginLeft, memTitleRect.bottom(), plotWidth, memPlotHeight);

    painter.setPen(QPen(QColor(200, 200, 200)));
    painter.drawRect(cpuPlotRect);
    painter.drawRect(memPlotRect);

    if (samples.size() < 2) {
        painter.setPen(QPen(QColor(140, 140, 140)));
        painter.drawText(QRect(QPoint(0, 0), size), Qt::AlignCenter, I18n::t(QStringLiteral("データなし")));
        return;
    }

    // Memory's own auto-scaled axis (0..maxMemory, at least 1MB so a
    // perfectly flat 0MB run doesn't divide by zero).
    double maxMemory = 1.0;
    for (const LoadSample &s : samples)
        maxMemory = std::max(maxMemory, s.targetMemoryMb);

    const double xStep = double(plotWidth) / double(samples.size() - 1);
    auto xAt = [&](int index) { return marginLeft + int(double(index) * xStep); };
    auto yForCpuPercent = [&](double percent) {
        return cpuPlotRect.bottom() - int(qBound(0.0, percent, 100.0) / 100.0 * cpuPlotRect.height());
    };
    auto yForMemory = [&](double mb) {
        return memPlotRect.bottom() - int(qBound(0.0, mb, maxMemory) / maxMemory * memPlotRect.height());
    };

    // Gridlines + axis labels -- CPU chart (0/50/100%).
    painter.setPen(QPen(QColor(230, 230, 230)));
    for (int pct = 0; pct <= 100; pct += 50)
        painter.drawLine(cpuPlotRect.left(), yForCpuPercent(pct), cpuPlotRect.right(), yForCpuPercent(pct));
    painter.setPen(QPen(QColor(120, 120, 120)));
    painter.drawText(QRect(0, cpuPlotRect.top() - 6, marginLeft - 4, 14), Qt::AlignRight, QStringLiteral("100%"));
    painter.drawText(QRect(0, cpuPlotRect.bottom() - 6, marginLeft - 4, 14), Qt::AlignRight, QStringLiteral("0%"));

    // Gridlines + axis labels -- memory chart (0/half/max, auto-scaled).
    painter.setPen(QPen(QColor(230, 230, 230)));
    for (int frac = 0; frac <= 100; frac += 50) {
        const int y = yForMemory(maxMemory * frac / 100.0);
        painter.drawLine(memPlotRect.left(), y, memPlotRect.right(), y);
    }
    painter.setPen(QPen(QColor(120, 120, 120)));
    painter.drawText(QRect(0, memPlotRect.top() - 6, marginLeft - 4, 14), Qt::AlignRight,
                      QStringLiteral("%1MB").arg(maxMemory, 0, 'f', 0));
    painter.drawText(QRect(0, memPlotRect.bottom() - 6, marginLeft - 4, 14), Qt::AlignRight, QStringLiteral("0MB"));

    // Load-injection-active shaded bands, drawn before the series lines so
    // every line is painted on top of them, not the other way around --
    // shown on both charts since it's context relevant to either metric.
    auto drawLoadInjectionBands = [&](const QRect &rect) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(kLoadInjectionBandColor);
        for (int i = 0; i + 1 < samples.size(); ++i) {
            if (!samples[i].loadInjectionActive)
                continue;
            painter.drawRect(QRect(QPoint(xAt(i), rect.top()), QPoint(xAt(i + 1), rect.bottom())));
        }
    };
    drawLoadInjectionBands(cpuPlotRect);
    drawLoadInjectionBands(memPlotRect);

    auto drawSeries = [&](const QColor &color, auto valueOf, auto yOf) {
        QPainterPath path;
        bool started = false;
        for (int i = 0; i < samples.size(); ++i) {
            const double value = valueOf(samples[i]);
            if (value < 0.0) {
                started = false;  // gap (e.g. system CPU% unavailable on this platform)
                continue;
            }
            const QPoint point(xAt(i), yOf(value));
            if (!started) {
                path.moveTo(point);
                started = true;
            } else {
                path.lineTo(point);
            }
        }
        painter.setPen(QPen(color, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    };

    drawSeries(kSystemCpuColor, [](const LoadSample &s) { return s.systemCpuPercent; }, yForCpuPercent);
    drawSeries(kTargetCpuColor, [](const LoadSample &s) { return s.targetCpuPercent; }, yForCpuPercent);
    drawSeries(kMemoryColor, [](const LoadSample &s) { return s.targetMemoryMb; }, yForMemory);

    // Per-chart title + latest values, replacing the old single combined
    // legend line now that CPU and memory are two separate charts.
    const LoadSample &latest = samples.last();
    painter.setPen(QPen(QColor(60, 60, 60)));
    painter.drawText(cpuTitleRect, Qt::AlignLeft | Qt::AlignVCenter,
                      I18n::t(QStringLiteral("CPU使用率（対象: %1%  システム: %2）"))
                          .arg(latest.targetCpuPercent, 0, 'f', 1)
                          .arg(latest.systemCpuPercent >= 0.0
                                   ? QStringLiteral("%1%").arg(latest.systemCpuPercent, 0, 'f', 1)
                                   : I18n::t(QStringLiteral("(非対応)"))));
    painter.drawText(memTitleRect, Qt::AlignLeft | Qt::AlignVCenter,
                      I18n::t(QStringLiteral("メモリ使用量（%1MB）")).arg(latest.targetMemoryMb, 0, 'f', 1));
}
