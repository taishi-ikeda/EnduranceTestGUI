#include "MouseActionOverlay.h"

#include <QColor>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QtMath>

namespace MouseActionOverlay
{
void pruneExpiredMarkers(QList<MouseActionMarker> &markers, qint64 nowMs)
{
    for (int i = markers.size() - 1; i >= 0; --i) {
        if (nowMs - markers[i].timestampMs > kMarkerLingerMs)
            markers.removeAt(i);
    }
}

namespace
{
QColor clickColor(Qt::MouseButton button)
{
    return button == Qt::RightButton ? QColor(50, 130, 255) : QColor(255, 60, 60);
}
}  // namespace

void paintMouseActionMarkers(QPixmap &frame, const QPoint &captureOrigin,
                              const QList<MouseActionMarker> &markers, qint64 frameTimestampMs)
{
    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const MouseActionMarker &marker : markers) {
        const qint64 age = frameTimestampMs - marker.timestampMs;
        if (age < 0 || age > kMarkerLingerMs)
            continue;
        // 0.0 right when the action happened, 1.0 just as it's about to
        // stop being drawn at all -- drives both the fade-out and (for a
        // click) the ripple's outward growth.
        const double progress = double(age) / double(kMarkerLingerMs);
        const int alpha = int(255 * (1.0 - progress));
        const QPoint from = marker.from - captureOrigin;
        const QPoint to = marker.to - captureOrigin;

        if (marker.kind == MouseActionMarker::Kind::Drag) {
            QColor lineColor(255, 160, 0, alpha);  // orange: distinct from a plain click's red/blue
            painter.setPen(QPen(lineColor, 3));
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(from, to);
            painter.drawEllipse(from, 6, 6);  // ring at the drag's start point

            const QLineF line(from, to);
            if (line.length() > 0.5) {
                const double angle = std::atan2(-line.dy(), line.dx());
                constexpr double kArrowSize = 12.0;
                const QPointF tip(to);
                const QPointF p1 = tip - QPointF(std::cos(angle - M_PI / 6) * kArrowSize,
                                                  -std::sin(angle - M_PI / 6) * kArrowSize);
                const QPointF p2 = tip - QPointF(std::cos(angle + M_PI / 6) * kArrowSize,
                                                  -std::sin(angle + M_PI / 6) * kArrowSize);
                painter.setPen(Qt::NoPen);
                painter.setBrush(lineColor);
                painter.drawPolygon(QPolygonF() << tip << p1 << p2);
            }
            continue;
        }

        // Click/DoubleClick: an expanding, fading ripple ring plus a small
        // solid dot pinpointing the exact spot.
        constexpr int kBaseRadius = 6;
        constexpr int kMaxGrowth = 18;
        const int radius = kBaseRadius + int(progress * kMaxGrowth);
        QColor ringColor = clickColor(marker.button);
        ringColor.setAlpha(alpha);
        painter.setPen(QPen(ringColor, 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(from, radius, radius);
        if (marker.kind == MouseActionMarker::Kind::DoubleClick) {
            const int innerRadius = qMax(2, radius - 9);
            painter.drawEllipse(from, innerRadius, innerRadius);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(ringColor);
        painter.drawEllipse(from, 3, 3);
    }
}
}  // namespace MouseActionOverlay
