#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>

// Cross-run crash statistics (SPEC.md 6.7/10, "確率的なクラッシュの解析"):
// a single run's RandomActionEngine::RunSummary answers "what happened this
// time", but diagnosing a bug that only crashes the target app some
// fraction of the time needs "out of N attempts under the same test
// conditions, how many crashed, and at which step". Every run's outcome --
// whether started by hand via ▶開始 (including after the user manually
// relaunches a crashed target app themselves) or by the automatic batch
// loop (SPEC.md 10 ①) -- is appended to a single persistent JSON-Lines log
// on disk, keyed by a fingerprint of the test setup that produced it, so
// repeated manual runs of "the same test" accumulate into the same
// statistics without requiring batch mode or auto-restart to be used.
class TestStatistics
{
public:
    struct RunRecord
    {
        QDateTime finishedAt;
        QString configFingerprint;
        QString targetAppName;  // display-only, not part of the fingerprint
        bool crashed = false;
        bool anomaly = false;
        int crashStepIndex = -1;  // 0-based, -1 when crashed == false
        qint64 totalIterations = 0;
        qint64 elapsedMs = 0;
        quint32 rngSeedUsed = 0;
        QString stopReason;
    };

    struct StepCrashCount
    {
        int stepIndex = 0;  // 0-based
        int crashCount = 0;
    };

    struct Aggregate
    {
        int totalRuns = 0;
        int crashRuns = 0;
        double crashRate() const { return totalRuns > 0 ? double(crashRuns) / totalRuns : 0.0; }
        // Mean of totalIterations/elapsedMs across just the crashed runs --
        // "how long does it usually take to reproduce", 0 when crashRuns == 0.
        double meanIterationsToCrash = 0.0;
        double meanElapsedMsToCrash = 0.0;
        // stepIndex -> how many crashes happened while that step was
        // executing (SPEC.md 10 ④), sorted by stepIndex ascending.
        QList<StepCrashCount> crashesByStep;
    };

    // A stable identifier for "this exact test setup" (named regions,
    // steps, default action params/kinds, and the core timing & limits that
    // define what the test actually does). Deliberately excludes the RNG
    // seed (varies run to run even when the setup itself hasn't changed),
    // targetAppNameHint (tracked separately per-record for display), and
    // the opt-in diagnostics toggles (screenshot/recording/crash-dump
    // settings -- irrelevant to what the test does), so repeated manual
    // runs of "the same test" land in the same bucket even if the user
    // tweaks those between runs. `presetJson` is the same shape
    // MainWindow::buildPresetJson() produces.
    static QString computeFingerprint(const QJsonObject &presetJson);

    // Where the persistent run-history log lives (SPEC.md 10) -- alongside
    // the full per-run text logs, in the same Documents subfolder, so both
    // are easy to find together.
    static QString historyFilePath();

    // Loads every record from historyFilePath() into memory, replacing
    // whatever was cached before -- called once at startup, and there is no
    // need to call it again afterward since addRun() keeps the in-memory
    // cache and the on-disk log in sync itself.
    void reload();

    // Appends one run's outcome to the persistent log (creating the file/
    // directory if necessary) and to the in-memory cache. Safe to call
    // after every run regardless of how it was started.
    void addRun(const RunRecord &record);

    // Aggregated stats for just `fingerprint`, across every run ever
    // recorded for it (this session and previous ones alike).
    Aggregate aggregate(const QString &fingerprint) const;

    // True if any run has ever been recorded for `fingerprint`.
    bool hasAnyRuns(const QString &fingerprint) const;

    // Exports every recorded run (every fingerprint, not just the current
    // one) as CSV, one row per run -- for analysis outside the app (SPEC.md
    // 10 ③).
    bool exportCsv(const QString &path) const;

    // Removes every recorded run for `fingerprint` from disk and memory
    // ("統計をリセット"); other fingerprints' history is untouched.
    void resetFingerprint(const QString &fingerprint);

private:
    QVector<RunRecord> m_records;

    bool writeAll() const;
};
