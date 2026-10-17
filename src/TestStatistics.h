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
    // whatever was cached before -- called once at startup. Every other
    // accessor below (aggregate()/hasAnyRuns()/exportCsv()/addRun()) also
    // calls syncFromDisk() itself first, so there is no need to call this
    // again afterward just to pick up runs recorded by *this* process --
    // reload() only remains useful for discarding this cache and starting
    // over from whatever is on disk right now.
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
    // 不具合報告(無人実行・共有環境で静かに眠るもの) 1.3: crash_stats.jsonl is
    // a shared, append-only log that any number of separate process
    // instances write to concurrently (the GUI itself, plus every headless
    // `--run` invocation launched alongside it) -- m_records used to be
    // populated once from disk at GUI startup and never refreshed, so runs
    // recorded by any *other* process while the GUI stayed open were
    // silently invisible to its statistics display and CSV export, even
    // though they were correctly sitting on disk the whole time. Every
    // reader below now calls syncFromDisk() first instead of trusting a
    // cache that can go stale the moment a second process writes a line.
    // mutable because the const read accessors (aggregate()/hasAnyRuns()/
    // exportCsv()) need to trigger this resync too -- it only ever grows or
    // replaces the cache, never changes the logical, externally-visible
    // constness of "what this object currently reports".
    mutable QVector<RunRecord> m_records;
    // How many bytes of historyFilePath() are already reflected in
    // m_records -- lets syncFromDisk() cheaply read just the bytes some
    // process (this one or another) appended since the last sync, instead
    // of re-parsing the whole file every time. Reset to 0 by reload() (full
    // re-read) and to the post-write file size by addRun()/writeAll().
    mutable qint64 m_loadedByteSize = 0;

    // Appends any records written to disk since the last reload()/
    // syncFromDisk()/addRun() -- by this process or (the common case this
    // exists for) another one entirely -- onto m_records, without discarding
    // anything already cached. Falls back to a full reload() if the file is
    // now *smaller* than last seen (resetFingerprint() from this or another
    // process), since byte-offset resumption can't make sense of that.
    void syncFromDisk() const;

    bool writeAll() const;
};
