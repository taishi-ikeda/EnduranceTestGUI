#pragma once

#include <QDialog>

#include "TestConfig.h"

class QListWidget;
class QPushButton;
class QSpinBox;
class QLabel;
class QRadioButton;
class ActionKindEditor;
class ActionParamsEditor;

// Modal dialog for editing a step group (SPEC.md 6.2): the group's total
// call count, and its member steps (each a full region + action-kind/
// weight + ActionParams configuration, plus a groupWeight controlling how
// often it gets picked relative to its siblings). Structurally a smaller
// clone of MainWindow's own ②ステップ構成/③操作パラメータ master-detail
// panels, scoped to one group's members -- reuses the same StepEditorDialog
// (region-only picker), ActionKindEditor, and ActionParamsEditor widgets so
// a group member is configured exactly like a top-level step, minus the
// action count (the group's own total call count governs that -- see
// ActionKindEditor::setActionCountRowVisible()) plus the added groupWeight.
class StepGroupEditorDialog : public QDialog
{
    Q_OBJECT

public:
    // `defaultActionParams`/`defaultActionKindsTemplate` are snapshots used
    // the same way MainWindow uses them for top-level steps: a member set
    // to "デフォルトを使う" displays/edits `defaultActionParams` live (it is
    // *not* re-fetched after this dialog closes -- MainWindow re-resolves
    // effective params from the live default at run time regardless of
    // what was shown here), and `defaultActionKindsTemplate` seeds a
    // newly-added member's kinds/weights (its actionCount is ignored).
    StepGroupEditorDialog(const RegionStep &initialGroup, const QList<NamedRegion> &namedRegions,
                           const ActionParams &defaultActionParams,
                           const RegionStep &defaultActionKindsTemplate, QWidget *parent = nullptr);

    // isGroup=true, groupMembers/groupTotalCallCount from this dialog's
    // edits; all other RegionStep fields default-constructed (unused for a
    // group container -- see TestConfig.h).
    RegionStep result() const;

private slots:
    void onAddMember();
    void onEditSelectedMember();
    void onRemoveSelectedMember();
    void onMoveMemberUp();
    void onMoveMemberDown();
    void onMemberSelectionChanged();
    void onAccept();

private:
    QString describeMember(const RegionStep &member, int index) const;
    void refreshMemberList();
    void flushMemberEditor();
    void loadMemberEditorForSelection();

    QList<NamedRegion> m_namedRegions;
    ActionParams m_defaultActionParams;
    RegionStep m_defaultActionKindsTemplate;
    QList<RegionStep> m_members;
    int m_lastEditedMemberRow = -1;

    QSpinBox *m_totalCallCountSpin = nullptr;
    QListWidget *m_memberListWidget = nullptr;
    QPushButton *m_addMemberButton = nullptr;
    QPushButton *m_editMemberButton = nullptr;
    QPushButton *m_removeMemberButton = nullptr;
    QPushButton *m_moveMemberUpButton = nullptr;
    QPushButton *m_moveMemberDownButton = nullptr;

    QLabel *m_detailContextLabel = nullptr;
    QSpinBox *m_memberWeightSpin = nullptr;
    ActionKindEditor *m_kindEditor = nullptr;
    QRadioButton *m_useDefaultParamsRadio = nullptr;
    QRadioButton *m_useCustomParamsRadio = nullptr;
    ActionParamsEditor *m_paramsEditor = nullptr;
};
