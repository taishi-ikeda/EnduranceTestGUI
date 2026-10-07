#pragma once

#include <QList>
#include <QPoint>
#include <Qt>

class QPixmap;

// A single mouse click/double-click/drag RandomActionEngine performed,
// kept around just long enough (kMarkerLingerMs) that a recording frame
// grabbed on its own fixed interval -- ManualRecorder::captureFrame() /
// RandomActionEngine::captureRecordingFrame(), both every 500ms and so
// rarely aligned with the exact instant an action was dispatched -- still
// has a good chance to land inside the window and show it.
//
// SPEC.md 10, 「GIFアニメーション上でどこをクリックしたのか/ドラッグした
// のかわかるようにしたい」: the request was specifically about making an
// automated run's fast, hard-to-follow random clicks/drags visible after
// the fact in the exported GIF, so only the coordinate-based actions
// (Click/DoubleClick/Drag) are marked here -- scrolls and name-based
// clicks (which have no single meaningful screen point, or none at all)
// are deliberately left unmarked.
struct MouseActionMarker
{
    enum class Kind { Click, DoubleClick, Drag };

    Kind kind = Kind::Click;
    QPoint from;              // click position, or drag start
    QPoint to;                 // == from for Click/DoubleClick
    Qt::MouseButton button = Qt::LeftButton;
    qint64 timestampMs = 0;   // QDateTime::currentMSecsSinceEpoch() when dispatched
};

namespace MouseActionOverlay
{
// See MouseActionMarker's own comment for why this needs to be longer
// than either recorder's capture interval (500ms), not just "however long
// the action visually takes" -- it's a worst-case alignment margin, not a
// visual design choice on its own.
constexpr int kMarkerLingerMs = 900;

// Removes every marker older than kMarkerLingerMs relative to `nowMs` from
// `markers` -- called once per captured frame so a long-running recording
// doesn't accumulate markers forever.
void pruneExpiredMarkers(QList<MouseActionMarker> &markers, qint64 nowMs);

// Draws every marker in `markers` still within kMarkerLingerMs of
// `frameTimestampMs` onto `frame` (a window-sized capture, so each
// marker's screen-space point is translated by -captureOrigin first --
// captureOrigin being that capture's top-left corner in screen
// coordinates). A click/double-click draws as an expanding, fading ripple
// ring (red for the left button, blue for the right) with a small solid
// dot pinpointing the exact spot -- a double-click gets a second inner
// ring so it reads as distinct from a single click at a glance. A drag
// draws an orange arrow from its start to end point plus a small ring at
// the start, fading the same way.
void paintMouseActionMarkers(QPixmap &frame, const QPoint &captureOrigin,
                              const QList<MouseActionMarker> &markers, qint64 frameTimestampMs);
}  // namespace MouseActionOverlay
