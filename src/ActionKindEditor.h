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
};
