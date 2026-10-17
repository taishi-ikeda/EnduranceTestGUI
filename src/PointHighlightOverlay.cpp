#include "PointHighlightOverlay.h"
#include "OverlayGeometry.h"
#include "platform/PlatformAutomation.h"

#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QWidget>

using OverlayGeometry::settleDesktopBeforeSnapshot;

// One borderless, click-through, always-on-top window per QScreen -- same
// pattern as RegionHighlightOverlay's per-screen window, see its .cpp for
// the full rationale (mixed-scale-factor multi-monitor correctness, and
// why a background snapshot is painted instead of relying on
// Qt::WA_TranslucentBackground: without a compositor, that attribute
// renders as solid black instead of blending, which for a window covering
// a whole screen would hide the entire screen -- SPEC.md 6.3/6.13/8).
class PointHighlightScreenWindow : public QWidget
{
public:
    explicit PointHighlightScreenWindow() : QWidget(nullptr)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    void setContent(QScreen *screen, const QList<QPoint> &points, const QList<QString> &labels,
                     const QList<QPoint> &intervalMarks = {}, int jitterPx = 0)
    {
        m_screen = screen;
        m_points = points;
        m_labels = labels;
        m_intervalMarks = intervalMarks;
        m_jitterPx = jitterPx;
        // See RegionHighlightOverlay::setContent()'s identical guard: only
        // re-grab while not already visible, so a still-visible window
        // doesn't capture its own last-painted frame instead of the real
        // desktop, and settle first so a just-closed overlay (e.g.
        // PointPickerOverlay, dismissed right before this call) has
        // actually finished unmapping before the raw grab (SPEC.md 6.13).
        if (!isVisible()) {
            settleDesktopBeforeSnapshot();
            m_background = screen->grabWindow(0);
        }
        show();
        const QRect screenGeom = m_screen->geometry();
        setGeometry(screenGeom);
        m_origin = screenGeom.topLeft();
        raise();
        // 不具合報告D20(中): Qt::WA_TransparentForMouseEvents (set once, in
        // the constructor) was found unreliable for this exact kind of
        // window -- full-screen, Qt::WindowStaysOnTopHint, repeatedly
        // shown/hidden, under a bare (non-compositing) X11 window manager --
        // same exact failure mode RegionHighlightOverlay already hit
        // (SPEC.md 10 不具合報告 D1). This window is shown directly on top
        // of a still-open dialog (起動時セットアップ's position marker, or a
        // sweep region's already-added waypoints) while that dialog's own
        // "OK"/"点を追加..." button is waiting for the next click, so it was
        // absorbing that click instead of letting it reach the dialog.
        // Re-asserted on every show()/resize rather than only once in the
        // constructor, since this needs a valid native window handle
        // (winId()), which show() is what actually creates.
        PlatformAutomation::setWindowClickThrough(quint32(winId()));
        update();
    }

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.drawPixmap(0, 0, m_background);

        if (m_points.size() >= 2) {
            // Rects that don't overlap this screen still draw fine here --
            // the line/markers simply land (partially or fully) outside
            // the widget's own bounds, where QPainter clips them away. A
            // line per consecutive pair (not just points[0]-points[1]) so
            // an N-point sweep (SPEC.md 追加実装依頼「操作領域を点列
            // （スイープ）で指定」中点対応) draws as one connected path
            // through every waypoint in order, not just its endpoints.
            p.setPen(QPen(QColor(255, 140, 0), 2, Qt::DashLine));
            for (int i = 0; i + 1 < m_points.size(); ++i)
                p.drawLine(m_points[i] - m_origin, m_points[i + 1] - m_origin);
        }

        for (int i = 0; i < m_points.size(); ++i) {
            const QPoint local = m_points[i] - m_origin;
            p.setPen(QPen(Qt::white, 2));
            p.setBrush(QColor(255, 120, 0));
            p.drawEllipse(local, 7, 7);
            p.drawLine(local.x() - 13, local.y(), local.x() + 13, local.y());
            p.drawLine(local.x(), local.y() - 13, local.x(), local.y() + 13);

            if (i >= m_labels.size() || m_labels[i].isEmpty())
                continue;
            p.setFont(QFont(font().family(), 11, QFont::Bold));
            const QFontMetrics fm(p.font());
            const QRect labelBg(local.x() + 12, local.y() - 11, fm.horizontalAdvance(m_labels[i]) + 12, 22);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(200, 90, 0, 235));
            p.drawRect(labelBg);
            p.setPen(Qt::white);
            p.drawText(labelBg, Qt::AlignCenter, m_labels[i]);
        }

        // Interval marks + jitter error bars (SPEC.md 追加実装依頼「点列の
        // 間隔マーク・ランダム幅のエラーバー表示」): drawn in addition to
        // the waypoint markers/path above, not replacing them, so both the
        // raw waypoints the user clicked and the actual interval-spaced
        // sequence RandomActionEngine::pickSweepPoint() steps through at
        // runtime (see SweepPath::pointSequence()) are visible at once.
        // Small dot (no label -- there can be many) for each mark, plus a
        // cross-shaped error bar sized ±m_jitterPx when jitter is nonzero,
        // depicting the independent random x/y offset applied at runtime.
        if (!m_intervalMarks.isEmpty()) {
            p.setPen(QPen(QColor(0, 160, 220), 2));
            p.setBrush(QColor(0, 160, 220, 200));
            for (const QPoint &mark : m_intervalMarks) {
                const QPoint local = mark - m_origin;
                if (m_jitterPx > 0) {
                    const int j = m_jitterPx;
                    p.drawLine(local.x() - j, local.y(), local.x() + j, local.y());
                    p.drawLine(local.x() - j, local.y() - 4, local.x() - j, local.y() + 4);
                    p.drawLine(local.x() + j, local.y() - 4, local.x() + j, local.y() + 4);
                    p.drawLine(local.x(), local.y() - j, local.x(), local.y() + j);
                    p.drawLine(local.x() - 4, local.y() - j, local.x() + 4, local.y() - j);
                    p.drawLine(local.x() - 4, local.y() + j, local.x() + 4, local.y() + j);
                }
                p.drawEllipse(local, 3, 3);
            }
        }
    }

private:
    QScreen *m_screen = nullptr;
    QPoint m_origin;
    QList<QPoint> m_points;
    QList<QString> m_labels;
    QList<QPoint> m_intervalMarks;
    int m_jitterPx = 0;
    QPixmap m_background;
};

PointHighlightOverlay::PointHighlightOverlay(QObject *parent) : QObject(parent) {}

PointHighlightOverlay::~PointHighlightOverlay()
{
    qDeleteAll(m_screenWindows);
}

void PointHighlightOverlay::showPoints(const QList<QPoint> &points, const QList<QString> &labels)
{
    showSweepPath(points, labels, {}, 0);
}

void PointHighlightOverlay::showSweepPath(const QList<QPoint> &waypoints, const QList<QString> &waypointLabels,
                                           const QList<QPoint> &intervalMarks, int jitterPx)
{
    m_points = waypoints;
    m_labels = waypointLabels;
    m_intervalMarks = intervalMarks;
    m_jitterPx = jitterPx;

    const QList<QScreen *> screens = QGuiApplication::screens();

    // Reused across calls rather than destroyed and recreated -- see
    // RegionHighlightOverlay::showRegion()'s identical comment for why
    // (avoids visible hangs recreating native windows on every point pick).
    if (m_screenWindows.size() != screens.size()) {
        qDeleteAll(m_screenWindows);
        m_screenWindows.clear();
        for (int i = 0; i < screens.size(); ++i)
            m_screenWindows.append(new PointHighlightScreenWindow());
    }

    for (int i = 0; i < m_screenWindows.size(); ++i)
        m_screenWindows[i]->setContent(screens[i], m_points, m_labels, m_intervalMarks, m_jitterPx);
}

void PointHighlightOverlay::hide()
{
    for (PointHighlightScreenWindow *w : m_screenWindows)
        w->hide();
}
