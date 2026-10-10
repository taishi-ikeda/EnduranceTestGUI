#pragma once

#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QWidget>

// Full-screen transparent overlay used to let the user pick a single point
// on screen by clicking it, instead of typing pixel coordinates by hand
// (SPEC.md 6.x "起動時セットアップ" ③) -- used by SetupActionEditorDialog
// for a click/double-click/right-click/drag SetupAction's point(s).
// Deliberately simpler than RegionSelectorOverlay (which drags out
// rectangles and allows several): this is a single left-click, done.
//
// Usage: call PointPickerOverlay::run(). It blocks (via a local event
// loop) until the user clicks a point or cancels (Esc), then returns
// true/the picked point, or false if cancelled.
class PointPickerOverlay : public QWidget
{
    Q_OBJECT

public:
    // Returns true and fills `outPoint` (screen coordinates) if the user
    // clicked a point; returns false (outPoint left unset) if cancelled.
    static bool run(QPoint &outPoint);

    // Like run(), but for picking several points in one sitting instead of
    // one: each left-click adds a point (shown immediately as a marker, so
    // the user can see what's been picked so far) and the overlay stays up
    // for the next one, instead of ending the session like run() does on
    // its first click. A right-click ends the session, returning true and
    // filling `outPoints` with everything collected (possibly empty, if
    // right-clicked before any left-click). Esc still cancels the whole
    // session (returns false, outPoints left unset) -- used by
    // NamedRegionEditorDialog's 点列（スイープ）waypoint picker, so adding
    // several points doesn't require re-clicking "点を追加..." before each
    // one (SPEC.md 追加実装依頼「点列の点を連続して追加できるようにする」).
    static bool runMulti(QList<QPoint> &outPoints);

signals:
    void finishedPicking();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    explicit PointPickerOverlay(QWidget *parent = nullptr);
    void finish(bool accepted, const QPoint &pt);

    bool m_finished = false;
    bool m_accepted = false;
    QPoint m_pickedPoint;
    // Set only by runMulti(); mousePressEvent()/paintEvent() both branch on
    // this to switch between run()'s single-click-and-done behavior and
    // runMulti()'s accumulate-until-right-click one.
    bool m_multiMode = false;
    QList<QPoint> m_pickedPoints;
    // Screenshot of the virtual desktop taken right before this overlay is
    // shown, painted as its own background instead of relying on
    // Qt::WA_TranslucentBackground -- see RegionSelectorOverlay's identical
    // field and OverlayGeometry::grabVirtualDesktopSnapshot() for why.
    QPixmap m_backgroundSnapshot;
};
