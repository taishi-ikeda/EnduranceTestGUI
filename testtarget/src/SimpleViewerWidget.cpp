#include "SimpleViewerWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>

#include <cmath>

namespace
{
// A unit cube's 8 corners and 12 edges (pairs of vertex indices) -- the
// only "geometry" this stand-in viewer needs.
constexpr double kVertices[8][3] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
};
constexpr int kEdges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
    {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
};
}  // namespace

SimpleViewerWidget::SimpleViewerWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(220);
    setMouseTracking(false);
    setFocusPolicy(Qt::StrongFocus);
}

void SimpleViewerWidget::resetView()
{
    m_yaw = 0.5;
    m_pitch = 0.35;
    m_roll = 0.0;
    m_panX = 0.0;
    m_panY = 0.0;
    m_zoom = 1.0;
    m_rotateCount = 0;
    m_panCount = 0;
    m_spinCount = 0;
    m_zoomCount = 0;
    m_resizeCount = 0;
    update();
}

SimpleViewerWidget::DragMode SimpleViewerWidget::modeForButtonAndModifiers(Qt::MouseButton button,
                                                                            Qt::KeyboardModifiers modifiers)
{
    if (button == Qt::MiddleButton)
        return DragMode::Pan;
    if (button == Qt::RightButton)
        return DragMode::Zoom;
    if (button == Qt::LeftButton) {
        if (modifiers & Qt::ShiftModifier)
            return DragMode::Pan;
        if (modifiers & Qt::ControlModifier)
            return DragMode::Spin;
        return DragMode::Rotate;
    }
    return DragMode::None;
}

QString SimpleViewerWidget::describeDragMode(DragMode mode)
{
    switch (mode) {
    case DragMode::Rotate: return QStringLiteral("回転(rotate)");
    case DragMode::Pan: return QStringLiteral("パン(pan)");
    case DragMode::Spin: return QStringLiteral("スピン(spin)");
    case DragMode::Zoom: return QStringLiteral("ズーム(zoom)");
    case DragMode::None:
    default: return QStringLiteral("なし");
    }
}

void SimpleViewerWidget::mousePressEvent(QMouseEvent *event)
{
    m_dragMode = modeForButtonAndModifiers(event->button(), event->modifiers());
    m_lastMousePos = event->pos();
    if (m_dragMode != DragMode::None)
        emit interactionLogged(QStringLiteral("ビューワー: %1 開始").arg(describeDragMode(m_dragMode)));
    event->accept();
}

void SimpleViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragMode == DragMode::None) {
        event->accept();
        return;
    }
    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();
    switch (m_dragMode) {
    case DragMode::Rotate:
        m_yaw += delta.x() * 0.01;
        m_pitch += delta.y() * 0.01;
        ++m_rotateCount;
        break;
    case DragMode::Pan:
        m_panX += delta.x();
        m_panY += delta.y();
        ++m_panCount;
        break;
    case DragMode::Spin:
        m_roll += delta.x() * 0.01;
        ++m_spinCount;
        break;
    case DragMode::Zoom: {
        const double factor = std::pow(1.01, -delta.y());
        m_zoom = qBound(0.05, m_zoom * factor, 20.0);
        ++m_zoomCount;
        break;
    }
    case DragMode::None:
    default:
        break;
    }
    update();
    event->accept();
}

void SimpleViewerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragMode != DragMode::None)
        emit interactionLogged(QStringLiteral("ビューワー: %1 終了").arg(describeDragMode(m_dragMode)));
    m_dragMode = DragMode::None;
    event->accept();
}

void SimpleViewerWidget::wheelEvent(QWheelEvent *event)
{
    // One "notch" is 120 in Qt's angleDelta units -- scale smoothly with
    // however many notches this single event reports (a HoldDrag wheel
    // overlay sent at a very short interval may coalesce several notches
    // into one event).
    const double notches = event->angleDelta().y() / 120.0;
    m_zoom = qBound(0.05, m_zoom * std::pow(1.1, notches), 20.0);
    ++m_zoomCount;
    update();
    event->accept();
}

void SimpleViewerWidget::resizeEvent(QResizeEvent *event)
{
    ++m_resizeCount;
    QWidget::resizeEvent(event);
}

void SimpleViewerWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(20, 24, 32));

    const double cx = width() / 2.0 + m_panX;
    const double cy = height() / 2.0 + m_panY;
    const double baseScale = qMin(width(), height()) * 0.18 * m_zoom;

    const double cosY = std::cos(m_yaw), sinY = std::sin(m_yaw);
    const double cosP = std::cos(m_pitch), sinP = std::sin(m_pitch);
    const double cosR = std::cos(m_roll), sinR = std::sin(m_roll);

    QPointF projected[8];
    for (int i = 0; i < 8; ++i) {
        const double x0 = kVertices[i][0], y0 = kVertices[i][1], z0 = kVertices[i][2];
        // Rotate around Y (yaw), then X (pitch), then Z (roll/spin).
        const double x1 = x0 * cosY + z0 * sinY;
        const double z1 = -x0 * sinY + z0 * cosY;
        const double y1 = y0;
        const double y2 = y1 * cosP - z1 * sinP;
        const double z2 = y1 * sinP + z1 * cosP;
        const double x2 = x1;
        const double x3 = x2 * cosR - y2 * sinR;
        const double y3 = x2 * sinR + y2 * cosR;
        const double z3 = z2;
        // Simple weak-perspective projection: things further along z3
        // shrink slightly, so the cube still reads as 3D.
        const double depthFactor = 4.0 / (4.0 + z3);
        projected[i] = QPointF(cx + x3 * baseScale * depthFactor, cy - y3 * baseScale * depthFactor);
    }

    painter.setPen(QPen(QColor(120, 200, 255), 2));
    for (const auto &edge : kEdges)
        painter.drawLine(projected[edge[0]], projected[edge[1]]);
    painter.setBrush(QColor(120, 200, 255));
    for (const QPointF &p : projected)
        painter.drawEllipse(p, 3, 3);

    painter.setPen(QColor(210, 210, 210));
    const QString hud = QStringLiteral("yaw:%1 pitch:%2 roll:%3 zoom:%4 pan:(%5,%6)\n"
                                        "回転:%7 パン:%8 スピン:%9 ズーム:%10 リサイズ:%11  size:%12x%13")
                             .arg(m_yaw, 0, 'f', 2)
                             .arg(m_pitch, 0, 'f', 2)
                             .arg(m_roll, 0, 'f', 2)
                             .arg(m_zoom, 0, 'f', 2)
                             .arg(qRound(m_panX))
                             .arg(qRound(m_panY))
                             .arg(m_rotateCount)
                             .arg(m_panCount)
                             .arg(m_spinCount)
                             .arg(m_zoomCount)
                             .arg(m_resizeCount)
                             .arg(width())
                             .arg(height());
    painter.drawText(QRectF(8, 8, width() - 16.0, 48), Qt::AlignLeft | Qt::TextWordWrap, hud);
}
