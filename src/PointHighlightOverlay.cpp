#include "PointHighlightOverlay.h"
#include "OverlayGeometry.h"

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

    void setContent(QScreen *screen, const QList<QPoint> &points, const QList<QString> &labels)
    {
        m_screen = screen;
        m_points = points;
        m_labels = labels;
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
        update();
    }

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.drawPixmap(0, 0, m_background);

        if (m_points.size() == 2) {
            // Rects that don't overlap this screen still draw fine here --
            // the line/markers simply land (partially or fully) outside
            // the widget's own bounds, where QPainter clips them away.
            p.setPen(QPen(QColor(255, 140, 0), 2, Qt::DashLine));
            p.drawLine(m_points[0] - m_origin, m_points[1] - m_origin);
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
    }

private:
    QScreen *m_screen = nullptr;
    QPoint m_origin;
    QList<QPoint> m_points;
    QList<QString> m_labels;
    QPixmap m_background;
};

PointHighlightOverlay::PointHighlightOverlay(QObject *parent) : QObject(parent) {}

PointHighlightOverlay::~PointHighlightOverlay()
{
    qDeleteAll(m_screenWindows);
}

void PointHighlightOverlay::showPoints(const QList<QPoint> &points, const QList<QString> &labels)
{
    m_points = points;
    m_labels = labels;

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
        m_screenWindows[i]->setContent(screens[i], m_points, m_labels);
}

void PointHighlightOverlay::hide()
{
    for (PointHighlightScreenWindow *w : m_screenWindows)
        w->hide();
}
