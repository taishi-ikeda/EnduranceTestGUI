#pragma once

#include <QWidget>

#include "TestConfig.h"

class QCheckBox;
class QSpinBox;

// Editor widget for "which action kinds are enabled, their relative
// weight, and the action count" -- the RegionStep fields other than region
// (useWholeWindow/regionName) and ActionParams source
// (useDefaultActionParams/customActionParams), which this widget leaves
// untouched. Reused for both a selected step's own kinds (edited in
// "③操作パラメータ", see SPEC.md 6.2/6.4) and the shared preset used to seed
// newly created steps (edited via the "デフォルト" dialog,
// DefaultActionParamsDialog).
class ActionKindEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ActionKindEditor(QWidget *parent = nullptr);

    // Reads enable*/*/actionCount fields from step; ignores its region and
    // ActionParams-source fields.
    void setKinds(const RegionStep &step);
    // Writes enable*/*/actionCount fields into step; leaves its region and
    // ActionParams-source fields untouched.
    void applyKindsTo(RegionStep &step) const;

    // Hides the "操作回数" row entirely -- used when this editor is
    // editing a step that is a member of a step group (SPEC.md 6.2),
    // where the *group's* total call count governs how many actions
    // happen overall, not this member's own actionCount (which
    // applyKindsTo() still writes, but StepGroupEditorDialog simply
    // doesn't read it back). Visible by default.
    void setActionCountRowVisible(bool visible);

signals:
    // Emitted whenever any checkbox/spinbox changes, including during
    // setKinds() as it programmatically updates widgets -- callers that
    // write straight back into a step's fields on every emission still end
    // up with the correct final values, since setKinds() always finishes
    // by leaving every widget at its target value regardless of how many
    // intermediate signals fired along the way.
    void changed();

private:
    QCheckBox *m_clickCheck = nullptr;
    QCheckBox *m_leftClickCheck = nullptr;
    QCheckBox *m_rightClickCheck = nullptr;
    QSpinBox *m_clickWeightSpin = nullptr;
    QCheckBox *m_doubleClickCheck = nullptr;
    QSpinBox *m_doubleClickWeightSpin = nullptr;
    QCheckBox *m_dragCheck = nullptr;
    QSpinBox *m_dragWeightSpin = nullptr;
    QCheckBox *m_keyCheck = nullptr;
    QSpinBox *m_keyWeightSpin = nullptr;
    QCheckBox *m_scrollUpCheck = nullptr;
    QSpinBox *m_scrollUpWeightSpin = nullptr;
    QCheckBox *m_scrollDownCheck = nullptr;
    QSpinBox *m_scrollDownWeightSpin = nullptr;
    QCheckBox *m_scrollHorizontalCheck = nullptr;
    QSpinBox *m_scrollHorizontalWeightSpin = nullptr;
    QCheckBox *m_shortcutCheck = nullptr;
    QSpinBox *m_shortcutWeightSpin = nullptr;
    QCheckBox *m_windowOpCheck = nullptr;
    QSpinBox *m_windowOpWeightSpin = nullptr;
    QSpinBox *m_actionCountSpin = nullptr;
    QWidget *m_countRowWidget = nullptr;  // wraps the "操作回数" row so it can be hidden as a unit
};
