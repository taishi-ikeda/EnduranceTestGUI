#pragma once

#include <QDialog>

#include "TestConfig.h"

class QRadioButton;
class QComboBox;
class ActionKindEditor;
class ActionParamsEditor;

// Small modal dialog for picking which region a step operates in -- either
// the live target-window bounds, or one of the named operation regions
// already defined in the "①対象選択" column (see NamedRegionEditorDialog).
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
// show it twice.
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
    // `defaultActionParams` is only read when `includeActionParams` is true
    // (to display what "デフォルトを使う" resolves to, and to seed the
    // custom-params editor the first time the user switches to "このステップ
    // 専用の設定を使う"); ignored otherwise.
    StepEditorDialog(const RegionStep &initial, const QList<NamedRegion> &availableRegions,
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

private slots:
    void onModeChanged();
    void onActionParamsModeChanged();

private:
    QRadioButton *m_wholeWindowRadio = nullptr;
    QRadioButton *m_namedRegionRadio = nullptr;
    QComboBox *m_namedRegionCombo = nullptr;
    QRadioButton *m_popupDialogRadio = nullptr;  // null unless allowPopupDialogTarget was true

    // Null unless constructed with includeActionParams=true.
    ActionKindEditor *m_kindEditor = nullptr;
    QRadioButton *m_useDefaultParamsRadio = nullptr;
    QRadioButton *m_useCustomParamsRadio = nullptr;
    ActionParamsEditor *m_paramsEditor = nullptr;
    ActionParams m_defaultActionParams;
    ActionParams m_initialCustomActionParams;
};
