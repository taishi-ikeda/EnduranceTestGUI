#pragma once

#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QMainWindow>
#include <QMap>
#include <QPointer>

#include "RandomActionEngine.h"
#include "TestConfig.h"
#include "TestStatistics.h"
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
class RecordingIndicatorPanel;
class ActionParamsEditor;
class ActionKindEditor;
class GlobalHotkey;
class InputRecorder;

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

    // SPEC.md 10追加実装及び修正依頼「GUIを立ち上げなくてもターミナル実行で
    // 保存した手順を引数として与えることでテストを実行できるようにして
    // ください。その際連続実行の回数も引数として設定できるように」: entry
    // point for main.cpp's `--run <preset.json> [--repeat N]` CLI mode.
    // Loads the preset, sets the batch run count, and starts the exact same
    // kill-then-relaunch cycle "⟳ 連続実行" (onContinuousRun()) uses --
    // this window is never shown() in this mode (see main.cpp). Prints
    // progress to stdout/stderr via appendLog()'s m_headlessMode mirror
    // (there is no visible log pane to look at); the whole process exits
    // once every run has completed (checkHeadlessCompletion(), reached via
    // the same paths a normal batch run already uses to notice it's done or
    // aborted). Returns false (without starting the Qt event loop -- the
    // caller should exit(1) itself) if the preset can't be loaded, or if no
    // launch command is configured (required so each repeat gets a fresh
    // instance -- same requirement "⟳ 連続実行" already has).
    bool runHeadless(const QString &presetPath, int repeatCount);

private slots:
    void onRefreshTargets();
    void refreshPermissionLabel();

    void onAddNamedRegion();
    void onEditSelectedNamedRegion();
    void onRemoveSelectedNamedRegion();

    void onAddSetupAction();
    void onEditSelectedSetupAction();
    void onRemoveSelectedSetupAction();
    void onMoveSetupActionUp();
    void onMoveSetupActionDown();
    void onClearSetupActions();

    // SPEC.md 6.13追加実装及び修正依頼: records the user's own mouse/
    // keyboard operations (system-wide, including the target app's own
    // dialogs) into m_setupActions until Escape is pressed. See
    // InputRecorder for the actual observation mechanism.
    void onRecordSetupActions();
    void onSetupActionRecorded(SetupActionType type, QPoint point, QPoint dragToPoint, QString text,
                                QString keySequence, int scrollDx, int scrollDy,
                                ContextMenuSelectionMode menuSelectionMode, QString menuItemName,
                                int menuItemIndex);
    void onRecordingFinished(bool escapePressed);

    // SPEC.md 6.13追加実装及び修正依頼: runs just m_setupActions against the
    // selected target and stops (does not fall through into ②の
    // ステップ構成), so a user can verify a newly-built startup setup
    // actually works before configuring/running a full test. Deliberately
    // bypasses buildConfigFromUi() (which requires at least one step) --
    // see buildSetupOnlyConfigFromUi().
    void onTestSetupActions();

    void onAddStep();
    void onAddWaitStep();
    void onEditSelectedStep();
    void onRemoveSelectedStep();
    void onMoveStepUp();
    void onMoveStepDown();
    void onClearSteps();
    void onGroupSelectedSteps();
    void onUngroupSelectedStep();
    void onTaskifySelectedSteps();
    void onUntaskifySelectedStep();
    void onStepSelectionChanged();
    void onStepParamsModeChanged();
    void onEditDefaultParams();

    void onStart();
    void onContinuousRun();
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
    void onShowStatistics();
    void onBatchWaitTick();
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
    void refreshSetupActionList();
    QString describeSetupAction(const SetupAction &action, int index) const;
    // Live bounds top-left of whatever target is currently selected in
    // m_targetCombo, if any -- used as the anchor point for a named
    // region's "follow target window" option (SPEC.md 6.3/10). Returns
    // false (outTopLeft left unset) if no target is currently selectable.
    bool currentTargetTopLeft(QPoint &outTopLeft) const;
    // Live bounds of whatever target is currently selected in m_targetCombo,
    // if any -- used by the saved window size/position feature below
    // (SPEC.md 10 追加実装及び修正依頼). Returns false (outBounds left
    // unset) if no target is currently selectable.
    bool currentTargetBounds(QRect &outBounds) const;
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
    // Enables m_groupStepsButton/m_ungroupStepButton/m_taskifyStepsButton/
    // m_untaskifyStepButton based on the current ②list selection (2+ plain
    // steps -> グループ化/タスク化; exactly one group/task -> グループ解除/
    // タスク解除) and whether the steps panel is enabled at all (i.e. not
    // mid-run) -- called both when the selection changes and whenever
    // setControlsEnabled() toggles run state.
    void updateGroupButtonsEnabled();
    TestConfig buildConfigFromUi(bool &ok, QString &errorMessage) const;
    // Lighter counterpart to buildConfigFromUi() for onTestSetupActions():
    // only needs a selected target and a non-empty m_setupActions (②の
    // ステップ構成 is deliberately left empty in the returned config, since
    // this run is only ever meant to execute the setup phase and then stop
    // -- see RandomActionEngine::startSetupOnly()).
    TestConfig buildSetupOnlyConfigFromUi(bool &ok, QString &errorMessage) const;
    void appendLog(const QString &message);
    QString describeStep(const RegionStep &step, int index) const;
    // The current ①②③ setup (named regions, steps, default action params/
    // kinds, timing & limits) as the same JSON shape a preset file is saved
    // in (SPEC.md 10) -- shared by onSavePreset() and the anomaly auto-save
    // in onRunSummaryReady(), so a bug report's config file is produced the
    // same way a manually-saved preset is.
    QJsonObject buildPresetJson() const;
    // The actual "given a path, load and apply it" logic behind onLoadPreset()
    // (which wraps this with a QFileDialog + an overwrite-confirmation
    // prompt) -- factored out so runHeadless() can load a preset given
    // directly on the command line without going through either. Returns
    // false (errorMessage filled in) on a missing/unreadable/malformed
    // file; never shows any UI itself.
    bool loadPresetFromPath(const QString &path, QString &errorMessage);

    // Recomputes m_currentConfigFingerprint from the current ①②③ setup and
    // refreshes m_statisticsSummaryLabel and every ②list row's crash badge
    // from TestStatistics' history for that fingerprint (SPEC.md 10 ③④).
    // Called after loading a preset, right before a run starts, and after
    // every run finishes -- so the figures shown are never more than one
    // run stale regardless of whether runs are started by hand (including
    // after a fully manual target-app relaunch) or by the batch loop.
    void updateStatisticsDisplay();

    // Tries to select, in the just-(re)populated m_targetCombo/m_windows, a
    // window whose appName matches m_lastTargetAppName -- so that after a
    // crash (or any other reason the target list gets rebuilt), the app
    // keeps pointing at "the same app under test" once the user relaunches
    // it, without them having to re-pick it from ① by hand (SPEC.md 6.1):
    // the steps/regions/timing config never needed re-entering to begin
    // with (they aren't tied to a pid), so this closes the one piece that
    // did. Returns true if a match was found and selected.
    bool tryReselectLastTarget();

    // The actual "start the engine" logic (SPEC.md 10 ①), shared by the
    // ▶開始 button (onStart(), interactive == true: config/permission/
    // safety-check failures pop up a QMessageBox) and the batch loop's
    // automatic continuations (interactive == false: the same failures are
    // just logged and the batch is cancelled, since nobody may be watching
    // to dismiss a dialog). Returns true if the run was actually started.
    bool beginRun(bool interactive);
    // Called from onEngineFinished(): if a batch (SPEC.md 10 ①) or a
    // 連続実行 (SPEC.md 10 ⑤, m_continuousRunMode) is still in progress,
    // advances the counter and either starts the next run immediately
    // (target still alive -- 通常のバッチのみ; 連続実行は常にキル→再起動
    // を経由する) or hands off to waitForTargetThenContinueBatch(); does
    // nothing otherwise.
    void continueBatchIfNeeded();
    // SPEC.md 10追加実装及び修正依頼 (CLI/ヘッドレス実行モード): called from
    // every place m_batchModeActive can transition from true to false
    // (continueBatchIfNeeded()'s "all repeats done" branch, and beginRun()'s
    // three non-interactive failure paths) -- a no-op unless m_headlessMode
    // is set and the batch has genuinely ended, in which case it exits the
    // whole process (QCoreApplication::exit()) instead of just re-enabling
    // ①②③ the way the interactive GUI does, since there's no window for a
    // human to look at or press "▶開始" again from. Exit code reflects
    // whether any run in this headless invocation was anomalous
    // (m_headlessAnomalyOccurred), for use in a shell script/CI pipeline.
    void checkHeadlessCompletion();
    // Refreshes ①'s target list, then either:
    // - m_continuousRunMode (SPEC.md 10 ⑤): hands off to
    //   killTargetThenRelaunchForContinuousRun(), which always terminates
    //   any still-running target before relaunching it fresh, regardless
    //   of whether it was already gone.
    // - otherwise (plain batch, SPEC.md 10 ①): starts the next run right
    //   away if the target is already back, or launches it automatically
    //   (if ①'s "自動起動コマンド" is set -- SPEC.md 10 ②) and/or starts
    //   m_batchWaitTimer polling for its manual relaunch, so batch mode
    //   works the same way whether or not auto-restart is configured.
    void waitForTargetThenContinueBatch();
    // SPEC.md 10 ⑤ (連続実行): terminates the target app if it's still
    // running (via PlatformAutomation::terminateProcess(), using whichever
    // pid ①'s list currently shows for it, falling back to
    // m_lastRunTargetPid if the list doesn't have it anymore), then starts
    // m_batchWaitTimer to poll for it to actually exit before relaunching
    // it -- see onBatchWaitTick()'s m_continuousRunMode branch, which
    // drives the exit-then-appear two-phase wait via m_continuousWaitPhase.
    void killTargetThenRelaunchForContinuousRun();
    // Runs ①'s configured "自動起動コマンド" (SPEC.md 10 ②) via
    // QProcess::startDetached, splitting it into program + arguments with
    // QProcess::splitCommand() (portable across Qt5/Qt6, unlike the
    // single-QString startDetached() overload Qt6 removed). No-op if the
    // field is empty.
    void launchTargetAppFromConfiguredCommand();
    // SPEC.md 10追加実装及び修正依頼「連続実行ボタンを押してツールがすぐに
    // 立ち上がらないとエラーができます...起動まで一定時間待つパラメータを
    // 設定できるようにしてください」: called once the target window has
    // just been detected (①'s launch-detection polling in onBatchWaitTick(),
    // covering all three of its call sites -- 連続実行's kill+relaunch
    // cycle, plain batch mode's auto/manual relaunch wait, and ▶開始's
    // "起動してから開始する" option) instead of calling beginRun()
    // directly. If m_launchWaitSecondsSpin is 0 (the default -- unchanged
    // behavior), starts immediately; otherwise waits that many more seconds
    // via m_launchWaitTimer first, giving a target app that takes a while
    // to finish drawing its own UI time to become genuinely interactable
    // before beginRun()'s safety self-test runs against it.
    void startRunAfterLaunchWait(bool interactive);
    void onLaunchWaitElapsed();
    // SPEC.md 10追加実装及び修正依頼 (ウィンドウサイズの保存/復元): captures/
    // applies m_savedWindowSize against whichever target ① has selected.
    void onSaveWindowSize();
    void onRestoreWindowSize();
    void refreshSavedWindowSizeLabel();
    // SPEC.md 10追加実装及び修正依頼 (ウィンドウ位置の保存/復元): captures/
    // applies m_savedWindowPos against whichever target ① has selected.
    void onSaveWindowPos();
    void onRestoreWindowPos();
    void refreshSavedWindowPosLabel();

    // Target
    QComboBox *m_targetCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QLabel *m_permissionLabel = nullptr;
    QPushButton *m_openSettingsButton = nullptr;
    QList<WindowInfo> m_windows;
    // The appName of whichever window ① last had selected (updated on every
    // onRefreshTargets() call, and set directly from a loaded preset's
    // targetAppNameHint) -- see tryReselectLastTarget().
    QString m_lastTargetAppName;

    // SPEC.md 10 ②: an optional shell command line (program + arguments,
    // split with QProcess::splitCommand()) that (re)launches the target
    // app. Used by waitForTargetThenContinueBatch() when a batch run (①)
    // finds the target gone, and directly by m_launchTargetNowButton for a
    // manual one-off relaunch outside of batch mode too. Persisted in
    // preset JSON (buildPresetJson()/onLoadPreset()) as "targetLaunchCommand".
    // Left empty, batch mode still works -- it just waits for the user to
    // relaunch the target by hand instead (SPEC.md 10 ①).
    QLineEdit *m_targetLaunchCommandEdit = nullptr;
    QPushButton *m_browseLaunchCommandButton = nullptr;
    QPushButton *m_launchTargetNowButton = nullptr;
    // SPEC.md 10 ⑤: if checked, ▶開始 (onStart()) launches the target app
    // via the "自動起動コマンド" above and waits for it to appear *before*
    // running ②起動時セットアップ→③ステップ構成 -- instead of assuming the
    // currently-selected ①の対象 is already the one to operate. Requires
    // m_targetLaunchCommandEdit to be non-empty (checked in onStart()).
    // Independent of m_continuousRunMode below (連続実行 always launches
    // regardless of this checkbox's state).
    QCheckBox *m_launchBeforeStartCheck = nullptr;
    // SPEC.md 10追加実装及び修正依頼: extra seconds to wait, after ①'s
    // launch-detection polling first finds the target window, before
    // actually calling beginRun() -- see startRunAfterLaunchWait(). 0 (the
    // default) preserves the previous "start the instant it's detected"
    // behavior. Persisted in preset JSON as timing.launchWaitSeconds.
    QSpinBox *m_launchWaitSecondsSpin = nullptr;

    // SPEC.md 10追加実装及び修正依頼「テスト対象ツールの全体のウィンドウサイズを
    // 保存する機能と、保存したウィンドウサイズに合わせてテスト対象ツールの
    // ウィンドウサイズを変更する機能をつけてください」: a single captured
    // size (not a list -- one saved value is all the request asks for),
    // captured on demand from whichever target ① currently has selected and
    // re-appliable to it (or, after a relaunch, to whatever window is
    // selected at the time) via PlatformAutomation::resizeWindow(). Persisted
    // in preset JSON as hasSavedWindowSize/savedWindowWidth/savedWindowHeight.
    bool m_hasSavedWindowSize = false;
    QSize m_savedWindowSize;
    QLabel *m_savedWindowSizeLabel = nullptr;
    QPushButton *m_saveWindowSizeButton = nullptr;
    QPushButton *m_restoreWindowSizeButton = nullptr;

    // SPEC.md 10追加実装及び修正依頼「テスト対象ツールの現在のウインドウ位置を
    // 保存する機能と、保存したウィンドウ位置に合わせてテスト対象ツールの
    // ウィンドウ位置を変更する機能をつけてください」: same single-value
    // capture/restore design as the window size feature above, using
    // PlatformAutomation::moveWindow(). Persisted in preset JSON as
    // hasSavedWindowPos/savedWindowPosX/savedWindowPosY.
    bool m_hasSavedWindowPos = false;
    QPoint m_savedWindowPos;
    QLabel *m_savedWindowPosLabel = nullptr;
    QPushButton *m_saveWindowPosButton = nullptr;
    QPushButton *m_restoreWindowPosButton = nullptr;

    // Named operation regions (pool, referenced by name from steps)
    QListWidget *m_namedRegionListWidget = nullptr;
    QPushButton *m_addNamedRegionButton = nullptr;
    QPushButton *m_editNamedRegionButton = nullptr;
    QPushButton *m_removeNamedRegionButton = nullptr;
    QList<NamedRegion> m_namedRegions;

    // Startup setup macro (SPEC.md 6.x "起動時セットアップ"): a fixed,
    // deterministic sequence run once, in order, right after the target is
    // confirmed alive and before ②'s randomized steps begin -- e.g. the
    // login/navigation/configuration some target apps require right after
    // launch. Points are picked on screen via PointPickerOverlay
    // (SetupActionEditorDialog), stored window-relative like NamedRegion
    // (see TestConfig.h's SetupAction comment).
    QGroupBox *m_setupActionsGroup = nullptr;
    QListWidget *m_setupActionListWidget = nullptr;
    QPushButton *m_addSetupActionButton = nullptr;
    QPushButton *m_editSetupActionButton = nullptr;
    QPushButton *m_removeSetupActionButton = nullptr;
    QPushButton *m_moveSetupActionUpButton = nullptr;
    QPushButton *m_moveSetupActionDownButton = nullptr;
    // SPEC.md 6.13追加実装及び修正依頼「起動時セットアップに全消去のボタンを
    // 追加してください」: mirrors ②ステップ構成の「すべて削除」
    // (m_clearStepsButton/onClearSteps()) exactly -- clears m_setupActions
    // immediately, no confirmation prompt (same as that button).
    QPushButton *m_clearSetupActionsButton = nullptr;
    // SPEC.md 6.13追加実装及び修正依頼「記録」ボタン: appends to
    // m_setupActions in real time while InputRecorder is observing (see
    // onRecordSetupActions()/onSetupActionRecorded()). m_inputRecorder is
    // owned (parented to this); m_recordingPanel is a QPointer since it's a
    // separate top-level widget the user (or Escape) can close independent
    // of MainWindow, the same pattern as m_stopPanel.
    QPushButton *m_recordSetupButton = nullptr;
    InputRecorder *m_inputRecorder = nullptr;
    QPointer<RecordingIndicatorPanel> m_recordingPanel;
    int m_recordedActionCount = 0;
    // SPEC.md 6.13追加実装及び修正依頼「起動時セットアップを実行する機能が
    // 欲しい」: runs just m_setupActions (via RandomActionEngine::
    // startSetupOnly()) so it can be verified independently of ②の
    // ステップ構成 -- see onTestSetupActions().
    QPushButton *m_testSetupButton = nullptr;
    // Set for the duration of a run started by onTestSetupActions(), so
    // onEngineFinished()/onRunSummaryReady() (shared with every other kind
    // of run) can skip the bookkeeping that only makes sense for a real
    // random-action test run (crash-rate statistics, anomaly preset/
    // screenshot auto-save) -- reset back to false once that run's
    // finished() fires.
    bool m_setupOnlyTestRun = false;
    // If checked, a batch (① 連続自動実行) only runs this setup macro before
    // its first run, skipping it on every automatic restart after that
    // (e.g. a one-time EULA/license dialog that only appears the very first
    // time the target app is launched) -- decided in beginRun(), since
    // TestConfig::setupActions itself always means "run these now" (see its
    // own comment in TestConfig.h).
    QCheckBox *m_setupActionsFirstRunOnlyCheck = nullptr;
    QList<SetupAction> m_setupActions;

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
    // Combines the currently multi-selected steps into a single task step
    // (SPEC.md 6.2追加実装及び修正依頼): enabled only when 2+ plain steps
    // (no wait steps, no groups/tasks -- no nesting of any container kind)
    // are selected. Unlike grouping, a task runs every member exactly
    // once, in the order shown, every time its turn comes up (see
    // RegionStep::isTask) -- "タスク解除" is the reverse: enabled only
    // when exactly one task is selected, and replaces it with its member
    // steps as standalone top-level steps, same as "グループ解除".
    QPushButton *m_taskifyStepsButton = nullptr;
    QPushButton *m_untaskifyStepButton = nullptr;
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
    QCheckBox *m_autoSlowdownCheck = nullptr;

    // Groups (disabled while running)
    QGroupBox *m_targetGroup = nullptr;
    QGroupBox *m_namedRegionGroup = nullptr;
    QGroupBox *m_stepsGroup = nullptr;
    QGroupBox *m_actionParamsGroup = nullptr;
    QGroupBox *m_timingGroup = nullptr;

    // Controls
    QPushButton *m_startButton = nullptr;
    // SPEC.md 10 ⑤: independent of ▶開始/連続実行回数 above -- pressing
    // this always terminates the target app first (if it's still running)
    // and launches a fresh instance, runs ②起動時セットアップ→③ステップ構成
    // once against it, then repeats (kill leftover -> relaunch -> run)
    // m_batchRunCountSpin's configured number of times. See onContinuousRun()/
    // killTargetThenRelaunchForContinuousRun()/m_continuousRunMode.
    QPushButton *m_continuousRunButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_pauseResumeButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_elapsedLabel = nullptr;
    QLabel *m_iterationLabel = nullptr;
    QLabel *m_resourceUsageLabel = nullptr;

    // SPEC.md 10 ①: consecutive automatic runs, for reproducing a crash
    // that only happens some fraction of the time. 1 (the default) means no
    // batching -- ▶開始 behaves exactly as before. > 1 makes onStart() set
    // m_batchModeActive and, from then on, onEngineFinished() ->
    // continueBatchIfNeeded() keeps starting the next run automatically
    // until m_batchRunsRequested is reached or the user presses ■停止.
    QSpinBox *m_batchRunCountSpin = nullptr;
    QLabel *m_batchProgressLabel = nullptr;
    int m_batchRunsRequested = 1;
    int m_batchRunsCompleted = 0;
    bool m_batchModeActive = false;
    // SPEC.md 10追加実装及び修正依頼 (CLI/ヘッドレス実行モード): set once by
    // runHeadless(), never cleared -- see its own header comment and
    // checkHeadlessCompletion(). m_headlessAnomalyOccurred is OR'd in by
    // onRunSummaryReady() from every run's RunSummary::anomaly across the
    // whole invocation, and decides the process's final exit code.
    bool m_headlessMode = false;
    bool m_headlessAnomalyOccurred = false;
    // Polls (via onBatchWaitTick()) for the target app to reappear between
    // batch runs when it isn't already back the instant one finishes --
    // whether the user relaunches it by hand or m_targetLaunchCommandEdit's
    // command was used to relaunch it automatically (SPEC.md 10 ①②).
    // Also reused by 連続実行 (m_continuousRunMode) and by ▶開始's
    // "対象ツールの起動から開始する" option (m_launchBeforeStartPending) --
    // see onBatchWaitTick() for how it dispatches between the three.
    QTimer *m_batchWaitTimer = nullptr;
    // SPEC.md 10追加実装及び修正依頼: single-shot delay armed by
    // startRunAfterLaunchWait() once the target has just been detected but
    // m_launchWaitSecondsSpin is > 0 -- separate from m_batchWaitTimer
    // (which is stopped by that point) so onStop() can distinguish and
    // cancel "waiting for the extra grace period" from every other wait
    // state. m_launchWaitInteractive remembers which beginRun() mode to
    // resume with once it fires (onLaunchWaitElapsed()).
    QTimer *m_launchWaitTimer = nullptr;
    bool m_launchWaitInteractive = false;
    // SPEC.md 10 ⑤ (連続実行, m_continuousRunButton): true while the
    // current batch loop is running in "always kill+relaunch" mode rather
    // than the plain passive-relaunch batch semantics above. Checked by
    // continueBatchIfNeeded()/waitForTargetThenContinueBatch()/
    // onBatchWaitTick() to branch into killTargetThenRelaunchForContinuousRun()
    // instead of the original wait-for-manual-or-auto-relaunch flow.
    bool m_continuousRunMode = false;
    // Which half of 連続実行's per-cycle "wait for the old instance to
    // exit, then wait for the new one to appear" sequence m_batchWaitTimer
    // is currently polling for -- only meaningful while m_continuousRunMode
    // is true and m_batchWaitTimer is active.
    enum class ContinuousWaitPhase { None, WaitingForExit, WaitingForAppear };
    ContinuousWaitPhase m_continuousWaitPhase = ContinuousWaitPhase::None;
    // True from onStart() (when its "対象ツールの起動から開始する" checkbox
    // is checked) until the launched target is detected and the run
    // actually begins -- tells onBatchWaitTick() that the run about to
    // start is a plain interactive ▶開始 (not a 連続実行/batch
    // continuation), so it should call beginRun(interactive == true).
    bool m_launchBeforeStartPending = false;
    // The targetPid a run was last actually started with (set in
    // beginRun(), alongside config.targetPid). Used by
    // killTargetThenRelaunchForContinuousRun() as a fallback identifier for
    // the process to terminate when ①'s live window list no longer carries
    // it (e.g. it already crashed and disappeared from the list, but the
    // process is somehow still alive).
    qint64 m_lastRunTargetPid = -1;

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

    // Cross-run crash statistics (SPEC.md 10 ③④, "確率的なクラッシュの解析").
    // Persists to disk (TestStatistics::historyFilePath()) so it survives
    // the app being closed/reopened -- every finished run is recorded here
    // regardless of whether it was started by hand or by the batch loop.
    TestStatistics m_stats;
    // Identifies "this exact ①②③ setup" (see TestStatistics::computeFingerprint);
    // kept up to date by updateStatisticsDisplay().
    QString m_currentConfigFingerprint;
    // stepIndex -> crash count for m_currentConfigFingerprint, and the total
    // run count they're a fraction of -- cached here (rather than re-queried
    // per paint) so describeStep() can append a "⚠ クラッシュ N/M回" badge
    // (SPEC.md 10 ④) cheaply for every row.
    QMap<int, int> m_stepCrashCounts;
    int m_statsTotalRuns = 0;
    QLabel *m_statisticsSummaryLabel = nullptr;
    QPushButton *m_showStatisticsButton = nullptr;

    // System-wide emergency-stop hotkey (Ctrl+Alt+Shift+Esc), a backstop
    // for the floating StopPanel button -- see GlobalHotkey.h and SPEC.md
    // 6.7/10. May be null-effective (registered but never fires) if the
    // combo couldn't be grabbed on this system; that's a soft failure, not
    // a fatal one.
    GlobalHotkey *m_globalHotkey = nullptr;
};
