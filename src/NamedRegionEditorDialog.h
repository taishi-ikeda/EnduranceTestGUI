#pragma once

#include <QDialog>
#include <QRect>

#include "TestConfig.h"

class QLineEdit;
class QListWidget;
class QPushButton;
class QLabel;
class QCheckBox;
class QRadioButton;
class QSpinBox;
class QWidget;
class RegionHighlightOverlay;

// Modal dialog for creating/editing one NamedRegion: a name, one or more
// rectangles drawn via RegionSelectorOverlay, and optional mask/exclude
// sub-rectangles within them. Used from the "①対象選択" column's operation-
// region list (add/edit) -- see SPEC.md 6.3.
//
// While this dialog is open, the region being built/edited is shown on
// screen the whole time via RegionHighlightOverlay (updated after every
// add/remove of a rectangle), rather than MainWindow showing a highlight
// just from a list selection -- selecting an item in the list no longer
// shows anything on screen by itself.
class NamedRegionEditorDialog : public QDialog
{
    Q_OBJECT

public:
    // `targetTopLeft`/`hasTarget`: the currently-selected target window's
    // top-left corner at the moment this dialog was opened, as recorded by
    // MainWindow (which re-reads it live from the target combo box) --
    // used as the reference point if the user enables "対象ウィンドウの
    // 移動に追従させる" (SPEC.md 6.3/10). hasTarget is false when no
    // target window is currently selectable, in which case that option is
    // disabled. `targetPid`: the same target's pid, needed for the
    // accessibility-tree lookups behind "画面上の部品を指定" mode below
    // (SPEC.md 追加実装依頼「名前付きオブジェクト」) -- pass -1 if
    // `hasTarget` is false (that mode is then simply unusable, same as the
    // follow-target checkbox above).
    explicit NamedRegionEditorDialog(const NamedRegion &initial, const QPoint &targetTopLeft,
                                      bool hasTarget, qint64 targetPid, QWidget *parent = nullptr);

    NamedRegion result() const;

private slots:
    void onDrawRegions();
    void onRemoveSelectedRegion();
    void onDrawExcludeRegions();
    void onRemoveSelectedExcludeRegion();
    void onModeChanged();
    void onPickObject();
    void onAccept();

private:
    void refreshRegionList();
    void refreshExcludeList();
    void updateHighlight();
    void updateObjectInfoLabel();

    QLineEdit *m_nameEdit = nullptr;

    // SPEC.md 追加実装依頼「名前付きオブジェクト」: "矩形を描画" (existing,
    // rectangle-based) vs "画面上の部品を指定" (new, accessibility-tree-
    // based) -- mutually exclusive, toggled via onModeChanged(), which
    // simply shows/hides m_rectModeGroup vs m_objectModeGroup rather than
    // tearing anything down, so switching back and forth during one editing
    // session doesn't lose whichever side's state was entered.
    QRadioButton *m_rectModeRadio = nullptr;
    QRadioButton *m_objectModeRadio = nullptr;
    QWidget *m_rectModeGroup = nullptr;
    QWidget *m_objectModeGroup = nullptr;

    QListWidget *m_regionListWidget = nullptr;
    QPushButton *m_drawButton = nullptr;
    QPushButton *m_removeRegionButton = nullptr;
    QListWidget *m_excludeListWidget = nullptr;
    QPushButton *m_drawExcludeButton = nullptr;
    QPushButton *m_removeExcludeButton = nullptr;
    QCheckBox *m_followTargetCheck = nullptr;

    QPushButton *m_pickObjectButton = nullptr;
    QLabel *m_objectInfoLabel = nullptr;
    QCheckBox *m_useDefaultActionCheck = nullptr;
    QSpinBox *m_reresolveIntervalSpin = nullptr;

    QList<QRect> m_regions;
    QList<QRect> m_excludeRegions;
    QPoint m_targetTopLeft;
    bool m_hasTarget = false;
    qint64 m_targetPid = -1;
    // If editing a region that already had followsTargetWindow set, its
    // original anchor is preserved unless the user's current target
    // selection replaces it (see result()).
    QPoint m_existingAnchorTopLeft;
    bool m_hadExistingAnchor = false;
    // Whether any of m_regions/m_excludeRegions were actually touched
    // (drawn/removed) during this dialog session -- see result()'s use of
    // this alongside m_hadExistingAnchor.
    bool m_regionsChanged = false;
    RegionHighlightOverlay *m_highlightOverlay = nullptr;

    // Set by onPickObject() (or seeded from `initial` when editing an
    // already-object-target region); result() copies this into the
    // returned NamedRegion::objectTarget verbatim except for
    // useDefaultAction/reresolveEveryActions, which always come fresh from
    // their own widgets below instead (so toggling them doesn't require
    // re-picking the object).
    ObjectTarget m_objectTarget;
    bool m_objectPicked = false;
    // The exact bounds onPickObject() resolved the object at, shown via
    // m_highlightOverlay as a one-time (not live-tracking) confirmation --
    // see updateHighlight().
    QRect m_lastPickedBounds;
};
