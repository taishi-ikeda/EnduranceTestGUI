#include "ObjectPickerOverlay.h"
#include "OverlayGeometry.h"

#include <QEventLoop>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

using OverlayGeometry::globalPosOf;
using OverlayGeometry::grabVirtualDesktopSnapshot;
using OverlayGeometry::virtualDesktopGeometry;

ObjectPickerOverlay::ObjectPickerOverlay(qint64 targetPid, QWidget *parent)
    : QWidget(parent)
    , m_targetPid(targetPid)
    // Only grabbed when real window transparency isn't safe to use -- see
    // RegionSelectorOverlay's constructor for the full rationale (shared by
    // both overlays via PlatformAutomation::supportsWindowTransparency()).
    , m_backgroundSnapshot(PlatformAutomation::supportsWindowTransparency()
                                ? QPixmap()
                                : grabVirtualDesktopSnapshot())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    // Same rationale as RegionSelectorOverlay/PointPickerOverlay: keep this
    // the active modal surface instead of fighting a still-running modal
    // dialog underneath it (e.g. NamedRegionEditorDialog) while it's up.
    setWindowModality(Qt::ApplicationModal);
    if (PlatformAutomation::supportsWindowTransparency())
        setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    // Needed so mouseMoveEvent() fires continuously (not just while a
    // button is held) -- that's what drives the live hover highlight.
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setGeometry(virtualDesktopGeometry());
}

bool ObjectPickerOverlay::run(qint64 targetPid, PlatformAutomation::AccessibleObjectInfo &outInfo)
{
    ObjectPickerOverlay overlay(targetPid);
    // Deliberately NOT showFullScreen() -- see RegionSelectorOverlay::run()
    // for why (native fullscreen hides the target window on macOS).
    overlay.show();
    overlay.setGeometry(virtualDesktopGeometry());
    overlay.activateWindow();
    overlay.raise();

    QEventLoop loop;
    QObject::connect(&overlay, &ObjectPickerOverlay::finishedPicking, &loop, &QEventLoop::quit);
    loop.exec();

    if (overlay.m_accepted) {
        outInfo = overlay.m_pickedInfo;
        return true;
    }
    return false;
}

void ObjectPickerOverlay::finish(bool accepted, const PlatformAutomation::AccessibleObjectInfo &info)
{
    // See PointPickerOverlay::finish()'s identical guard: without it, a
    // queued second mousePressEvent/keyPressEvent for this same overlay
    // could run finish() twice, re-emitting finishedPicking() after run()'s
    // local QEventLoop/overlay has already gone away.
    if (m_finished)
        return;
    m_finished = true;
    m_accepted = accepted;
    m_pickedInfo = info;
    close();
    emit finishedPicking();
}

void ObjectPickerOverlay::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawPixmap(0, 0, m_backgroundSnapshot);

    if (!m_hoverInfo.found)
        return;

    // m_hoverInfo.bounds is in screen coordinates; this widget's own
    // geometry origin is virtualDesktopGeometry()'s top-left (which may not
    // be (0, 0) with multiple displays, the primary one not being
    // leftmost/topmost), so translate into widget-local coordinates the
    // same way RegionHighlightOverlay does before drawing.
    const QRect local = m_hoverInfo.bounds.translated(-geometry().topLeft());
    QPen pen(QColor(0, 180, 0));
    pen.setWidth(3);
    p.setPen(pen);
    p.setBrush(QColor(0, 180, 0, 40));
    p.drawRect(local);

    const QString label = m_hoverInfo.name.isEmpty()
                               ? QStringLiteral("(%1)").arg(m_hoverInfo.role)
                               : QStringLiteral("%1 \"%2\"").arg(m_hoverInfo.role, m_hoverInfo.name);
    const QRect textBounds =
        p.boundingRect(QRect(local.left(), 0, 2000, 20), Qt::AlignLeft | Qt::AlignVCenter, label);
    const QRect labelRect(local.left(), qMax(0, local.top() - 22), textBounds.width() + 12, 20);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 200));
    p.drawRect(labelRect);
    p.setPen(Qt::white);
    p.drawText(labelRect, Qt::AlignCenter, label);
}

void ObjectPickerOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const QPoint pt = globalPosOf(event);
    const PlatformAutomation::AccessibleObjectInfo info =
        PlatformAutomation::accessibleObjectAtPoint(pt, m_targetPid);
    if (info.found)
        finish(true, info);
    // else: no resolvable object at this exact point -- stay open and let
    // the user try again, rather than treating a near-miss as a cancel.
}

void ObjectPickerOverlay::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pt = globalPosOf(event);
    m_hoverInfo = PlatformAutomation::accessibleObjectAtPoint(pt, m_targetPid);
    update();
}

void ObjectPickerOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        finish(false, PlatformAutomation::AccessibleObjectInfo());
}
