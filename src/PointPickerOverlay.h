#pragma once

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

signals:
    void finishedPicking();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    explicit PointPickerOverlay(QWidget *parent = nullptr);
    void finish(bool accepted, const QPoint &pt);

    bool m_accepted = false;
    QPoint m_pickedPoint;
};
