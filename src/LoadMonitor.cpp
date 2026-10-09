#include "LoadMonitor.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QTimer>

#include "LoadInjector.h"
#include "platform/PlatformAutomation.h"

LoadMonitor::LoadMonitor(QObject *parent) : QObject(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &LoadMonitor::onTick);
}

void LoadMonitor::start(qint64 targetPid, int intervalMs)
{
    m_targetPid = targetPid;
    m_samples.clear();
    m_hasCpuSample = false;
    m_hasSystemCpuSample = false;
    m_timer->start(qMax(50, intervalMs));
    emit sampleAdded();  // let a chart widget clear itself immediately
}

void LoadMonitor::stop()
{
    m_timer->stop();
}

bool LoadMonitor::isRunning() const
{
    return m_timer->isActive();
}

void LoadMonitor::onTick()
{
    LoadSample sample;
    sample.timestampMs = QDateTime::currentMSecsSinceEpoch();
    sample.loadInjectionActive = m_loadInjector && m_loadInjector->isRunning();

    const ProcessStats targetStats = PlatformAutomation::queryProcessStats(m_targetPid);
    if (targetStats.ok) {
        sample.targetMemoryMb = targetStats.residentMemoryMB;
        if (m_hasCpuSample) {
            const double deltaCpuSec = targetStats.cpuTimeSeconds - m_lastCpuTimeSeconds;
            const double deltaWallSec = double(sample.timestampMs - m_lastCpuSampleMs) / 1000.0;
            if (deltaWallSec > 0.0)
                sample.targetCpuPercent = qMax(0.0, deltaCpuSec / deltaWallSec * 100.0);
        }
        m_lastCpuTimeSeconds = targetStats.cpuTimeSeconds;
        m_lastCpuSampleMs = sample.timestampMs;
        m_hasCpuSample = true;
    }

    const SystemCpuStats systemStats = PlatformAutomation::querySystemCpuStats();
    if (systemStats.ok) {
        if (m_hasSystemCpuSample) {
            const double deltaTotal = systemStats.totalCpuTimeSeconds - m_lastSystemTotalSeconds;
            const double deltaIdle = systemStats.idleCpuTimeSeconds - m_lastSystemIdleSeconds;
            if (deltaTotal > 0.0)
                sample.systemCpuPercent = qBound(0.0, (deltaTotal - deltaIdle) / deltaTotal * 100.0, 100.0);
        }
        m_lastSystemTotalSeconds = systemStats.totalCpuTimeSeconds;
        m_lastSystemIdleSeconds = systemStats.idleCpuTimeSeconds;
        m_hasSystemCpuSample = true;
    }

    m_samples.append(sample);
    while (m_samples.size() > kMaxSamples)
        m_samples.removeFirst();

    emit sampleAdded();
}

bool LoadMonitor::saveSamplesAsCsv(const QString &path) const
{
    if (m_samples.isEmpty())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "timestamp,target_cpu_percent,target_memory_mb,system_cpu_percent,load_injection_active\n";
    for (const LoadSample &sample : m_samples) {
        out << QDateTime::fromMSecsSinceEpoch(sample.timestampMs).toString(Qt::ISODateWithMs) << ','
            << QString::number(sample.targetCpuPercent, 'f', 2) << ','
            << QString::number(sample.targetMemoryMb, 'f', 2) << ','
            << (sample.systemCpuPercent >= 0.0 ? QString::number(sample.systemCpuPercent, 'f', 2) : QString())
            << ',' << (sample.loadInjectionActive ? "1" : "0") << '\n';
    }
    return true;
}
