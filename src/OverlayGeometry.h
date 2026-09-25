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
// Union of every screen's *available* geometry (QScreen::availableGeometry(),
// i.e. excluding whatever strip the OS reserves for its own chrome -- the
// menu bar/notch area and Dock on macOS, a top/bottom panel on GNOME/KDE,
// etc.), not the screen's full physical bounds (QScreen::geometry()).
// Deliberately *not* the full geometry: an ordinary always-on-top window
// (Qt::Tool | Qt::WindowStaysOnTopHint, which is what RegionSelectorOverlay/
// PointPickerOverlay/RegionHighlightOverlay all use) is not allowed to
// actually occupy that reserved strip, so a window explicitly sized/
// positioned to cover the *full* geometry gets silently pushed/clipped away
// from it by the window manager -- its real on-screen position then differs
// from the position Qt was asked for and (initially) still reports back.
// This is what caused both the screenshot-vs-real-screen misalignment fixed
// in SPEC.md追加実装及び修正依頼 (#51, v0.79/v0.80) and a further residual
// "the drawn selection lands off by roughly the reserved strip's height"
// report on a real menu-bar/panel-having desktop even after switching to
// real transparency there (real transparency removes the *screenshot*'s own
// misalignment, but does nothing about the window itself still being pushed
// around by the same reserved-strip rule). Restricting these overlay
// windows to availableGeometry() up front means the window manager never
// needs to move/resize them away from where they were asked to be in the
// first place -- there is no reserved-strip conflict left to trigger the
// silent repositioning either fix was working around. The one user-visible
// cost is that a region can no longer be drawn literally underneath the
// menu bar/Dock/panel strip itself, which is not a real limitation in
// practice since ordinary application content is never rendered there.
inline QRect virtualDesktopGeometry()
{
    QRect all;
    for (QScreen *screen : QGuiApplication::screens())
        all = all.united(screen->availableGeometry());
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
    // Composite at the primary screen's device pixel ratio, not the
    // implicit default of 1.0 -- SPEC.md追加実装及び修正依頼 ("矩形を描画する
    // モードで選択した位置と実際に操作する操作領域がずれないように"): on a
    // HiDPI/scaled display (dpr > 1), a QPixmap left at the default ratio
    // here is, once devicePixelRatio() is accounted for, treated by Qt as
    // covering *dpr times* virtualGeom's logical size. Drawing it into the
    // overlay widget with drawPixmap(0, 0, pixmap) in paintEvent() then
    // paints it too large relative to the widget's own logical coordinate
    // space (the same space QMouseEvent::globalPosition() reports drag
    // points in) -- what the user sees drifts away from the real screen
    // toward the edges, matching the reported "選択した位置と実画面での位置
    // が少しズレている" symptom. Each per-screen grabWindow(0) pixmap below
    // already self-describes its own correct ratio and is composited
    // correctly regardless (QPainter::drawPixmap(QPoint, QPixmap) honors
    // the source pixmap's own ratio) -- only the *destination* pixmap's own
    // ratio was left unset before. Not reproducible in this project's own
    // dpr=1 Linux/Xvfb test environment (verified byte-for-byte aligned
    // there both before and after this change -- see SPEC.md for the
    // reproduction steps used), so treat this as a code-reviewed, not
    // visually-verified-on-HiDPI, fix. Mixed-dpr multi-monitor setups (each
    // screen at a different ratio) remain an approximation: one ratio is
    // used for the whole composite, matching the primary screen.
    qreal dpr = 1.0;
    if (QScreen *primaryScreen = QGuiApplication::primaryScreen())
        dpr = primaryScreen->devicePixelRatio();
    QPixmap snapshot(virtualGeom.size() * dpr);
    snapshot.setDevicePixelRatio(dpr);
    snapshot.fill(Qt::black);
    QPainter painter(&snapshot);
    for (QScreen *screen : QGuiApplication::screens()) {
        // Cropped to this screen's *available* area (see virtualDesktopGeometry()'s
        // comment) so the grabbed content lines up with virtualGeom's own
        // coordinate frame -- grabWindow(0)'s x/y/w/h are relative to this
        // screen's own top-left, which is what screen->geometry().topLeft()
        // is subtracted against here.
        const QRect avail = screen->availableGeometry();
        const QPoint availOffsetInScreen = avail.topLeft() - screen->geometry().topLeft();
        const QPixmap shot = screen->grabWindow(0, availOffsetInScreen.x(), availOffsetInScreen.y(),
                                                 avail.width(), avail.height());
        if (shot.isNull())
            continue;
        painter.drawPixmap(avail.topLeft() - virtualGeom.topLeft(), shot);
    }
    return snapshot;
}
}  // namespace OverlayGeometry
