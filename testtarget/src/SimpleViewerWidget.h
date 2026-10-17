#pragma once

#include <QPoint>
#include <QWidget>

// SPEC.md 追加実装依頼「ビューワー耐久テスト強化」: a minimal, dependency-
// free (no VTK, no 3D API -- just QPainter) stand-in for a 3D viewer, so
// EnduranceTestGUI's new HoldDrag/middle-drag/modifier-drag/viewer-mode
// features have something real to exercise and visually confirm against.
// Draws a wireframe cube that responds to the same button/modifier
// bindings VTK's own default "trackball camera" interaction style uses:
//   - plain left-drag: rotate (yaw/pitch)
//   - Shift+left-drag, or middle-drag: pan
//   - Ctrl+left-drag: spin (roll)
//   - right-drag: zoom (vertical movement)
//   - wheel: zoom
// None of this is wired up to actually move geometry "for real" -- it is
// only meant to make camera-manipulation input visibly do something.
class SimpleViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SimpleViewerWidget(QWidget *parent = nullptr);

public slots:
    void resetView();

signals:
    // Emitted on a drag gesture's press/release (not per mouse-move tick,
    // and not per wheel notch -- HoldDrag can generate hundreds of those
    // per second, which would flood TestTargetWindow's log view for no
    // benefit; the live counts/values are already visible in this widget's
    // own on-screen HUD) so TestTargetWindow can mirror a short summary
    // into its own log, same as every other interactive widget there.
    void interactionLogged(const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class DragMode { None, Rotate, Pan, Spin, Zoom };
    static DragMode modeForButtonAndModifiers(Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    static QString describeDragMode(DragMode mode);

    double m_yaw = 0.5;    // radians, rotation around the vertical axis
    double m_pitch = 0.35; // radians, rotation around the horizontal axis
    double m_roll = 0.0;   // radians, rotation around the viewing axis ("spin")
    double m_panX = 0.0;
    double m_panY = 0.0;
    double m_zoom = 1.0;

    DragMode m_dragMode = DragMode::None;
    QPoint m_lastMousePos;

    // Counts shown in the on-screen HUD, so a human watching this window
    // can see at a glance that each camera operation is actually arriving
    // (not just that the view redraws) -- reset by resetView().
    qint64 m_rotateCount = 0;
    qint64 m_panCount = 0;
    qint64 m_spinCount = 0;
    qint64 m_zoomCount = 0;
    qint64 m_resizeCount = 0;
};
