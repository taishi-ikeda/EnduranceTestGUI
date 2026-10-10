#include "PointPickerOverlay.h"
#include "OverlayGeometry.h"
#include "platform/PlatformAutomation.h"

#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

using OverlayGeometry::globalPosOf;
using OverlayGeometry::grabVirtualDesktopSnapshot;
using OverlayGeometry::virtualDesktopGeometry;

PointPickerOverlay::PointPickerOverlay(QWidget *parent)
    : QWidget(parent)
    // Only grabbed when real window transparency isn't safe to use -- see
    // RegionSelectorOverlay's constructor for the full rationale (shared by
    // both overlays via PlatformAutomation::supportsWindowTransparency()).
    , m_backgroundSnapshot(PlatformAutomation::supportsWindowTransparency()
                                ? QPixmap()
                                : grabVirtualDesktopSnapshot())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    // Same rationale as RegionSelectorOverlay: keep this the active modal
    // surface instead of fighting a still-running modal dialog underneath
    // (e.g. SetupActionEditorDialog) while it's up.
    setWindowModality(Qt::ApplicationModal);
    // Qt::WA_TranslucentBackground only where it's known to actually render
    // as see-through -- see RegionSelectorOverlay's constructor comment.
    if (PlatformAutomation::supportsWindowTransparency())
        setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setCursor(Qt::CrossCursor);
    setGeometry(virtualDesktopGeometry());
}

bool PointPickerOverlay::run(QPoint &outPoint)
{
    PointPickerOverlay overlay;
    // Deliberately NOT showFullScreen() -- see RegionSelectorOverlay::run()
    // for why (native fullscreen hides the target window on macOS).
    overlay.show();
    overlay.setGeometry(virtualDesktopGeometry());
    overlay.activateWindow();
    overlay.raise();

    QEventLoop loop;
    QObject::connect(&overlay, &PointPickerOverlay::finishedPicking, &loop, &QEventLoop::quit);
    loop.exec();

    if (overlay.m_accepted) {
        outPoint = overlay.m_pickedPoint;
        return true;
    }
    return false;
}

bool PointPickerOverlay::runMulti(QList<QPoint> &outPoints)
{
    PointPickerOverlay overlay;
    overlay.m_multiMode = true;
    overlay.show();
    overlay.setGeometry(virtualDesktopGeometry());
    overlay.activateWindow();
    overlay.raise();

    QEventLoop loop;
    QObject::connect(&overlay, &PointPickerOverlay::finishedPicking, &loop, &QEventLoop::quit);
    loop.exec();

    if (overlay.m_accepted) {
        outPoints = overlay.m_pickedPoints;
        return true;
    }
    return false;
}

void PointPickerOverlay::finish(bool accepted, const QPoint &pt)
{
    // See RegionSelectorOverlay::finish()'s identical guard: without it,
    // a queued second mousePressEvent/keyPressEvent for this same overlay
    // (e.g. a click and an Escape arriving in quick succession) could run
    // finish() twice, re-emitting finishedPicking() after run()'s local
    // QEventLoop/overlay has already gone away.
    if (m_finished)
        return;
    m_finished = true;
    m_accepted = accepted;
    m_pickedPoint = pt;
    close();
    emit finishedPicking();
}

void PointPickerOverlay::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // No-op when real transparency is in use (m_backgroundSnapshot is a null
    // QPixmap then -- see the constructor); the real screen shows through
    // this window directly instead. The full-screen semi-transparent dark
    // tint and hint text bar this used to draw here (mirroring
    // RegionSelectorOverlay's, until the same multi-monitor "黒帯" fix was
    // applied there -- see its paintEvent() comment) were removed outright;
    // Qt::CrossCursor (set in the constructor) already signals that this
    // overlay is in point-picking mode.
    p.drawPixmap(0, 0, m_backgroundSnapshot);

    // runMulti() only: mark each point already clicked this session, so the
    // user can see what they've picked so far while deciding where to click
    // next (or whether to right-click and stop) -- same dot+crosshair style
    // as PointHighlightOverlay, which isn't reused directly here since it
    // draws onto its own separate always-on-top windows rather than this
    // overlay's own paintEvent().
    if (m_multiMode && !m_pickedPoints.isEmpty()) {
        const QPoint origin = geometry().topLeft();
        if (m_pickedPoints.size() >= 2) {
            p.setPen(QPen(QColor(255, 140, 0), 2, Qt::DashLine));
            for (int i = 0; i + 1 < m_pickedPoints.size(); ++i)
                p.drawLine(m_pickedPoints[i] - origin, m_pickedPoints[i + 1] - origin);
        }
        for (const QPoint &pt : m_pickedPoints) {
            const QPoint local = pt - origin;
            p.setPen(QPen(Qt::white, 2));
            p.setBrush(QColor(255, 120, 0));
            p.drawEllipse(local, 7, 7);
            p.drawLine(local.x() - 13, local.y(), local.x() + 13, local.y());
            p.drawLine(local.x(), local.y() - 13, local.x(), local.y() + 13);
        }
    }
}

void PointPickerOverlay::mousePressEvent(QMouseEvent *event)
{
    if (m_multiMode) {
        // Left-click keeps adding points (and keeps this overlay up for the
        // next one) instead of ending the session on the first click like
        // run() does; right-click ends it, handing back whatever was
        // collected (possibly nothing).
        if (event->button() == Qt::LeftButton) {
            m_pickedPoints.append(globalPosOf(event));
            update();
        } else if (event->button() == Qt::RightButton) {
            finish(true, QPoint());
        }
        return;
    }
    if (event->button() == Qt::LeftButton)
        finish(true, globalPosOf(event));
}

void PointPickerOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        finish(false, QPoint());
}
