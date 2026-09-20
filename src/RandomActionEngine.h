#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QPixmap>
#include <QRandomGenerator>
#include <QString>
#include <QTimer>

#include "TestConfig.h"

// Drives the actual endurance test. TestConfig::steps is an ordered list of
// (region, enabled-action-kinds, action-count) steps; this engine walks
// through them in order, performing that many random actions in each
// step's region before advancing, and loops back to the first step once
// the last one is done -- continuing until a global stop condition (time
// limit, iteration limit, manual stop, or target crash) is hit. Runs on
// the GUI thread via a single-shot QTimer that reschedules itself with a
// new random delay after each action; individual native calls are short
// (a few ms of usleep at most) so this does not perceptibly freeze the UI.
class RandomActionEngine : public QObject
{
    Q_OBJECT

public:
    // Post-run report (SPEC.md 10): built up over the course of a run and
    // finalized in doStop(), then handed to the UI via summaryReady() so it
    // can be shown/saved without the UI having to track per-kind counters
    // itself. actionKindCounts keys are the same Japanese labels
    // MainWindow::describeStep() already uses for each kind, so the report
    // reads consistently with the rest of the app.
    struct RunSummary
    {
        QString stopReason;
        bool anomaly = false;
        quint32 rngSeedUsed = 0;
        qint64 totalIterations = 0;
        qint64 sequenceLoopsCompleted = 0;
        qint64 elapsedMs = 0;
        QMap<QString, qint64> actionKindCounts;  // insertion order doesn't matter; QMap sorts by key
        // Set only when anomaly is true: the "yyyyMMdd_HHmmss" timestamp
        // this run's anomaly screenshots/recording were saved under, inside
        // anomalyArtifactsDirectory() (SPEC.md 6.7/10). MainWindow reuses
        // this exact string when it saves its own additional artifacts (the
        // test config as a JSON preset, the operation-region screenshot) so
        // everything from one incident groups together under one prefix.
        QString anomalyArtifactTimestamp;
        // True only when this run stopped specifically because
        // PlatformAutomation::isProcessRunning() found the target process
        // gone (as opposed to the target window merely being lost, a
        // safety-stop, or a hang) -- MainWindow uses this to pop up a
        // dedicated crash-notification dialog (SPEC.md 6.7), separate from
        // its usual anomaly-artifact auto-save which applies to every kind
        // of anomaly.
        bool targetCrashed = false;
    };

    // Where captureAnomalyArtifacts() saves anomaly screenshots/recording
    // frames, and where MainWindow saves its own anomaly-triggered
    // artifacts (SPEC.md 6.7/10) -- a single shared location so a bug
    // report is just "everything with this timestamp prefix in this
    // folder".
    static QString anomalyArtifactsDirectory();

    // Human-readable multi-line report of a RunSummary, in the same style
    // as the rest of the app's log messages -- used both to append to the
    // log when a run finishes and as the ".txt" option when saving it
    // (SPEC.md 10).
    static QString formatSummaryText(const RunSummary &summary);
    // Machine-readable equivalent, for the ".json" save option.
    static QJsonObject summaryToJson(const RunSummary &summary);

    explicit RandomActionEngine(QObject *parent = nullptr);

    void start(const TestConfig &config);
    void stop();
    void pause();
    void resume();
    bool isRunning() const { return m_running; }
    bool isPaused() const { return m_paused; }

    // The most recently captured operation-region screenshot (SPEC.md
    // 6.2/10), if any -- persists across runs until a new capture overwrites
    // it (start() does not clear it), so MainWindow's save button can stay
    // enabled between runs the same way its run-summary save button does.
    bool hasRegionScreenshot() const { return m_hasCapturedScreenshot; }
    QPixmap lastRegionScreenshot() const { return m_lastCapturedScreenshot; }
    // Which step/member the returned screenshot was captured for, e.g.
    // "ステップ2" or "ステップ1（グループ内メンバー2）" -- used to build a
    // sensible file name when saving it.
    QString lastRegionScreenshotLabel() const { return m_lastCapturedScreenshotLabel; }
    // The operation region's own name (the NamedRegion's TestConfig::name,
    // or "対象GUIの全領域" for a whole-window step) -- distinct from the
    // step label above, and also drawn directly on the screenshot next to
    // each include rectangle (see renderRegionScreenshot()) so a developer
    // looking at the saved PNG can immediately tell which named region each
    // box is, not just which step captured it.
    QString lastRegionScreenshotRegionName() const { return m_lastCapturedScreenshotRegionName; }

signals:
    void actionPerformed(const QString &description);
    void logMessage(const QString &message);
    void iterationCountChanged(qint64 count);
    // Emitted when execution moves to a different step of TestConfig::steps
    // (at start, and each time the engine advances past the last step in
    // the sequence back to index 0) -- lets the UI show which step is
    // currently running (SPEC.md 6.9).
    void currentStepChanged(int index);
    void finished(const QString &reason);
    void pausedChanged(bool paused);
    // Emitted periodically (every few seconds) with the target process's
    // current resident memory (MB) and CPU usage (% of one core, averaged
    // since the previous sample) -- see SPEC.md 6.7/8.1.
    void resourceUsageUpdated(double residentMemoryMB, double cpuPercent);
    // Emitted right before finished(), with the completed run's summary
    // (SPEC.md 10) -- always paired one-to-one with a finished() emission
    // for the same run, emitted immediately before it.
    void summaryReady(const RandomActionEngine::RunSummary &summary);
    // Emitted whenever a new operation-region screenshot has just been
    // captured and is available via lastRegionScreenshot() (SPEC.md 6.2/10).
    void regionScreenshotCaptured();

private slots:
    void performRandomAction();
    void sampleResourceUsage();
    void checkTargetResponsiveness();
    void captureRecordingFrame();

private:
    enum class ActionKind {
        Click,
        DoubleClick,
        Drag,
        Key,
        ScrollUp,
        ScrollDown,
        ScrollHorizontal,
        Shortcut,
        WindowOp
    };

    // Result of runOneAction() below.
    enum class ActionOutcome {
        Performed,      // outDesc/outKind filled in; caller counts/logs/checks its threshold
        SkippedNoCount, // nothing dispatched (e.g. no valid point/candidates this tick); caller just reschedules
        StoppedEngine,  // doStop() was already called; caller must return immediately
    };

    const ActionParams &effectiveParams(const RegionStep &step) const;
    // Resolves `step`'s region, picks a weighted-random enabled action kind
    // for it, runs the usual pre-dispatch safety checks, and dispatches
    // exactly one action. Shared between a normal top-level step
    // (performRandomAction) and a single pick from within a step group's
    // members (performGroupAction) -- `stepLabel` (e.g. "ステップ3" or
    // "ステップ3（グループ内メンバー2）") is used in any stop/log message
    // this produces so it reads correctly either way.
    ActionOutcome runOneAction(const RegionStep &step, const QString &stepLabel, QString &outDesc,
                                ActionKind &outKind);
    // Weighted-random pick of one index into group.groupMembers, by each
    // member's own groupWeight (<= 0 treated as 1, same convention as
    // pickWeightedActionKind). group.groupMembers must not be empty.
    int pickWeightedGroupMemberIndex(const RegionStep &group);
    // Handles a tick where the current top-level step is a group (SPEC.md
    // 6.2): picks one member at random and performs exactly one action
    // from it, advancing to the next top-level step once
    // group.groupTotalCallCount actions have been performed in total
    // across the whole group.
    void performGroupAction(const RegionStep &group);
    // Resolves a step's region: either the live target-window bounds, or
    // the NamedRegion it references by name (see TestConfig::namedRegions).
    // Returns false if that can't be done right now (window gone, or the
    // referenced named region no longer exists / is empty).
    bool resolveStepRegion(const RegionStep &step, QList<QRect> &outIncludeRegions,
                            QList<QRect> &outExcludeRegions);
    // Captures a fresh operation-region screenshot (see renderRegionScreenshot()
    // below) and overwrites m_lastCapturedScreenshot with it, but only when
    // m_config.screenshotCaptureMode's condition is actually met right now
    // for the step/member currently being acted on (`stepLabel`,
    // `includeRegions`/`excludeRegions` already resolved by the caller, e.g.
    // runOneAction()) -- called on every action attempt, cheap to skip when
    // the mode's condition isn't met. Emits regionScreenshotCaptured() on
    // an actual capture.
    void maybeCaptureRegionScreenshot(const QString &stepLabel, const QString &regionName,
                                       const QList<QRect> &includeRegions,
                                       const QList<QRect> &excludeRegions);
    // Grabs a screenshot of just the target window's current bounds and
    // draws `includeRegions` (solid green) / `excludeRegions` (dashed red)
    // on top of it in window-local coordinates, so the saved image shows
    // exactly where on the target app the operation region actually is.
    // `regionName` (the NamedRegion's own name, or "対象GUIの全領域" for a
    // whole-window step) is drawn as a small label at each include
    // rectangle's top-left corner, so the region a box represents is
    // legible directly from the image, not just from the file name.
    // Returns a null QPixmap if the target window's bounds or the screen it
    // is on can't currently be determined.
    QPixmap renderRegionScreenshot(const QString &regionName, const QList<QRect> &includeRegions,
                                    const QList<QRect> &excludeRegions) const;
    // Just the grab, with no region overlay drawn on top -- shared by
    // renderRegionScreenshot() above and captureRecordingFrame() below, the
    // latter of which wants a faithful, unannotated view of the target
    // window as it actually appeared at that moment. Same null-on-failure
    // contract as renderRegionScreenshot().
    QPixmap grabTargetWindowScreenshot() const;
    QPoint pickRandomPoint(const QList<QRect> &includeRegions, const QList<QRect> &excludeRegions,
                           bool &ok);
    bool pointExcluded(const QPoint &pt, const QList<QRect> &excludeRegions) const;
    ActionKind pickWeightedActionKind(const RegionStep &step);
    // Called right after a right-button mouse action (Click or Drag) that
    // may have opened a native context/popup menu: resolves it (selects a
    // configured item, or dismisses it with Escape) so it can never be
    // left open for subsequent random actions to land on. Appends a " →
    // ..." description of the selection (if any) to `desc`. Returns true
    // if it called doStop() (target lost focus mid-check) -- the caller
    // must return immediately without dispatching/logging anything further.
    bool handlePossibleContextMenu(const ActionParams &params, QString &desc);
    void scheduleNext();
    // `targetCrashed` is a narrower flag than `isAnomaly`: true only for the
    // specific "target process is gone" stop, so RunSummary::targetCrashed
    // can drive MainWindow's crash dialog without it having to pattern-match
    // the (possibly-translated, see I18n.h) `reason` text.
    void doStop(const QString &reason, bool isAnomaly = false, bool targetCrashed = false);
    void advanceToNextStep();
    // Saves whatever anomaly diagnostics are available under a single
    // shared "yyyyMMdd_HHmmss" timestamp (SPEC.md 6.7/10): a full-screen
    // screenshot per connected screen (as before), the buffered recording
    // frames if m_config.enableScreenRecording collected any, and -- if
    // m_config.enableCrashDumpCollection -- a best-effort search for a
    // native OS crash report referencing the target process. Returns the
    // timestamp used, so doStop() can hand it to RunSummary for MainWindow
    // to reuse for its own additional artifacts.
    QString captureAnomalyArtifacts(const QString &reason);
    // Writes m_recordingFrames out as frame_0001.png, frame_0002.png, ...
    // into anomalyArtifactsDirectory()/recording_<timestamp>/, oldest
    // first, then clears the buffer. No-op if it's empty.
    void saveRecordingFrames(const QString &timestamp);
    // Checks for a top-level window belonging to the target process other
    // than the one originally selected (config.targetWindowId) -- e.g. a
    // confirmation dialog the target itself popped up, unrelated to the
    // right-click context-menu handling above. Returns true if the caller
    // should skip this tick's action entirely (either because it just
    // tried to dismiss one, or because it gave up and stopped the run).
    bool handleUnexpectedWindows();

    TestConfig m_config;
    QTimer m_timer;
    QTimer m_resourceTimer;
    // Periodic target-responsiveness (hang) check -- see
    // checkTargetResponsiveness() and PlatformAutomation::
    // checkWindowResponsive() (SPEC.md 8/10).
    QTimer m_hangCheckTimer;
    int m_consecutiveUnresponsive = 0;
    // A window can *declare* _NET_WM_PING support (per its WM_PROTOCOLS
    // property) without a reply ever actually arriving -- observed in
    // practice with at least one Qt/xcb build, where TestTarget itself
    // advertises the protocol but never answers a ping sent by a process
    // other than the real window manager. Since PlatformAutomation has no
    // way to tell "broken/unimplemented" apart from "genuinely hung" up
    // front, checkTargetResponsiveness() requires seeing at least one
    // successful reply before it will ever treat a later miss as a real
    // hang -- otherwise a target that simply never replies would trip a
    // false "hang detected" stop on every single run (SPEC.md 8/10).
    bool m_everRespondedToPing = false;
    int m_neverRespondedStrikes = 0;
    QElapsedTimer m_elapsed;
    qint64 m_pausedElapsedMs = 0;  // wall-clock time already consumed before the current pause/run segment
    qint64 m_iterationCount = 0;
    int m_currentStepIndex = 0;
    qint64 m_currentStepActionsDone = 0;
    qint64 m_sequenceLoopCount = 0;
    bool m_running = false;
    bool m_paused = false;

    // Previous resource-usage sample, to compute CPU% as a delta over time.
    double m_lastCpuTimeSeconds = 0.0;
    qint64 m_lastCpuSampleMs = 0;
    bool m_hasCpuSample = false;

    // Consecutive ticks in a row an unexpected extra window has been seen
    // for the target process despite attempts to dismiss it (Escape) --
    // see handleUnexpectedWindows(). Reset to 0 whenever only the expected
    // window is present.
    int m_unexpectedWindowStrikes = 0;

    // Accumulated over the run, for RunSummary (SPEC.md 10): how many times
    // each action kind actually fired, and the seed the run started with
    // (start() may have replaced a 0/"random" config value with a freshly
    // generated one -- the summary should report what was actually used).
    QMap<ActionKind, qint64> m_actionKindCounts;
    quint32 m_rngSeedUsed = 0;
    QString describeActionKind(ActionKind kind) const;

    QRandomGenerator m_rng;

    // Operation-region screenshot capture (SPEC.md 6.2/10). The captured
    // image/label/hasCapturedScreenshot below persist across runs (start()
    // does not clear them) so a previously captured image stays available
    // to save even before/between runs -- only m_capturedThisRun and
    // m_lastScreenshotStepIndex, which gate *when* the next capture should
    // happen, are reset per run.
    bool m_capturedThisRun = false;         // OnceAtStart mode: capture at most once per run
    int m_lastScreenshotStepIndex = -1;     // PerStepChange mode: capture on each step-index change
    // FixedInterval mode: the m_iterationCount value last captured at, so a
    // stretch of SkippedNoCount ticks (which don't advance m_iterationCount)
    // doesn't re-trigger a capture on every one of those ticks.
    qint64 m_lastScreenshotIterationCount = -1;
    QPixmap m_lastCapturedScreenshot;
    QString m_lastCapturedScreenshotLabel;
    QString m_lastCapturedScreenshotRegionName;
    bool m_hasCapturedScreenshot = false;

    // Rolling screen-recording buffer (SPEC.md 6.7/10, opt-in via
    // TestConfig::enableScreenRecording): captureRecordingFrame() grabs an
    // unannotated snapshot of the target window every
    // kRecordingFrameIntervalMs while m_recordingTimer runs, keeping only
    // the most recent kMaxRecordingFrames (older ones are dropped), so this
    // never grows past a bounded size regardless of run length. On an
    // anomaly stop the whole buffer is written out as numbered frames and
    // cleared (see saveRecordingFrames()); it is also cleared, and the
    // timer (re)started only if enabled, at the top of every start().
    QTimer m_recordingTimer;
    QList<QPixmap> m_recordingFrames;
};
