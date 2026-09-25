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
}

void PointPickerOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        finish(true, globalPosOf(event));
}

void PointPickerOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        finish(false, QPoint());
}
