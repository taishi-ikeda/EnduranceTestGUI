#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QRandomGenerator>
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

private slots:
    void performRandomAction();
    void sampleResourceUsage();

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

    const ActionParams &effectiveParams(const RegionStep &step) const;
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
    void scheduleNext();
    void doStop(const QString &reason, bool isAnomaly = false);
    void advanceToNextStep();
    void captureAnomalyScreenshots(const QString &reason);

    TestConfig m_config;
    QTimer m_timer;
    QTimer m_resourceTimer;
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

    QRandomGenerator m_rng;
};
