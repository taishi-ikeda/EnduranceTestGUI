#pragma once

#include <QGuiApplication>
#include <QMouseEvent>
#include <QPoint>
#include <QRect>
#include <QScreen>

// Shared helpers for the full-screen always-on-top overlay widgets
// (RegionSelectorOverlay, PointPickerOverlay): the virtual desktop's
// bounding rectangle (union of every screen's geometry) and the Qt5/Qt6-
// portable way to read a QMouseEvent's global position
// (QMouseEvent::globalPos() was replaced by globalPosition() in Qt6).
// Header-only/inline so both overlays' .cpp files can use identical logic
// without one silently drifting from the other.
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
}  // namespace OverlayGeometry
