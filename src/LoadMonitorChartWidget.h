#pragma once

#include <QPixmap>
#include <QSize>
#include <QWidget>

#include "LoadMonitor.h"

// Live CPU%/memory chart for the "負荷モニター" feature (SPEC.md 追加実装
// 依頼「負荷モニター」) -- draws LoadMonitor's current ring buffer as a
// scrolling line chart: the target process' own CPU% and (Linux only)
// whole-system CPU% on a shared 0-100% axis, the target process' memory on
// its own auto-scaled axis, and a light shaded band behind any stretch
// where LoadInjector was active, so a load-injection period is visually
// obvious against the CPU/memory curves around it. Purely a renderer: it
// never samples anything itself, only repaints (via LoadMonitor::
// sampleAdded()) from whatever m_monitor already holds. Kept as a small,
// self-contained custom-painted widget rather than adding a QtCharts
// dependency, consistent with this project's other hand-painted overlays
// (RegionHighlightOverlay, PointHighlightOverlay, etc).
class LoadMonitorChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LoadMonitorChartWidget(QWidget *parent = nullptr);

    // Non-owning; may be null (nothing is drawn -- see paintEvent()).
    void setLoadMonitor(LoadMonitor *monitor);

    QSize minimumSizeHint() const override { return QSize(300, 120); }
    QSize sizeHint() const override { return QSize(500, 160); }

    // Renders the chart exactly as currently drawn into a standalone
    // QPixmap of `size` -- used both by this widget's own paintEvent() and
    // by MainWindow to save a PNG snapshot (manually, or automatically on
    // an abnormal stop alongside the CSV -- see LoadMonitor::saveSamplesAsCsv()).
    QPixmap renderToPixmap(const QSize &size) const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void paintChart(QPainter &painter, const QSize &size) const;

    LoadMonitor *m_monitor = nullptr;
};
