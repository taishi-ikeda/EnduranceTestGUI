#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QMap>
#include <QObject>
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
    };

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

private slots:
    void performRandomAction();
    void sampleResourceUsage();
    void checkTargetResponsiveness();

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
    void doStop(const QString &reason, bool isAnomaly = false);
    void advanceToNextStep();
    void captureAnomalyScreenshots(const QString &reason);
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
};
