#pragma once

#include <QList>
#include <QObject>
#include <QProcess>

// SPEC.md 追加実装依頼「負荷注入モード」: deliberately injects CPU+memory load
// so a load/timing-dependent ("ハイゼンバグ") crash in the target app can be
// reproduced more reliably -- the user's own hypothesis was that other
// software's CPU load on the same machine changes the target's own thread-
// scheduling timing, and this is the tool to go test that directly.
//
// Launches `processCount` independent OS *processes* (never threads inside
// EnduranceTestGUI's own process), each running the loadstress_helper
// executable, which busy-loops one CPU core and continually touches a block
// of memory until terminated. This is out-of-process on purpose:
// RandomActionEngine's action-dispatch loop is a single QTimer running on
// EnduranceTestGUI's own main/GUI thread (see its own header), so a CPU-
// hungry *thread* sharing that same process would compete with the dispatch
// timer for the same scheduling slot -- undermining the feature's own goal
// by slowing down/jittering EnduranceTestGUI's own synthetic clicks together
// with the target app's. A separate process can be pinned away from that
// thread's core instead (see applyCpuAffinityExcludingSelf()).
//
// On Linux, each worker's CPU affinity is pinned, via sched_setaffinity()
// (which needs no special privilege for a process's own child -- see
// LoadInjector.cpp), away from a core reserved for EnduranceTestGUI's own
// process while injection is running. macOS has no public API to pin
// another process's CPU affinity (thread_policy_set's affinity tag is only
// an advisory hint for threads within the *same* process), so there workers
// simply run unpinned -- still genuine CPU/memory load, just without the
// "never shares a core with EnduranceTestGUI itself" guarantee.
class LoadInjector : public QObject
{
    Q_OBJECT

public:
    explicit LoadInjector(QObject *parent = nullptr);
    ~LoadInjector() override;

    // Stops whatever is currently running (if anything) and starts
    // `processCount` fresh worker processes, each allocating/touching
    // `memoryMbPerProcess` MB. Returns false (errorMessage filled in, and
    // nothing left running) if the helper executable can't be found or a
    // worker fails to start.
    bool start(int processCount, int memoryMbPerProcess, QString &errorMessage);
    // Terminates every worker (QProcess::terminate(), then kill() after a
    // short grace period if still alive) and clears all state. Safe to call
    // when nothing is running.
    void stop();
    bool isRunning() const { return !m_workers.isEmpty(); }
    int runningProcessCount() const { return m_workers.size(); }

signals:
    // A worker process exited on its own (crashed, was killed by something
    // other than our own stop()) while it was still supposed to be running.
    // Informational only -- MainWindow just logs it; never treated as a
    // target-app anomaly.
    void workerExitedUnexpectedly(qint64 pid, int exitCode);

private slots:
    void onWorkerFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    // Absolute path to the loadstress_helper executable, expected to sit
    // right next to this app's own binary (QCoreApplication::applicationDirPath(),
    // the same convention CMakeLists.txt's RUNTIME_OUTPUT_DIRECTORY setup for
    // the loadstress_helper target follows). Empty if not found there.
    QString helperExecutablePath() const;
    // Linux only: pins `pid`'s CPU affinity to one core reserved for worker
    // processes (never the core(s) reserved for EnduranceTestGUI itself by
    // start()'s own self-affinity call below), spreading workers round-robin
    // across whichever cores remain via `workerIndex`. A silent no-op (not
    // an error -- see this class's header comment) on any other platform,
    // or if fewer than 2 logical cores are available to begin with.
    // `totalCores` must be the *unrestricted* logical CPU count, queried by
    // the caller before start() narrows this process's own affinity down to
    // core 0 -- QThread::idealThreadCount() itself reads the calling
    // thread's current affinity mask on Linux, so calling it again from in
    // here (after that self-restriction) would see only 1 CPU and silently
    // skip pinning every worker entirely, leaving them all inheriting the
    // same core-0-only mask instead of spreading across the rest.
    void pinWorkerAffinity(qint64 pid, int workerIndex, int totalCores) const;

    QList<QProcess *> m_workers;
    // Guards onWorkerFinished() against treating our own stop()'s
    // terminate()/kill() calls as an unexpected exit.
    bool m_stopping = false;
    // The true, unrestricted logical CPU count, queried once at construction
    // time (before this process's own affinity is ever touched) and reused
    // by every start()/stop() call thereafter. Needed because
    // QThread::idealThreadCount() itself reads the *calling thread's
    // current* CPU affinity mask on Linux: querying it again after start()
    // has already restricted this process to one core (see its own
    // comment) would see just that 1 core and wrongly conclude there's
    // nothing left to pin workers to, or nothing left to release in stop().
    int m_totalCores = 0;
};
