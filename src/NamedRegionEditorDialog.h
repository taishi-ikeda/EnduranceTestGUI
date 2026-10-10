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
class PointHighlightOverlay;

// `nameTemplate` is a translatable string containing one "%1" placeholder
// (e.g. QStringLiteral("操作領域%1") or QStringLiteral("操作線分%1"));
// returns the first `nameTemplate.arg(n)` (n = 1, 2, ...) not already used
// by an existing region's `.name`. Shared between StepEditorDialog (which
// computes a brand-new region's initial default name, before this dialog
// even opens and before the user has picked a mode) and this dialog itself
// (which regenerates the name reactively if the user switches the mode
// radio between rectangle/object vs 点列（スイープ）, so sweep regions get
// their own "操作線分N" counter independent of "操作領域N" -- SPEC.md 追加
// 実装依頼「操作領域のデフォルト名を種別ごとに独立させる」).
QString generateDefaultRegionName(const QList<NamedRegion> &existing, const QString &nameTemplate);

// Modal dialog for creating/editing one NamedRegion: a name, one or more
// rectangles drawn via RegionSelectorOverlay, and optional mask/exclude
// sub-rectangles within them. Used from StepEditorDialog's "新規作成.../
// 編集..." buttons (region authoring is per-step now, not a standalone
// "①対象選択" panel -- see RegionEditContext) -- see SPEC.md 6.3.
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
    //
    // `existingRegionsForNaming`/`autoManageName`: when `autoManageName` is
    // true (StepEditorDialog::onCreateRegion() creating a brand-new region
    // -- never set for onEditRegion()'s already-named regions), this dialog
    // keeps the name field in sync with generateDefaultRegionName() as the
    // user switches modes, using `existingRegionsForNaming` (the shared
    // region pool, not yet including the one being created) to pick the
    // next free number -- but only as long as the field still holds exactly
    // the auto-generated value it last set; once the user types their own
    // name, mode switches no longer touch it.
    explicit NamedRegionEditorDialog(const NamedRegion &initial, const QPoint &targetTopLeft,
                                      bool hasTarget, qint64 targetPid, QWidget *parent = nullptr,
                                      const QList<NamedRegion> *existingRegionsForNaming = nullptr,
                                      bool autoManageName = false);

    NamedRegion result() const;

private slots:
    void onDrawRegions();
    void onRemoveSelectedRegion();
    void onDrawExcludeRegions();
    void onRemoveSelectedExcludeRegion();
    void onModeChanged();
    void onPickObject();
    void onAddSweepWaypoint();
    void onRemoveSelectedSweepWaypoint();
    void onAccept();

private:
    void refreshRegionList();
    void refreshExcludeList();
    void updateHighlight();
    void updateObjectInfoLabel();
    void refreshSweepWaypointList();
    void updateSweepHighlight();

    QLineEdit *m_nameEdit = nullptr;

    // SPEC.md 追加実装依頼「名前付きオブジェクト」/「操作領域を点列
    // （スイープ）で指定」: "矩形を描画" (existing, rectangle-based) vs
    // "画面上の部品を指定" (accessibility-tree-based) vs "点列（スイープ）
    // で指定" (a fixed start->end point sequence, same generation rule as
    // RegionStep's own sweep mode -- see NamedRegion::isSweepTarget) --
    // mutually exclusive, toggled via onModeChanged(), which simply shows/
    // hides m_rectModeGroup/m_objectModeGroup/m_sweepModeGroup rather than
    // tearing anything down, so switching back and forth during one editing
    // session doesn't lose whichever side's state was entered.
    QRadioButton *m_rectModeRadio = nullptr;
    QRadioButton *m_objectModeRadio = nullptr;
    QRadioButton *m_sweepModeRadio = nullptr;
    QWidget *m_rectModeGroup = nullptr;
    QWidget *m_objectModeGroup = nullptr;
    QWidget *m_sweepModeGroup = nullptr;

    // See the constructor's `existingRegionsForNaming`/`autoManageName` doc
    // comment above. m_lastAutoName tracks whatever name onModeChanged()
    // (or the constructor, for the mode seeded at startup) most recently
    // put in m_nameEdit itself, so a later mode switch can tell "still the
    // auto value, safe to replace" apart from "the user typed their own
    // name since, leave it alone".
    const QList<NamedRegion> *m_existingRegionsForNaming = nullptr;
    bool m_autoManageName = false;
    QString m_lastAutoName;

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

    // SPEC.md 追加実装依頼「操作領域を点列（スイープ）で指定」/「中点
    // 対応」: an ordered list of window-relative waypoints (first = start,
    // last = end, any number of entries in between = midpoints), picked one
    // at a time via PointPickerOverlay -- same list-based UI pattern as
    // m_regionListWidget/m_drawButton/m_removeRegionButton above, rather
    // than StepEditorDialog's fixed pair of "開始位置を選択.../終了位置を
    // 選択..." buttons, since the number of points here isn't fixed at two.
    QListWidget *m_sweepWaypointListWidget = nullptr;
    QPushButton *m_addSweepWaypointButton = nullptr;
    QPushButton *m_removeSweepWaypointButton = nullptr;
    QSpinBox *m_sweepIntervalSpin = nullptr;
    QSpinBox *m_sweepJitterSpin = nullptr;
    QList<QPoint> m_sweepWaypoints;
    // Separate from m_highlightOverlay (rectangle-mode only, started/
    // stopped by RegionHighlightOverlay) since a sweep has no rectangle to
    // show -- just the picked waypoints connected in order, via the same
    // point-highlight mechanism StepEditorDialog uses for its own sweep UI.
    PointHighlightOverlay *m_sweepHighlightOverlay = nullptr;
};
