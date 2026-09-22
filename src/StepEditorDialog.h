#pragma once

#include <QDialog>

#include "TestConfig.h"

class QRadioButton;
class QComboBox;

// Small modal dialog for picking which region a step operates in -- either
// the live target-window bounds, or one of the named operation regions
// already defined in the "①対象選択" column (see NamedRegionEditorDialog).
// Everything else about a step (which action kinds are enabled, their
// weights, the action count, and the detailed ActionParams) is edited
// directly in the "③操作パラメータ" column once the step is selected in
// "②ステップ構成" -- see SPEC.md section 6.2/6.3/6.9.
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
    StepEditorDialog(const RegionStep &initial, const QList<NamedRegion> &availableRegions,
                      QWidget *parent = nullptr, bool allowPopupDialogTarget = false);

    bool useWholeWindow() const;
    QString regionName() const;
    // True if the third, popup-dialog-targeting option was picked (only
    // ever possible when constructed with allowPopupDialogTarget=true).
    bool targetsPopupDialog() const;

private slots:
    void onModeChanged();

private:
    QRadioButton *m_wholeWindowRadio = nullptr;
    QRadioButton *m_namedRegionRadio = nullptr;
    QComboBox *m_namedRegionCombo = nullptr;
    QRadioButton *m_popupDialogRadio = nullptr;  // null unless allowPopupDialogTarget was true
};
