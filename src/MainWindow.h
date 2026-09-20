#pragma once

#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
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
class QAction;
class StopPanel;
class ActionParamsEditor;
class ActionKindEditor;
class GlobalHotkey;

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

    void onAddStep();
    void onAddWaitStep();
    void onEditSelectedStep();
    void onRemoveSelectedStep();
    void onMoveStepUp();
    void onMoveStepDown();
    void onClearSteps();
    void onGroupSelectedSteps();
    void onUngroupSelectedStep();
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
    void onRunSummaryReady(const RandomActionEngine::RunSummary &summary);
    void onSaveSummary();
    void onRegionScreenshotCaptured();
    void onSaveRegionScreenshot();
    void onOpenAccessibilitySettings();
    void updateElapsedLabel();
    void onAboutApp();
    void onAboutQt();
    void onSavePreset();
    void onLoadPreset();
    void onGlobalEmergencyStop();
    void onSelectLanguageJapanese();
    void onSelectLanguageEnglish();

private:
    void buildUi();
    void buildMenuBar();
    QWidget *buildTargetColumn(QWidget *parent);
    QWidget *buildStepsColumn(QWidget *parent);
    QWidget *buildActionParamsColumn(QWidget *parent);

    void refreshStepList();
    void refreshNamedRegionList();
    // Live bounds top-left of whatever target is currently selected in
    // m_targetCombo, if any -- used as the anchor point for a named
    // region's "follow target window" option (SPEC.md 6.3/10). Returns
    // false (outTopLeft left unset) if no target is currently selectable.
    bool currentTargetTopLeft(QPoint &outTopLeft) const;
    QString describeNamedRegion(const NamedRegion &region) const;
    // "領域1", "領域2", ... -- the first of these not already used by an
    // existing named region, so a new region always starts with a usable
    // name and the user isn't required to type one (SPEC.md 6.3).
    QString generateDefaultRegionName() const;
    // Names of steps (1-based, human-facing) that reference this named
    // region; used to block deleting/renaming a region still in use.
    // Recurses into group members (a step inside a group can reference a
    // named region just like a top-level one).
    QStringList stepsReferencing(const QString &regionName) const;
    // Renames `regionName` to `newName` in every step that references it,
    // top-level or inside a group (SPEC.md 6.2/6.3) -- called when a named
    // region is renamed so existing references keep pointing at it.
    void renameRegionReferences(QList<RegionStep> &steps, const QString &oldName, const QString &newName) const;
    // True if every enabled action kind on `step` (or, when it's a group,
    // on every one of its members) has the configuration it needs to
    // actually run (e.g. enableKey needs a non-empty character set) --
    // used by buildConfigFromUi() to validate top-level steps and group
    // members alike. On failure, fills in `errorMessage` (referencing
    // `stepLabel`) and returns false.
    bool validateStepActionConfig(const RegionStep &step, const QString &stepLabel,
                                   QString &errorMessage) const;
    void flushActionParamsEditor();
    void loadActionParamsEditorForSelection();
    void setControlsEnabled(bool enabled);
    // Enables m_groupStepsButton/m_ungroupStepButton based on the current
    // ②list selection (2+ plain steps -> グループ化; exactly one group ->
    // グループ解除) and whether the steps panel is enabled at all (i.e.
    // not mid-run) -- called both when the selection changes and whenever
    // setControlsEnabled() toggles run state.
    void updateGroupButtonsEnabled();
    TestConfig buildConfigFromUi(bool &ok, QString &errorMessage) const;
    void appendLog(const QString &message);
    QString describeStep(const RegionStep &step, int index) const;
    // The current ①②③ setup (named regions, steps, default action params/
    // kinds, timing & limits) as the same JSON shape a preset file is saved
    // in (SPEC.md 10) -- shared by onSavePreset() and the anomaly auto-save
    // in onRunSummaryReady(), so a bug report's config file is produced the
    // same way a manually-saved preset is.
    QJsonObject buildPresetJson() const;

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

    // Steps
    QListWidget *m_stepListWidget = nullptr;
    QPushButton *m_addStepButton = nullptr;
    QPushButton *m_addWaitStepButton = nullptr;
    QPushButton *m_editStepButton = nullptr;
    QPushButton *m_removeStepButton = nullptr;
    QPushButton *m_moveStepUpButton = nullptr;
    QPushButton *m_moveStepDownButton = nullptr;
    QPushButton *m_clearStepsButton = nullptr;
    // Combines the currently multi-selected steps into a single group step
    // (SPEC.md 6.2): enabled only when 2+ plain steps (no wait steps, no
    // groups -- groups cannot be nested) are selected. "グループ解除" is
    // the reverse: enabled only when exactly one group is selected, and
    // replaces it with its member steps as standalone top-level steps.
    QPushButton *m_groupStepsButton = nullptr;
    QPushButton *m_ungroupStepButton = nullptr;
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
    // Set around any sequence that mutates m_steps' indices (move/remove/
    // group/ungroup) and then re-populates/reselects m_stepListWidget:
    // QListWidget::clear() and setCurrentRow() both fire currentRowChanged
    // synchronously, which would otherwise re-enter onStepSelectionChanged()
    // mid-mutation and flush the (by-then-stale) editor contents into
    // whichever step has shifted into that index -- silently corrupting it
    // (or, once m_lastEditedStepRow is invalidated, into m_defaultActionParams
    // instead). While this is true, onStepSelectionChanged() does nothing;
    // the caller calls loadActionParamsEditorForSelection() itself once,
    // after the dust settles.
    bool m_suppressStepSelectionHandling = false;

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

    // When RandomActionEngine internally captures its operation-region
    // screenshot (region boundary drawn on top -- SPEC.md 6.2/10):
    // once right before the run's first action, every time the current
    // step changes, or every N actions (m_screenshotIntervalSpin, only
    // shown/used when the interval radio is selected).
    QRadioButton *m_screenshotModeOnceRadio = nullptr;
    QRadioButton *m_screenshotModePerStepRadio = nullptr;
    QRadioButton *m_screenshotModeIntervalRadio = nullptr;
    QSpinBox *m_screenshotIntervalSpin = nullptr;

    // Opt-in diagnostics for an abnormal stop (SPEC.md 6.7/10), chosen
    // before starting a run like the screenshot-timing radios above:
    // - 録画: RandomActionEngine keeps a rolling buffer of screenshots and
    //   writes them out as numbered frames on an anomaly stop (extra
    //   ongoing CPU/memory/disk cost while running, hence opt-in/default off).
    // - クラッシュダンプ: a one-shot best-effort filesystem search for a
    //   native OS crash report when a run stops abnormally (negligible
    //   cost, hence default on).
    QCheckBox *m_recordingCheck = nullptr;
    QCheckBox *m_crashDumpCollectionCheck = nullptr;

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
    QPushButton *m_saveSummaryButton = nullptr;
    RandomActionEngine::RunSummary m_lastSummary;
    bool m_hasLastSummary = false;
    // Saves m_engine's most recently captured operation-region screenshot
    // (SPEC.md 6.2/10) to a file the user picks a destination folder for.
    // Enabled once the engine has ever captured one (persists across runs,
    // like m_saveSummaryButton above).
    QPushButton *m_saveRegionScreenshotButton = nullptr;
    // Write-through copy of every log line for the run in progress, opened
    // fresh in onStart() and closed in onEngineFinished() (SPEC.md 6.8/10).
    // m_logView above is capped at 5000 blocks for display performance and
    // silently drops older lines once a long run exceeds that -- this file
    // is not, so "ログを保存..." after a very long run is never missing the
    // earlier part of what happened. Null/not open when no run has started
    // yet this session.
    QFile m_fullLogFile;

    RandomActionEngine *m_engine = nullptr;
    QPointer<StopPanel> m_stopPanel;
    QTimer *m_uiTimer = nullptr;
    QElapsedTimer m_runElapsed;

    // "ファイル" menu: save/load the whole editable test setup (named
    // regions, steps, default action params/kinds, timing & limits) as a
    // JSON preset file (SPEC.md 10) -- disabled while a run is in progress,
    // alongside the rest of the ①②③ panels (setControlsEnabled()).
    QAction *m_savePresetAction = nullptr;
    QAction *m_loadPresetAction = nullptr;
    QAction *m_languageJapaneseAction = nullptr;
    QAction *m_languageEnglishAction = nullptr;

    // System-wide emergency-stop hotkey (Ctrl+Alt+Shift+Esc), a backstop
    // for the floating StopPanel button -- see GlobalHotkey.h and SPEC.md
    // 6.7/10. May be null-effective (registered but never fires) if the
    // combo couldn't be grabbed on this system; that's a soft failure, not
    // a fatal one.
    GlobalHotkey *m_globalHotkey = nullptr;
};
