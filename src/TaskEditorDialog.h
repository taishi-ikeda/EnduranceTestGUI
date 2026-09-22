#pragma once

#include <QDialog>

#include "TestConfig.h"

class QListWidget;
class QPushButton;
class QLabel;
class QRadioButton;
class ActionKindEditor;
class ActionParamsEditor;

// Modal dialog for editing a task (SPEC.md 6.2追加実装及び修正依頼): a
// fixed, ordered sequence of member steps (each a full region + action-kind
// configuration, same as a group member) that always run in full, in list
// order, every time the task's turn comes up. Structurally a near-clone of
// StepGroupEditorDialog, minus the two fields that only make sense for
// random selection (the group's total call count, and each member's own
// selection weight) -- a task has neither: running it always means running
// every member exactly once, in the order shown, and that order is exactly
// what "↑ 上へ"/"↓ 下へ" here control (unlike a group, where reordering
// members is purely cosmetic since they're picked at random).
class TaskEditorDialog : public QDialog
{
    Q_OBJECT

public:
    // Same parameter meaning as StepGroupEditorDialog's constructor.
    TaskEditorDialog(const RegionStep &initialTask, const QList<NamedRegion> &namedRegions,
                      const ActionParams &defaultActionParams,
                      const RegionStep &defaultActionKindsTemplate, QWidget *parent = nullptr);

    // isTask=true, taskMembers from this dialog's edits; all other
    // RegionStep fields default-constructed (unused for a task container).
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

    QListWidget *m_memberListWidget = nullptr;
    QPushButton *m_addMemberButton = nullptr;
    QPushButton *m_editMemberButton = nullptr;
    QPushButton *m_removeMemberButton = nullptr;
    QPushButton *m_moveMemberUpButton = nullptr;
    QPushButton *m_moveMemberDownButton = nullptr;

    QLabel *m_detailContextLabel = nullptr;
    ActionKindEditor *m_kindEditor = nullptr;
    QRadioButton *m_useDefaultParamsRadio = nullptr;
    QRadioButton *m_useCustomParamsRadio = nullptr;
    ActionParamsEditor *m_paramsEditor = nullptr;
};
