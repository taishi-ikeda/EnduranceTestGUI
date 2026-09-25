#include "RegionSelectorOverlay.h"
#include "OverlayGeometry.h"
#include "platform/PlatformAutomation.h"

#include <QApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

using OverlayGeometry::globalPosOf;
using OverlayGeometry::grabVirtualDesktopSnapshot;
using OverlayGeometry::virtualDesktopGeometry;

RegionSelectorOverlay::RegionSelectorOverlay(Mode mode, const QList<QRect> &existingIncludes,
                                              const QList<QRect> &existingExcludes,
                                              QWidget *parent)
    : QWidget(parent)
    , m_mode(mode)
    , m_existingIncludes(existingIncludes)
    , m_existingExcludes(existingExcludes)
    // Only grabbed when real window transparency isn't safe to use (see
    // PlatformAutomation::supportsWindowTransparency()) -- when it is, this
    // window is genuinely see-through instead, so there is no screenshot to
    // capture or to ever drift out of alignment with the real screen. Must
    // happen before this (still invisible) widget is shown when it *is*
    // grabbed -- see grabVirtualDesktopSnapshot()'s comment.
    , m_backgroundSnapshot(PlatformAutomation::supportsWindowTransparency()
                                ? QPixmap()
                                : grabVirtualDesktopSnapshot())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    // This overlay is typically launched from inside a modal QDialog
    // (StepEditorDialog) that stays running (hidden or not) for the
    // duration -- on macOS a QDialog::exec() modal session doesn't end
    // just because the dialog is hidden. Making this overlay itself
    // application-modal makes it the active modal surface while it's up,
    // instead of being a separate, unrelated top-level window fighting
    // the still-active modal session underneath (which was left the app
    // unresponsive after the overlay closed).
    setWindowModality(Qt::ApplicationModal);
    // Qt::WA_TranslucentBackground only where PlatformAutomation::
    // supportsWindowTransparency() says it will actually render as
    // see-through (see its header comment) -- otherwise this window paints
    // m_backgroundSnapshot as an opaque background instead (paintEvent()).
    if (PlatformAutomation::supportsWindowTransparency())
        setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    setGeometry(virtualDesktopGeometry());
}

QList<QRect> RegionSelectorOverlay::run(Mode mode, const QList<QRect> &existingIncludes,
                                         const QList<QRect> &existingExcludes)
{
    RegionSelectorOverlay overlay(mode, existingIncludes, existingExcludes);
    // Deliberately NOT showFullScreen(): on macOS that enters native
    // fullscreen (a separate Space), which hides every other window --
    // including the target app the user needs to see to draw regions
    // over. A plain show() at the explicit virtual-desktop geometry set in
    // the constructor covers the same area as a borderless always-on-top
    // window without leaving the normal desktop/Space, so the background
    // snapshot painted in paintEvent() (captured in the constructor, before
    // this show()) still matches what the user actually sees underneath.
    overlay.show();
    overlay.setGeometry(virtualDesktopGeometry());
    overlay.activateWindow();
    overlay.raise();

    QEventLoop loop;
    QObject::connect(&overlay, &RegionSelectorOverlay::finishedSelecting, &loop, &QEventLoop::quit);
    loop.exec();

    return overlay.m_accepted ? overlay.m_newRects : QList<QRect>();
}

void RegionSelectorOverlay::finish(bool accepted)
{
    // Guards against finish() running twice for what the user experiences
    // as a single confirm gesture -- e.g. a fast double right-click (each
    // press finishes independently: mousePressEvent's right-button branch
    // AND mouseDoubleClickEvent() below both call finish() unconditionally),
    // or a platform event-delivery quirk where a second press/click for the
    // same physical input gets redelivered after close() has already
    // started hiding this overlay (this has been reported as "two dialogs
    // appear" after confirming a rectangle -- SPEC.md追加実装及び修正依頼).
    // Without this, a second finish() call re-emits finishedSelecting()
    // after run()'s local QEventLoop has already (or is about to) return,
    // which can requeue a second confirm on whatever ends up on top once
    // this overlay closes (e.g. the same "矩形を描画..." button underneath,
    // reopening a second overlay/dialog) or worse, a stale-object access
    // once run()'s stack-local `overlay` has already been destroyed.
    if (m_finished)
        return;
    m_accepted = accepted;
    m_finished = true;
    close();
    emit finishedSelecting();
}

void RegionSelectorOverlay::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // No-op (draws nothing) when PlatformAutomation::supportsWindowTransparency()
    // is true: m_backgroundSnapshot is a default-constructed null QPixmap in
    // that case (see the constructor), and the real screen shows through this
    // window's own transparency instead -- nothing painted here can drift out
    // of alignment with it. SPEC.md追加実装及び修正依頼 has the full history
    // of why a captured screenshot is still used at all on platforms where
    // real transparency isn't safe (a semi-transparent black tint and a hint
    // text bar also used to be drawn over this background; both were removed
    // outright as unnecessary -- the drawn rectangles below plus the
    // Qt::CrossCursor set in the constructor are enough indication that this
    // overlay is in region-drawing mode).
    p.drawPixmap(0, 0, m_backgroundSnapshot);
    const QPoint origin = geometry().topLeft();

    auto drawRectList = [&](const QList<QRect> &rects, const QColor &fill, const QColor &border) {
        for (const QRect &r : rects) {
            const QRect local = r.translated(-origin);
            p.setPen(QPen(border, 2));
            p.setBrush(fill);
            p.drawRect(local);
        }
    };

    drawRectList(m_existingIncludes, QColor(0, 200, 0, 40), QColor(0, 200, 0, 160));
    drawRectList(m_existingExcludes, QColor(220, 0, 0, 60), QColor(220, 0, 0, 180));

    const QColor newFill = (m_mode == Mode::Include) ? QColor(0, 220, 0, 90)
                                                       : QColor(230, 30, 30, 110);
    const QColor newBorder = (m_mode == Mode::Include) ? QColor(0, 255, 0) : QColor(255, 60, 60);
    drawRectList(m_newRects, newFill, newBorder);

    if (m_dragging) {
        const QRect local = m_currentRect.translated(-origin);
        p.setPen(QPen(newBorder, 2, Qt::DashLine));
        p.setBrush(newFill);
        p.drawRect(local);
    }
    // The "含める領域を選択中 ― ドラッグで矩形を追加..." hint text bar (and
    // the full-screen dark tint it used to sit on) was removed here per
    // the request above -- see the comment earlier in this function.
}

void RegionSelectorOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = globalPosOf(event);
        m_currentRect = QRect(m_dragStart, m_dragStart);
        update();
    } else if (event->button() == Qt::RightButton) {
        finish(true);
    }
}

void RegionSelectorOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_currentRect = QRect(m_dragStart, globalPosOf(event)).normalized();
        update();
    }
}

void RegionSelectorOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        if (m_currentRect.width() >= 4 && m_currentRect.height() >= 4)
            m_newRects.append(m_currentRect);
        m_currentRect = QRect();
        update();
    }
}

void RegionSelectorOverlay::mouseDoubleClickEvent(QMouseEvent * /*event*/)
{
    finish(true);
}

void RegionSelectorOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        finish(false);
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finish(true);
    }
}
