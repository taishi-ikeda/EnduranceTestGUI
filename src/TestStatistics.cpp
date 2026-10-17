#include "TestStatistics.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMap>
#include <QStandardPaths>
#include <QTextStream>

namespace
{
QJsonObject recordToJson(const TestStatistics::RunRecord &r)
{
    QJsonObject o;
    o["finishedAt"] = r.finishedAt.toString(Qt::ISODate);
    o["configFingerprint"] = r.configFingerprint;
    o["targetAppName"] = r.targetAppName;
    o["crashed"] = r.crashed;
    o["anomaly"] = r.anomaly;
    o["crashStepIndex"] = r.crashStepIndex;
    o["totalIterations"] = double(r.totalIterations);
    o["elapsedMs"] = double(r.elapsedMs);
    o["rngSeedUsed"] = double(r.rngSeedUsed);
    o["stopReason"] = r.stopReason;
    return o;
}

TestStatistics::RunRecord recordFromJson(const QJsonObject &o)
{
    TestStatistics::RunRecord r;
    r.finishedAt = QDateTime::fromString(o["finishedAt"].toString(), Qt::ISODate);
    r.configFingerprint = o["configFingerprint"].toString();
    r.targetAppName = o["targetAppName"].toString();
    r.crashed = o["crashed"].toBool();
    r.anomaly = o["anomaly"].toBool();
    r.crashStepIndex = o["crashStepIndex"].toInt(-1);
    r.totalIterations = qint64(o["totalIterations"].toDouble());
    r.elapsedMs = qint64(o["elapsedMs"].toDouble());
    r.rngSeedUsed = quint32(o["rngSeedUsed"].toDouble());
    r.stopReason = o["stopReason"].toString();
    return r;
}

// Wraps `field` in double quotes (doubling any embedded quotes) whenever it
// contains a character CSV readers would otherwise treat as a delimiter --
// stopReason in particular is free-form, translated text that may contain
// commas.
QString csvField(const QString &field)
{
    if (!field.contains(QLatin1Char(',')) && !field.contains(QLatin1Char('"')) &&
        !field.contains(QLatin1Char('\n')))
        return field;
    QString escaped = field;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}
}  // namespace

QString TestStatistics::computeFingerprint(const QJsonObject &presetJson)
{
    QJsonObject filtered;
    filtered["namedRegions"] = presetJson["namedRegions"];
    filtered["steps"] = presetJson["steps"];
    filtered["defaultActionParams"] = presetJson["defaultActionParams"];
    filtered["defaultActionKinds"] = presetJson["defaultActionKinds"];

    const QJsonObject timing = presetJson["timing"].toObject();
    QJsonObject filteredTiming;
    filteredTiming["intervalMode"] = timing["intervalMode"];
    filteredTiming["minIntervalMs"] = timing["minIntervalMs"];
    filteredTiming["maxIntervalMs"] = timing["maxIntervalMs"];
    filteredTiming["minRate"] = timing["minRate"];
    filteredTiming["maxRate"] = timing["maxRate"];
    filteredTiming["maxIterations"] = timing["maxIterations"];
    filteredTiming["maxDurationSec"] = timing["maxDurationSec"];
    filteredTiming["maxSequenceLoops"] = timing["maxSequenceLoops"];
    filteredTiming["keepTargetActive"] = timing["keepTargetActive"];
    filtered["timing"] = filteredTiming;

    const QByteArray canonical = QJsonDocument(filtered).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(QCryptographicHash::hash(canonical, QCryptographicHash::Sha1).toHex().left(16));
}

QString TestStatistics::historyFilePath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/EnduranceTestGUI_Logs");
    return dir + QStringLiteral("/crash_stats.jsonl");
}

void TestStatistics::reload()
{
    m_records.clear();
    m_loadedByteSize = 0;
    QFile file(historyFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject())
            m_records.append(recordFromJson(doc.object()));
    }
    m_loadedByteSize = file.size();
}

void TestStatistics::syncFromDisk() const
{
    QFile file(historyFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const qint64 currentSize = file.size();
    if (currentSize == m_loadedByteSize)
        return;
    if (currentSize < m_loadedByteSize) {
        // The log is shorter than what we already parsed -- some process
        // (this one or another) called resetFingerprint()/writeAll() and
        // rewrote it from scratch, so resuming from m_loadedByteSize would
        // skip content or read garbage. Only a full reload can make sense
        // of it; const_cast is safe here, reload() only touches the
        // `mutable` cache fields.
        file.close();
        const_cast<TestStatistics *>(this)->reload();
        return;
    }
    if (!file.seek(m_loadedByteSize))
        return;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject())
            m_records.append(recordFromJson(doc.object()));
    }
    m_loadedByteSize = currentSize;
}

void TestStatistics::addRun(const RunRecord &record)
{
    // Pick up anything another process appended first -- otherwise the
    // file.size() captured below after our own write would silently include
    // those unread bytes too, permanently hiding them from every future
    // syncFromDisk() call (it would believe it had already read past them).
    syncFromDisk();
    const QString path = historyFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << QJsonDocument(recordToJson(record)).toJson(QJsonDocument::Compact) << '\n';
        stream.flush();
        m_loadedByteSize = file.size();
    }
    m_records.append(record);
}

TestStatistics::Aggregate TestStatistics::aggregate(const QString &fingerprint) const
{
    syncFromDisk();
    Aggregate agg;
    qint64 iterationsSum = 0;
    qint64 elapsedSum = 0;
    QMap<int, int> byStep;
    for (const RunRecord &r : m_records) {
        if (r.configFingerprint != fingerprint)
            continue;
        ++agg.totalRuns;
        if (r.crashed) {
            ++agg.crashRuns;
            iterationsSum += r.totalIterations;
            elapsedSum += r.elapsedMs;
            if (r.crashStepIndex >= 0)
                ++byStep[r.crashStepIndex];
        }
    }
    if (agg.crashRuns > 0) {
        agg.meanIterationsToCrash = double(iterationsSum) / agg.crashRuns;
        agg.meanElapsedMsToCrash = double(elapsedSum) / agg.crashRuns;
    }
    for (auto it = byStep.constBegin(); it != byStep.constEnd(); ++it)
        agg.crashesByStep.append({it.key(), it.value()});
    return agg;
}

bool TestStatistics::hasAnyRuns(const QString &fingerprint) const
{
    syncFromDisk();
    for (const RunRecord &r : m_records) {
        if (r.configFingerprint == fingerprint)
            return true;
    }
    return false;
}

bool TestStatistics::exportCsv(const QString &path) const
{
    syncFromDisk();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    stream << "finishedAt,configFingerprint,targetAppName,crashed,anomaly,crashStepIndex1Based,"
              "totalIterations,elapsedSec,rngSeedUsed,stopReason\n";
    for (const RunRecord &r : m_records) {
        stream << csvField(r.finishedAt.toString(Qt::ISODate)) << ',' << csvField(r.configFingerprint) << ','
               << csvField(r.targetAppName) << ',' << (r.crashed ? "1" : "0") << ',' << (r.anomaly ? "1" : "0")
               << ',' << (r.crashStepIndex >= 0 ? QString::number(r.crashStepIndex + 1) : QString()) << ','
               << r.totalIterations << ',' << QString::number(r.elapsedMs / 1000.0, 'f', 1) << ','
               << r.rngSeedUsed << ',' << csvField(r.stopReason) << '\n';
    }
    return true;
}

void TestStatistics::resetFingerprint(const QString &fingerprint)
{
    // writeAll() below replaces the entire on-disk file with m_records --
    // without syncing first, any run another process appended since our
    // last sync would simply never make it into `kept` and be permanently
    // lost the moment this overwrites the file, rather than just staying
    // unreset like it should.
    syncFromDisk();
    QVector<RunRecord> kept;
    kept.reserve(m_records.size());
    for (const RunRecord &r : m_records) {
        if (r.configFingerprint != fingerprint)
            kept.append(r);
    }
    m_records = kept;
    writeAll();
}

bool TestStatistics::writeAll() const
{
    const QString path = historyFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    for (const RunRecord &r : m_records)
        stream << QJsonDocument(recordToJson(r)).toJson(QJsonDocument::Compact) << '\n';
    return true;
}
