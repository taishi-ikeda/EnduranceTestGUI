#include "SweepPath.h"

#include <QtMath>

namespace SweepPath
{
QList<QPoint> pointSequence(const QList<QPoint> &waypoints, int intervalPx)
{
    if (waypoints.size() < 2)
        return {};

    QList<QPoint> points;
    points << waypoints.first();
    for (int i = 0; i + 1 < waypoints.size(); ++i) {
        const QPoint &start = waypoints[i];
        const QPoint &end = waypoints[i + 1];
        const double dx = end.x() - start.x();
        const double dy = end.y() - start.y();
        const double dist = qSqrt(dx * dx + dy * dy);
        if (dist < 1.0) {
            points << end;
            continue;
        }
        const int interval = qMax(1, intervalPx);
        const int segments = qMax(1, qRound(dist / interval));
        // Starts at s=1 (not 0): waypoints[i] itself is already the last
        // point appended from the previous iteration (or the `points <<
        // waypoints.first()` above, for i==0), so repeating it here would
        // duplicate it instead of walking through it once.
        for (int s = 1; s <= segments; ++s) {
            const double t = double(s) / double(segments);
            points << QPoint(qRound(start.x() + dx * t), qRound(start.y() + dy * t));
        }
    }
    return points;
}
}  // namespace SweepPath
