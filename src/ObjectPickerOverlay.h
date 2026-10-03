#pragma once

#include "platform/PlatformAutomation.h"

#include <QPixmap>
#include <QWidget>

// Full-screen overlay used to pick a UI object (button, menu item, check
// box, ...) by hovering over it -- highlighted live, using whichever object
// this platform's accessibility backend (AT-SPI/Accessibility API) actually
// resolves at the cursor -- and clicking it, instead of drawing a rectangle
// the way RegionSelectorOverlay does (SPEC.md 追加実装依頼「名前付き
// オブジェクト」). A click that doesn't land on any resolvable object is
// simply ignored (the overlay stays open) rather than treated as
// cancelling, since missing a small/oddly-shaped target is easy to do.
//
// Usage: call ObjectPickerOverlay::run(targetPid, outInfo). It blocks (via a
// local event loop) until the user clicks a resolvable object or cancels
// (Esc), then returns true/the picked object's info, or false if cancelled.
class ObjectPickerOverlay : public QWidget
{
    Q_OBJECT

public:
    // Returns true and fills `outInfo` if the user clicked an object that
    // could be resolved; returns false (outInfo left unset) if cancelled.
    static bool run(qint64 targetPid, PlatformAutomation::AccessibleObjectInfo &outInfo);

signals:
    void finishedPicking();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    explicit ObjectPickerOverlay(qint64 targetPid, QWidget *parent = nullptr);
    void finish(bool accepted, const PlatformAutomation::AccessibleObjectInfo &info);

    qint64 m_targetPid;
    bool m_finished = false;
    bool m_accepted = false;
    PlatformAutomation::AccessibleObjectInfo m_pickedInfo;
    // Re-resolved on every mouseMoveEvent -- cheap enough for interactive
    // use (a single accessibleObjectAtPoint() call per mouse-move tick, not
    // a full tree search; see its own doc comment), and lets paintEvent()
    // draw a live highlight around whatever is currently under the cursor.
    PlatformAutomation::AccessibleObjectInfo m_hoverInfo;
    // Screenshot of the virtual desktop taken right before this overlay is
    // shown, painted as its own background instead of relying on
    // Qt::WA_TranslucentBackground -- see RegionSelectorOverlay's identical
    // field and OverlayGeometry::grabVirtualDesktopSnapshot() for why.
    QPixmap m_backgroundSnapshot;
};
