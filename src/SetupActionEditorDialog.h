#pragma once

#include <QDialog>
#include <QPoint>

#include "TestConfig.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class PointHighlightOverlay;

// Modal dialog for creating/editing one SetupAction ("起動時セットアップ",
// SPEC.md 6.x): a type (Click/DoubleClick/RightClick/Drag/TypeText/KeyPress/
// Wait) plus that type's own fields. Point fields are picked by clicking on
// screen via PointPickerOverlay rather than typing pixel coordinates by
// hand -- mirrors NamedRegionEditorDialog's on-screen-picking UX, simplified
// to single points instead of dragged rectangles.
class SetupActionEditorDialog : public QDialog
{
    Q_OBJECT

public:
    // `targetTopLeft`/`hasTarget`: the currently-selected target window's
    // top-left corner at the moment this dialog was opened (SetupAction
    // points are always stored window-relative -- see TestConfig.h -- so a
    // picked absolute screen point is converted using this). If no target
    // is currently selectable (hasTarget == false), point picking is
    // disabled and a warning is shown instead.
    explicit SetupActionEditorDialog(const SetupAction &initial, const QPoint &targetTopLeft,
                                      bool hasTarget, QWidget *parent = nullptr);

    SetupAction result() const;

private slots:
    void onTypeChanged(int index);
    void onPickPoint();
    void onPickDragFromPoint();
    void onPickDragToPoint();
    void onAccept();

private:
    // Shared by onPickPoint()/onPickDragFromPoint() (both write `m_point` --
    // Drag's "from" point is the same field a plain Click/DoubleClick/
    // RightClick uses) and onPickDragToPoint() (`m_dragToPoint`). Runs
    // PointPickerOverlay, converts the picked absolute point to
    // window-relative, and marks `pickedFlag` true so onAccept() can tell a
    // deliberately-picked point apart from an untouched default QPoint(0,0)
    // (see m_pointPicked's comment).
    void pickPointInto(QPoint &target, bool &pickedFlag);
    void refreshPointLabels();
    // Shows the currently-relevant picked point(s) (none for TypeText/
    // KeyPress/Wait, one for Click/DoubleClick/RightClick, up to two for
    // Drag) as on-screen markers via m_highlightOverlay, for as long as
    // this dialog stays open (SPEC.md 6.13's追加実装及び修正依頼 -- mirrors
    // NamedRegionEditorDialog::updateHighlight()). Called after every point
    // pick and every type-combo change.
    void updateHighlight();
    QWidget *buildPointPage();
    QWidget *buildDragPage();
    QWidget *buildTypeTextPage();
    QWidget *buildKeyPressPage();
    QWidget *buildWaitPage();

    QComboBox *m_typeCombo = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLineEdit *m_labelEdit = nullptr;

    QPushButton *m_pickPointButton = nullptr;
    QLabel *m_pointValueLabel = nullptr;

    QPushButton *m_pickDragFromButton = nullptr;
    QLabel *m_dragFromValueLabel = nullptr;
    QPushButton *m_pickDragToButton = nullptr;
    QLabel *m_dragToValueLabel = nullptr;

    QLineEdit *m_typeTextEdit = nullptr;
    QLineEdit *m_keySequenceEdit = nullptr;
    QSpinBox *m_waitMsSpin = nullptr;

    QPoint m_point;
    QPoint m_dragToPoint;
    // True once the user has actually pressed "位置を選択.../終了位置を選択..." in
    // this dialog session (or the action being edited already had a
    // non-origin point, see the constructor) -- lets onAccept() reject a
    // Click/DoubleClick/RightClick/Drag action whose point was left at the
    // default QPoint(0,0) (which typically lands on the target window's
    // frame/decoration, not any real content) instead of silently accepting
    // it, which previously made a forgotten point pick fail every run at
    // runtime with a confusing safety-stop (see SPEC.md 6.13).
    bool m_pointPicked = false;
    bool m_dragToPicked = false;
    QPoint m_targetTopLeft;
    bool m_hasTarget = false;
    PointHighlightOverlay *m_highlightOverlay = nullptr;
};
