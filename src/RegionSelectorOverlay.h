#pragma once

#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QWidget>

// Full-screen transparent overlay used to let the user draw one or more
// rectangles on screen. Used both to pick "include" click regions and to
// pick "exclude" (mask) regions -- the caller sets which via `mode` and the
// overlay just changes the rectangle's fill/label color accordingly.
//
// Usage: call RegionSelectorOverlay::run(mode, existingIncludes, existingExcludes).
// It blocks (via a local event loop) until the user finishes (Enter/double
// left-click on empty space) or cancels (Esc), then returns the rectangles
// drawn in *this* session (possibly empty if cancelled).
class RegionSelectorOverlay : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Include, Exclude };

    static QList<QRect> run(Mode mode, const QList<QRect> &existingIncludes,
                             const QList<QRect> &existingExcludes);

signals:
    void finishedSelecting();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    explicit RegionSelectorOverlay(Mode mode, const QList<QRect> &existingIncludes,
                                    const QList<QRect> &existingExcludes,
                                    QWidget *parent = nullptr);
    void finish(bool accepted);
    void commitDraggedRect();

    Mode m_mode;
    QList<QRect> m_existingIncludes;
    QList<QRect> m_existingExcludes;
    QList<QRect> m_newRects;
    // Screenshot of the virtual desktop taken right before this overlay is
    // shown, painted as its own background (see OverlayGeometry::
    // grabVirtualDesktopSnapshot()) instead of relying on
    // Qt::WA_TranslucentBackground, which several Linux window managers
    // render as solid black when no compositor is running (SPEC.md 6.3/8).
    QPixmap m_backgroundSnapshot;

    bool m_dragging = false;
    QPoint m_dragStart;
    QRect m_currentRect;

    bool m_finished = false;
    bool m_accepted = false;
};
