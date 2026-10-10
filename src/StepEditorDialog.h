#pragma once

#include <QDialog>

#include "RegionEditContext.h"
#include "TestConfig.h"

class QRadioButton;
class QComboBox;
class QGroupBox;
class QPushButton;
class ActionKindEditor;
class ActionParamsEditor;

// Small modal dialog for picking which region a step operates in -- either
// the live target-window bounds, or one of the named operation regions in
// the shared pool (SPEC.md 6.3) -- and for authoring that pool directly:
// "新規作成.../編集.../削除" open NamedRegionEditorDialog and mutate
// `*regionCtx->namedRegions` in place (see RegionEditContext's own comment),
// so a region created while editing one step is immediately selectable from
// any other step's dialog.
//
// For a top-level step in "②ステップ構成" (includeActionParams=true, the
// only caller that passes it), this dialog *also* covers everything that
// used to be edited live in the now-removed "③操作パラメータ" column while
// that step was selected: which action kinds are enabled, their weights,
// the action count, and the detailed ActionParams (SPEC.md追加実装及び
// 修正依頼 -- moved here so it's reachable from ②'s own "編集..."/double-
// click, instead of needing a separate always-visible column). A step-group
// or task member is still region-only (includeActionParams left false, the
// default): StepGroupEditorDialog/TaskEditorDialog already have their own
// equivalent kind/params editor bound to their own member list, mirroring
// how this column used to work, and embedding another copy here would just
// show it twice. The region-authoring buttons above, however, are shown
// regardless of includeActionParams, so a group/task member can create or
// reuse a region exactly like a top-level step can.
class StepEditorDialog : public QDialog
{
    Q_OBJECT

public:
    // `allowPopupDialogTarget` adds a third choice, "operate on a newly
    // appeared dialog" (RegionStep::targetsPopupDialog) -- only passed true
    // by TaskEditorDialog, since that option only makes sense for a task
    // member (see the field's doc comment in TestConfig.h). Left false (the
    // default) for every other caller (a top-level step, or a step-group
    // member) so the option doesn't show there at all.
    //
    // `regionCtx` must outlive this dialog's exec() call (callers hold it as
    // a local/member RegionEditContext and pass its address -- see
    // RegionEditContext.h).
    //
    // `defaultActionParams` is only read when `includeActionParams` is true
    // (to display what "デフォルトを使う" resolves to, and to seed the
    // custom-params editor the first time the user switches to "このステップ
    // 専用の設定を使う"); ignored otherwise.
    StepEditorDialog(const RegionStep &initial, RegionEditContext *regionCtx,
                      QWidget *parent = nullptr, bool allowPopupDialogTarget = false,
                      bool includeActionParams = false,
                      const ActionParams &defaultActionParams = ActionParams());

    bool useWholeWindow() const;
    QString regionName() const;
    // True if the third, popup-dialog-targeting option was picked (only
    // ever possible when constructed with allowPopupDialogTarget=true).
    bool targetsPopupDialog() const;

    // The remaining accessors are only meaningful when constructed with
    // includeActionParams=true -- meaningless (default-constructed/false)
    // otherwise, since nothing here was ever shown to edit them.
    void applyActionKindsTo(RegionStep &step) const;
    bool useDefaultActionParams() const;
    ActionParams customActionParams() const;

    // Writes sweepRegionUseRandomPoint into step (unchanged/left at its
    // prior value if the selected region isn't a sweep-type NamedRegion, in
    // which case the choice isn't offered at all -- see TestConfig.h);
    // leaves every other field untouched. Only meaningful when constructed
    // with includeActionParams=true.
    void applySweepModeTo(RegionStep &step) const;

private slots:
    void onModeChanged();
    void onActionParamsModeChanged();
    void onRegionSelectionChanged();
    void onCreateRegion();
    void onEditRegion();
    void onDeleteRegion();

private:
    // Repopulates m_namedRegionCombo from *m_regionCtx->namedRegions,
    // preserving the current selection by name where possible (or selecting
    // `selectIndex` if >= 0, e.g. right after a newly created region is
    // appended at the end of the pool). Also refreshes every enabled state
    // that depends on whether the pool is currently empty.
    void refreshRegionCombo(int selectIndex = -1);
    void updateRegionButtonsEnabled();

    QRadioButton *m_wholeWindowRadio = nullptr;
    QRadioButton *m_namedRegionRadio = nullptr;
    QComboBox *m_namedRegionCombo = nullptr;
    QPushButton *m_createRegionButton = nullptr;
    QPushButton *m_editRegionButton = nullptr;
    QPushButton *m_deleteRegionButton = nullptr;
    QRadioButton *m_popupDialogRadio = nullptr;  // null unless allowPopupDialogTarget was true

    // Null unless constructed with includeActionParams=true.
    ActionKindEditor *m_kindEditor = nullptr;
    QRadioButton *m_useDefaultParamsRadio = nullptr;
    QRadioButton *m_useCustomParamsRadio = nullptr;
    ActionParamsEditor *m_paramsEditor = nullptr;
    ActionParams m_defaultActionParams;
    ActionParams m_initialCustomActionParams;

    // The region-type-driven ランダム/点列スイープ choice (TestConfig.h's
    // RegionStep::sweepRegionUseRandomPoint) -- null unless constructed
    // with includeActionParams=true. The containing group box is shown
    // only while the currently selected named region is itself a
    // 点列(スイープ) region (see onRegionSelectionChanged()); for an
    // ordinary rectangle region or useWholeWindow, it stays hidden and the
    // step always gets a plain random point within the region, same as
    // before this choice existed.
    QGroupBox *m_sweepModeGroup = nullptr;
    QRadioButton *m_sweepRandomRadio = nullptr;
    QRadioButton *m_sweepSequenceRadio = nullptr;

    RegionEditContext *m_regionCtx = nullptr;
    bool m_initialSweepRegionUseRandomPoint = false;
};
