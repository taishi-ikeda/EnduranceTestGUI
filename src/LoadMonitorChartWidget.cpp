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

    const int marginLeft = 36;
    const int marginRight = 36;
    const int marginTop = 20;
    const int marginBottom = 16;
    const QRect plotRect(marginLeft, marginTop, size.width() - marginLeft - marginRight,
                         size.height() - marginTop - marginBottom);

    painter.setPen(QPen(QColor(200, 200, 200)));
    painter.drawRect(plotRect);

    if (samples.size() < 2) {
        painter.setPen(QPen(QColor(140, 140, 140)));
        painter.drawText(plotRect, Qt::AlignCenter, I18n::t(QStringLiteral("データなし")));
        return;
    }

    // Horizontal 0/50/100% gridlines (left axis, shared by CPU% series).
    painter.setPen(QPen(QColor(230, 230, 230)));
    for (int pct = 0; pct <= 100; pct += 50) {
        const int y = plotRect.bottom() - int(double(pct) / 100.0 * plotRect.height());
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);
    }
    painter.setPen(QPen(QColor(120, 120, 120)));
    painter.drawText(QRect(0, plotRect.top() - 6, marginLeft - 4, 14), Qt::AlignRight, QStringLiteral("100%"));
    painter.drawText(QRect(0, plotRect.bottom() - 6, marginLeft - 4, 14), Qt::AlignRight, QStringLiteral("0%"));

    // Memory's own auto-scaled right axis (0..maxMemory, at least 1MB so a
    // perfectly flat 0MB run doesn't divide by zero).
    double maxMemory = 1.0;
    for (const LoadSample &s : samples)
        maxMemory = std::max(maxMemory, s.targetMemoryMb);
    painter.drawText(QRect(plotRect.right() + 4, plotRect.top() - 6, marginRight - 4, 14), Qt::AlignLeft,
                      QStringLiteral("%1MB").arg(maxMemory, 0, 'f', 0));
    painter.drawText(QRect(plotRect.right() + 4, plotRect.bottom() - 6, marginRight - 4, 14), Qt::AlignLeft,
                      QStringLiteral("0MB"));

    const double xStep = samples.size() > 1 ? double(plotRect.width()) / double(samples.size() - 1) : 0.0;
    auto xAt = [&](int index) { return plotRect.left() + int(double(index) * xStep); };
    auto yForPercent = [&](double percent) {
        return plotRect.bottom() - int(qBound(0.0, percent, 100.0) / 100.0 * plotRect.height());
    };
    auto yForMemory = [&](double mb) {
        return plotRect.bottom() - int(qBound(0.0, mb, maxMemory) / maxMemory * plotRect.height());
    };

    // Load-injection-active shaded bands, drawn first so every line is
    // painted on top of them, not the other way around.
    painter.setPen(Qt::NoPen);
    painter.setBrush(kLoadInjectionBandColor);
    for (int i = 0; i + 1 < samples.size(); ++i) {
        if (!samples[i].loadInjectionActive)
            continue;
        painter.drawRect(QRect(QPoint(xAt(i), plotRect.top()), QPoint(xAt(i + 1), plotRect.bottom())));
    }

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

    drawSeries(kMemoryColor, [](const LoadSample &s) { return s.targetMemoryMb; }, yForMemory);
    drawSeries(kSystemCpuColor, [](const LoadSample &s) { return s.systemCpuPercent; }, yForPercent);
    drawSeries(kTargetCpuColor, [](const LoadSample &s) { return s.targetCpuPercent; }, yForPercent);

    // Legend + latest values, top-left.
    const LoadSample &latest = samples.last();
    const QString legend =
        I18n::t(QStringLiteral("対象CPU: %1%  システムCPU: %2  メモリ: %3MB"))
            .arg(latest.targetCpuPercent, 0, 'f', 1)
            .arg(latest.systemCpuPercent >= 0.0 ? QStringLiteral("%1%").arg(latest.systemCpuPercent, 0, 'f', 1)
                                                 : I18n::t(QStringLiteral("(非対応)")))
            .arg(latest.targetMemoryMb, 0, 'f', 1);
    painter.setPen(QPen(QColor(60, 60, 60)));
    painter.drawText(QRect(marginLeft, 2, size.width() - marginLeft - marginRight, marginTop - 2), Qt::AlignLeft,
                      legend);
}
