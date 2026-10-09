#pragma once

#include <QList>
#include <QObject>
#include <QString>

class QTimer;
class LoadInjector;

// One sampled data point for the "負荷モニター" feature (SPEC.md 追加実装依頼
// 「負荷モニター」). Timestamps are QDateTime::currentMSecsSinceEpoch() so a
// saved CSV's rows can be correlated against the run log's own timestamps.
struct LoadSample
{
    qint64 timestampMs = 0;
    double targetCpuPercent = 0.0;
    double targetMemoryMb = 0.0;
    // -1 means "not available on this platform" (macOS -- see
    // PlatformAutomation::querySystemCpuStats()), not "0% busy".
    double systemCpuPercent = -1.0;
    bool loadInjectionActive = false;
};

// Independent, self-contained CPU/memory load monitor for diagnosing load/
// timing-dependent ("ハイゼンバグ") crashes -- deliberately *not* part of
// RandomActionEngine, which already has its own, separate, fixed-5-second
// resourceUsageUpdated() sampling (see RandomActionEngine::sampleResourceUsage()).
// Kept fully separate so that leaving this feature off (its own enable
// checkbox in MainWindow, unchecked by default) has *zero* effect on
// anything else: no extra timer is even constructed, nothing is shared with
// RandomActionEngine's state, and the action-dispatch loop is completely
// unaware this class exists. See MainWindow's own comment on
// m_loadMonitorEnabledCheck for the UI side of this guarantee.
//
// Each tick (at the caller-specified interval -- user-configurable in
// MainWindow, not hardcoded): reads the target process' current CPU%/memory
// (via PlatformAutomation::queryProcessStats(), computing %CPU as a delta
// against the previous sample -- the same technique
// RandomActionEngine::sampleResourceUsage() uses, but with its own
// independent delta-tracking state here, never shared with the engine's),
// the whole-system CPU% (Linux only, via PlatformAutomation::
// querySystemCpuStats() -- omitted/-1 elsewhere), and whether
// m_loadInjector (if set) is currently running; appends the result to a
// capped ring buffer; and emits sampleAdded() so a chart widget knows to
// repaint.
class LoadMonitor : public QObject
{
    Q_OBJECT

public:
    explicit LoadMonitor(QObject *parent = nullptr);

    // `loadInjector` is purely queried (isRunning()) each tick, never
    // started/stopped/owned here -- LoadMonitor only ever *reports* whether
    // load injection happens to be active at each sample, it never controls
    // it. May be null (loadInjectionActive is then always false).
    void setLoadInjector(LoadInjector *loadInjector) { m_loadInjector = loadInjector; }

    // Starts (or restarts, if already running) sampling `targetPid` every
    // `intervalMs`. Resets the CPU-delta tracking state and clears the ring
    // buffer, so a fresh run's chart starts empty rather than carrying over
    // a previous run's/previous target's samples.
    void start(qint64 targetPid, int intervalMs);
    void stop();
    bool isRunning() const;

    const QList<LoadSample> &samples() const { return m_samples; }

    // Writes samples() out as a CSV file (timestamp as ISO 8601, target
    // CPU%, target memory MB, system CPU% [blank if unavailable], load
    // injection active [0/1]) -- used both by MainWindow's manual "負荷
    // モニターのグラフを保存..." button and automatically on an abnormal
    // stop (SPEC.md 追加実装依頼「負荷モニター」, mirroring the existing
    // screen-recording-on-crash design so a crash's CPU/memory context is
    // captured without anyone needing to be watching the live chart at the
    // exact moment it happened). Returns false on a filesystem error, or if
    // there are no samples to write.
    bool saveSamplesAsCsv(const QString &path) const;

private slots:
    void onTick();

signals:
    // Emitted after every new sample is appended, and once (with an empty
    // buffer) from start()/stop(), so a chart widget knows to repaint.
    void sampleAdded();

private:
    // Caps memory/CSV size regardless of how long a run lasts or how short
    // an interval the user picks; the oldest sample is dropped once this is
    // exceeded (a sliding window, not a hard stop).
    static constexpr int kMaxSamples = 2000;

    QTimer *m_timer = nullptr;
    qint64 m_targetPid = -1;
    QList<LoadSample> m_samples;
    LoadInjector *m_loadInjector = nullptr;  // non-owning, may be null

    bool m_hasCpuSample = false;
    double m_lastCpuTimeSeconds = 0.0;
    qint64 m_lastCpuSampleMs = 0;

    bool m_hasSystemCpuSample = false;
    double m_lastSystemTotalSeconds = 0.0;
    double m_lastSystemIdleSeconds = 0.0;
};
