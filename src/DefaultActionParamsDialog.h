#pragma once

#include <QDialog>

#include "TestConfig.h"

class ActionParamsEditor;
class ActionKindEditor;

// Modal dialog opened from the "デフォルト" button at the top of
// "③操作パラメータ" (see SPEC.md 6.4/6.9). Edits two different things
// together, since together they're "the default action":
//  - initialKinds (-> resultKinds()): which action kinds/weights/action
//    count a NEWLY CREATED step starts with (a one-time seed applied in
//    MainWindow::onAddStep, not a live reference -- editing this later has
//    no effect on steps already created).
//  - initialParams (-> resultParams()): TestConfig::defaultActionParams,
//    the live shared ActionParams used by any step whose
//    useDefaultActionParams is true.
// Wraps the same ActionKindEditor/ActionParamsEditor widgets used in-place
// in that column, so editing here behaves identically to editing them
// directly -- this dialog is just a shortcut that doesn't disturb the
// current step selection in "②ステップ構成".
class DefaultActionParamsDialog : public QDialog
{
    Q_OBJECT

public:
    DefaultActionParamsDialog(const RegionStep &initialKinds, const ActionParams &initialParams,
                               QWidget *parent = nullptr);

    RegionStep resultKinds() const;
    ActionParams resultParams() const;

private:
    ActionKindEditor *m_kindEditor = nullptr;
    ActionParamsEditor *m_paramsEditor = nullptr;
};
