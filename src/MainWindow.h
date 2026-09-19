#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>

#include "RandomActionEngine.h"
#include "TestConfig.h"
#include "platform/PlatformAutomation.h"

class QComboBox;
class QLabel;
class QListWidget;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QStackedWidget;
class QLineEdit;
class QRadioButton;
class QPlainTextEdit;
class QPushButton;
class QGroupBox;
class QTimer;
class StopPanel;
class ActionParamsEditor;
class ActionKindEditor;
class RegionHighlightOverlay;

// Main window, laid out (per SPEC.md 6.9) as three columns:
//   1. 対象選択  -- target picker, named/reusable operation regions, timing
//      & limits.
//   2. ステップ構成 -- ordered step list (add/edit/remove/reorder). The
//      "追加"/"編集" buttons only pick which region (whole window, or one
//      of the named regions from column 1) the step operates in --
//      StepEditorDialog no longer edits anything else.
//   3. 操作パラメータ -- master/detail editor for whichever step is
//      selected in column 2: which action kinds are enabled for it, their
//      relative weights, and its action count (no "default" concept --
//      disabled while no step is selected), plus the detailed ActionParams,
//      either the shared defaults (edited when no step, or no step's own
//      settings, is selected) or that step's own custom ActionParams,
//      toggled as a whole per step.
// Start/Stop controls and the log span the full width below the columns.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onRefreshTargets();
    void refreshPermissionLabel();

    void onAddNamedRegion();
    void onEditSelectedNamedRegion();
    void onRemoveSelectedNamedRegion();
    void onNamedRegionSelectionChanged();

    void onAddStep();
    void onEditSelectedStep();
    void onRemoveSelectedStep();
    void onMoveStepUp();
    void onMoveStepDown();
    void onClearSteps();
    void onStepSelectionChanged();
    void onStepParamsModeChanged();
    void onEditDefaultParams();

    void onStart();
    void onStop();
    void onPauseResume();
    void onEngineFinished(const QString &reason);
    void onEnginePausedChanged(bool paused);
    void onActionLog(const QString &message);
    void onIterationCountChanged(qint64 count);
    void onCurrentStepChanged(int index);
    void onResourceUsageUpdated(double residentMemoryMB, double cpuPercent);
    void onClearLog();
    void onSaveLog();
    void onOpenAccessibilitySettings();
    void updateElapsedLabel();
    void onAboutApp();
    void onAboutQt();

private:
    void buildUi();
    void buildMenuBar();
    QWidget *buildTargetColumn(QWidget *parent);
    QWidget *buildStepsColumn(QWidget *parent);
    QWidget *buildActionParamsColumn(QWidget *parent);

    void refreshStepList();
    void refreshNamedRegionList();
    QString describeNamedRegion(const NamedRegion &region) const;
    // "領域1", "領域2", ... -- the first of these not already used by an
    // existing named region, so a new region always starts with a usable
    // name and the user isn't required to type one (SPEC.md 6.3).
    QString generateDefaultRegionName() const;
    // Names of steps (1-based, human-facing) that reference this named
    // region; used to block deleting/renaming a region still in use.
    QStringList stepsReferencing(const QString &regionName) const;
    // Shows/hides RegionHighlightOverlay to match whichever operation
    // region is currently selected in m_namedRegionListWidget (SPEC.md 6.3).
    void updateRegionHighlight();
    void flushActionParamsEditor();
    void loadActionParamsEditorForSelection();
    void setControlsEnabled(bool enabled);
    TestConfig buildConfigFromUi(bool &ok, QString &errorMessage) const;
    void appendLog(const QString &message);
    QString describeStep(const RegionStep &step, int index) const;

    // Target
    QComboBox *m_targetCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QLabel *m_permissionLabel = nullptr;
    QPushButton *m_openSettingsButton = nullptr;
    QList<WindowInfo> m_windows;

    // Named operation regions (pool, referenced by name from steps)
    QListWidget *m_namedRegionListWidget = nullptr;
    QPushButton *m_addNamedRegionButton = nullptr;
    QPushButton *m_editNamedRegionButton = nullptr;
    QPushButton *m_removeNamedRegionButton = nullptr;
    QList<NamedRegion> m_namedRegions;
    // Non-interactive on-screen highlight for whichever region is selected
    // above; created lazily, owned/destroyed as a child of MainWindow.
    RegionHighlightOverlay *m_regionHighlightOverlay = nullptr;

    // Steps
    QListWidget *m_stepListWidget = nullptr;
    QPushButton *m_addStepButton = nullptr;
    QPushButton *m_editStepButton = nullptr;
    QPushButton *m_removeStepButton = nullptr;
    QPushButton *m_moveStepUpButton = nullptr;
    QPushButton *m_moveStepDownButton = nullptr;
    QPushButton *m_clearStepsButton = nullptr;
    QList<RegionStep> m_steps;
    // Index into m_steps currently being executed by m_engine, or -1 while
    // not running; describeStep() marks this one so ②'s list shows
    // progress during a run (SPEC.md 6.9).
    int m_currentRunningStepIndex = -1;

    // Action parameters (master/detail: default, or selected step's custom)
    QLabel *m_actionParamsContextLabel = nullptr;
    QRadioButton *m_stepUseDefaultParamsRadio = nullptr;
    QRadioButton *m_stepUseCustomParamsRadio = nullptr;
    ActionParamsEditor *m_actionParamsEditor = nullptr;
    ActionParams m_defaultActionParams;
    int m_lastEditedStepRow = -1;  // row whose params/kinds the editors currently reflect, -1 = defaults

    // Step-level: which action kinds the selected step performs, their
    // relative weight, and its action count (SPEC.md 6.2/6.3). There is no
    // *live* shared default for these (unlike ActionParams above) that
    // existing steps can opt into -- so the group is disabled while no step
    // is selected in column 2. m_defaultActionKinds below is only a seed
    // template applied to newly created steps, not a dynamic reference.
    QGroupBox *m_stepKindGroup = nullptr;
    ActionKindEditor *m_stepKindEditor = nullptr;

    // Top-right of column ③: opens DefaultActionParamsDialog to edit
    // m_defaultActionParams (the live shared ActionParams default) and
    // m_defaultActionKinds (the kind/weight/count preset used to seed
    // newly created steps) together, without disturbing the current step
    // selection in column ②.
    QPushButton *m_editDefaultParamsButton = nullptr;
    RegionStep m_defaultActionKinds;

    // Timing & limits
    // Operation interval can be specified either directly in ms, or as a
    // rate (operations/sec, converted to an equivalent ms range when
    // building TestConfig -- see MainWindow::buildConfigFromUi). Both
    // widget sets stay populated at all times so switching modes back and
    // forth doesn't lose either one's values; m_intervalStack shows
    // whichever is currently selected.
    QRadioButton *m_intervalModeMsRadio = nullptr;
    QRadioButton *m_intervalModeRateRadio = nullptr;
    QStackedWidget *m_intervalStack = nullptr;
    QSpinBox *m_minIntervalSpin = nullptr;
    QSpinBox *m_maxIntervalSpin = nullptr;
    QDoubleSpinBox *m_minRateSpin = nullptr;
    QDoubleSpinBox *m_maxRateSpin = nullptr;
    QSpinBox *m_maxIterationsSpin = nullptr;
    QSpinBox *m_maxDurationSecSpin = nullptr;
    QSpinBox *m_maxSequenceLoopsSpin = nullptr;
    QCheckBox *m_keepActiveCheck = nullptr;
    // 0 = pick a fresh random seed each run (see TestConfig::rngSeed).
    QSpinBox *m_rngSeedSpin = nullptr;

    // Groups (disabled while running)
    QGroupBox *m_targetGroup = nullptr;
    QGroupBox *m_namedRegionGroup = nullptr;
    QGroupBox *m_stepsGroup = nullptr;
    QGroupBox *m_actionParamsGroup = nullptr;
    QGroupBox *m_timingGroup = nullptr;

    // Controls
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_pauseResumeButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_elapsedLabel = nullptr;
    QLabel *m_iterationLabel = nullptr;
    QLabel *m_resourceUsageLabel = nullptr;

    // Log
    QPlainTextEdit *m_logView = nullptr;

    RandomActionEngine *m_engine = nullptr;
    QPointer<StopPanel> m_stopPanel;
    QTimer *m_uiTimer = nullptr;
    QElapsedTimer m_runElapsed;
};
