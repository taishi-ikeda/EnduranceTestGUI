#pragma once

#include <QCoreApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QThread>

// Shared helpers for the full-screen always-on-top overlay widgets
// (RegionSelectorOverlay, PointPickerOverlay): the virtual desktop's
// bounding rectangle (union of every screen's geometry), a live screenshot
// of it, and the Qt5/Qt6-portable way to read a QMouseEvent's global
// position (QMouseEvent::globalPos() was replaced by globalPosition() in
// Qt6). Header-only/inline so both overlays' .cpp files can use identical
// logic without one silently drifting from the other.
namespace OverlayGeometry
{
inline QRect virtualDesktopGeometry()
{
    QRect all;
    for (QScreen *screen : QGuiApplication::screens())
        all = all.united(screen->geometry());
    return all;
}

inline QPoint globalPosOf(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

// Gives the window manager/X server a brief moment to finish processing
// whatever was just hidden or closed on screen (typically another overlay
// window, dismissed immediately before this call) before a raw
// screen->grabWindow(0) grab. Unmapping a window is asynchronous -- the
// exposed windows underneath still need to repaint before the on-screen
// pixels actually reflect their removal -- so grabbing right after can
// otherwise capture leftover content from something that is already
// logically gone. Confirmed on Xvfb+openbox: RegionHighlightOverlay
// grabbing its background right after RegionSelectorOverlay closed could
// still capture RegionSelectorOverlay's hint-text bar, frozen into the
// highlight's snapshot from then on (SPEC.md 6.3/8). Called before every
// raw grab in this file, not just once, since the same race applies
// whichever overlay is grabbing next.
inline void settleDesktopBeforeSnapshot()
{
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep(20);
    }
}

// Grabs a screenshot of the whole virtual desktop (every screen, composited
// into one pixmap covering virtualDesktopGeometry()), for use as an
// overlay's own background instead of Qt::WA_TranslucentBackground.
//
// True window-level translucency (WA_TranslucentBackground) requires a
// compositing manager to actually blend the overlay against whatever is
// behind it; without one (common on minimal/tiling Linux window managers --
// e.g. a bare openbox session with no compositor running), the ARGB alpha
// channel is simply not composited and the window renders as solid black
// instead, leaving the user unable to see the target window they're trying
// to draw a region over (SPEC.md 6.3/6.13/8). Painting a snapshot taken
// *before* the overlay is shown, then drawing everything else on top of it
// within the same (fully opaque) paint buffer, reproduces the same "see
// the desktop through the overlay" effect without depending on the window
// manager/compositor at all -- it works identically with or without one.
// Only safe for an overlay that is shown briefly and modally (the
// screenshot is a frozen frame, not a live view), which both
// RegionSelectorOverlay and PointPickerOverlay are.
inline QPixmap grabVirtualDesktopSnapshot()
{
    settleDesktopBeforeSnapshot();
    const QRect virtualGeom = virtualDesktopGeometry();
    QPixmap snapshot(virtualGeom.size());
    snapshot.fill(Qt::black);
    QPainter painter(&snapshot);
    for (QScreen *screen : QGuiApplication::screens()) {
        const QPixmap shot = screen->grabWindow(0);
        if (shot.isNull())
            continue;
        painter.drawPixmap(screen->geometry().topLeft() - virtualGeom.topLeft(), shot);
    }
    return snapshot;
}
}  // namespace OverlayGeometry
