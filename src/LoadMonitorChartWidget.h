#pragma once

#include <QPixmap>
#include <QSize>
#include <QWidget>

#include "LoadMonitor.h"

// Live CPU%/memory chart for the "負荷モニター" feature (SPEC.md 追加実装
// 依頼「負荷モニター」) -- draws LoadMonitor's current ring buffer as two
// stacked scrolling line charts, one above the other: CPU usage (the target
// process' own CPU% and, Linux only, whole-system CPU%, sharing a 0-100%
// axis) on top, and the target process' memory (its own auto-scaled axis)
// below -- separated per「CPUのusageとメモリ使用率を別グラフで表示して
// ください」so the two metrics, which live on very different scales, each
// get a full-height axis instead of one being squeezed onto a secondary
// axis of the other. A light shaded band behind any stretch where
// LoadInjector was active is drawn across both charts, so a load-injection
// period is visually obvious against the CPU/memory curves around it in
// either one. Purely a renderer: it never samples anything itself, only
// repaints (via LoadMonitor::sampleAdded()) from whatever m_monitor already
// holds. Kept as a small, self-contained custom-painted widget rather than
// adding a QtCharts dependency, consistent with this project's other
// hand-painted overlays (RegionHighlightOverlay, PointHighlightOverlay, etc).
class LoadMonitorChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LoadMonitorChartWidget(QWidget *parent = nullptr);

    // Non-owning; may be null (nothing is drawn -- see paintEvent()).
    void setLoadMonitor(LoadMonitor *monitor);

    // Taller than before the CPU/memory split (two stacked charts, each
    // wanting a comfortable minimum height of its own).
    QSize minimumSizeHint() const override { return QSize(300, 220); }
    QSize sizeHint() const override { return QSize(500, 300); }

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
