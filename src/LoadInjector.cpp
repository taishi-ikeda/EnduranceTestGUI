#include "LoadInjector.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QThread>

#include "I18n.h"

#ifdef Q_OS_LINUX
#include <sched.h>
#endif

#ifdef Q_OS_LINUX
namespace
{
// Pins the calling/given process to exactly the cores in `cpuIndices`.
// `pid` == 0 means "the calling thread itself" (sched_setaffinity()'s own
// convention) -- used here to reserve a core for EnduranceTestGUI's own
// main/GUI thread (where RandomActionEngine's action-dispatch QTimer runs).
// Deliberately ignores failure: an unprivileged caller can always set
// affinity for its own process/children on Linux, so a failure here would
// only mean something unusual about the host (e.g. a restrictive sandbox)
// that this feature should degrade gracefully from rather than surface as
// an error -- load injection still runs, just without the core-separation
// guarantee.
void setAffinity(pid_t pid, const QList<int> &cpuIndices)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (int cpu : cpuIndices)
        CPU_SET(cpu, &mask);
    sched_setaffinity(pid, sizeof(mask), &mask);
}

void clearAffinityRestriction(int totalCores)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (int i = 0; i < totalCores; ++i)
        CPU_SET(i, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);
}
}  // namespace
#endif

LoadInjector::LoadInjector(QObject *parent) : QObject(parent), m_totalCores(QThread::idealThreadCount()) {}

LoadInjector::~LoadInjector()
{
    stop();
}

QString LoadInjector::helperExecutablePath() const
{
#ifdef Q_OS_WIN
    const QString helperName = QStringLiteral("loadstress_helper.exe");
#else
    const QString helperName = QStringLiteral("loadstress_helper");
#endif
    const QString candidate = QDir(QCoreApplication::applicationDirPath()).filePath(helperName);
    return QFileInfo(candidate).isExecutable() ? candidate : QString();
}

void LoadInjector::pinWorkerAffinity(qint64 pid, int workerIndex, int totalCores) const
{
#ifdef Q_OS_LINUX
    if (totalCores < 2)
        return;  // nothing to separate onto -- let it share the one core
    // Core 0 is reserved for EnduranceTestGUI itself by start() below;
    // workers round-robin across every other core.
    const int workerCoreCount = totalCores - 1;
    const int assignedCore = 1 + (workerIndex % workerCoreCount);
    setAffinity(static_cast<pid_t>(pid), {assignedCore});
#else
    Q_UNUSED(pid);
    Q_UNUSED(workerIndex);
    Q_UNUSED(totalCores);
#endif
}

bool LoadInjector::start(int processCount, int memoryMbPerProcess, QString &errorMessage)
{
    stop();  // always start from a clean slate with the newly requested parameters

    const QString helperPath = helperExecutablePath();
    if (helperPath.isEmpty()) {
        errorMessage =
            I18n::t(QStringLiteral("loadstress_helper実行ファイルが見つかりません（%1 に配置されている必要があります）"))
                .arg(QCoreApplication::applicationDirPath());
        return false;
    }

#ifdef Q_OS_LINUX
    // Reserve core 0 for EnduranceTestGUI's own main/GUI thread -- see this
    // class's header comment for why that thread specifically matters here.
    // Uses m_totalCores (cached at construction, before any self-restriction
    // could ever skew it), not a fresh QThread::idealThreadCount() call --
    // see m_totalCores' own comment.
    if (m_totalCores >= 2)
        setAffinity(0, {0});
#endif

    for (int i = 0; i < processCount; ++i) {
        auto *process = new QProcess(this);
        process->setProgram(helperPath);
        process->setArguments({QString::number(memoryMbPerProcess)});
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                &LoadInjector::onWorkerFinished);
        process->start();
        if (!process->waitForStarted(3000)) {
            errorMessage =
                I18n::t(QStringLiteral("負荷注入プロセスの起動に失敗しました: %1")).arg(process->errorString());
            delete process;
            stop();
            return false;
        }
        pinWorkerAffinity(process->processId(), i, m_totalCores);
        m_workers.append(process);
    }
    return true;
}

void LoadInjector::stop()
{
    m_stopping = true;
    for (QProcess *process : m_workers)
        process->terminate();
    for (QProcess *process : m_workers) {
        if (!process->waitForFinished(2000))
            process->kill();
        process->deleteLater();
    }
    m_workers.clear();
    m_stopping = false;

#ifdef Q_OS_LINUX
    // Release EnduranceTestGUI's own core reservation now that no worker
    // needs to be kept off of it.
    if (m_totalCores >= 2)
        clearAffinityRestriction(m_totalCores);
#endif
}

void LoadInjector::onWorkerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    if (m_stopping)
        return;  // our own stop() caused this -- not unexpected
    auto *process = qobject_cast<QProcess *>(sender());
    const qint64 pid = process ? process->processId() : -1;
    m_workers.removeOne(process);
    if (process)
        process->deleteLater();
    emit workerExitedUnexpectedly(pid, exitCode);
}
