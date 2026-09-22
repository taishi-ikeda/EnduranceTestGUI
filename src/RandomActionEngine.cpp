#include "RandomActionEngine.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QJsonArray>
#include <QKeySequence>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QStandardPaths>
#include <QThread>
#include <QtMath>

#include "platform/PlatformAutomation.h"
#include "I18n.h"

namespace
{
bool parseShortcut(const QString &text, Qt::Key &outKey, Qt::KeyboardModifiers &outMods)
{
    const QKeySequence sequence(text);
    if (sequence.count() == 0)
        return false;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QKeyCombination combo = sequence[0];
    outKey = combo.key();
    outMods = combo.keyboardModifiers();
#else
    const int combined = sequence[0];
    outMods = Qt::KeyboardModifiers(combined & Qt::KeyboardModifierMask);
    outKey = Qt::Key(combined & ~Qt::KeyboardModifierMask);
#endif
    return outKey != 0;
}
}  // namespace

QString RandomActionEngine::formatSummaryText(const RunSummary &summary)
{
    QStringList lines;
    lines << I18n::t(QStringLiteral("==== 実行結果サマリー ===="));
    lines << I18n::t(QStringLiteral("停止理由: %1%2"))
                 .arg(summary.stopReason, summary.anomaly ? I18n::t(QStringLiteral("（異常停止）")) : QString());
    lines << I18n::t(QStringLiteral("実行回数（全ステップ合計）: %1")).arg(summary.totalIterations);
    lines << I18n::t(QStringLiteral("完走したシーケンス回数: %1")).arg(summary.sequenceLoopsCompleted);
    lines << I18n::t(QStringLiteral("経過時間: %1 秒")).arg(summary.elapsedMs / 1000.0, 0, 'f', 1);
    lines << I18n::t(QStringLiteral("使用した乱数シード: %1")).arg(summary.rngSeedUsed);
    if (summary.anomaly && !summary.anomalyArtifactTimestamp.isEmpty()) {
        lines << I18n::t(QStringLiteral("異常停止時の記録一式: %1 内の「anomaly_%2」で始まるファイル/フォルダ"))
                     .arg(RandomActionEngine::anomalyArtifactsDirectory(), summary.anomalyArtifactTimestamp);
    }
    if (summary.actionKindCounts.isEmpty()) {
        lines << I18n::t(QStringLiteral("操作種別ごとの回数: (なし)"));
    } else {
        lines << I18n::t(QStringLiteral("操作種別ごとの回数:"));
        for (auto it = summary.actionKindCounts.constBegin(); it != summary.actionKindCounts.constEnd();
             ++it)
            lines << QStringLiteral("  %1: %2").arg(it.key()).arg(it.value());
    }
    if (!summary.recentActions.isEmpty()) {
        lines << I18n::t(QStringLiteral("直近の操作（古い順、確率的な不具合の解析用）:"));
        for (const QString &action : summary.recentActions)
            lines << QStringLiteral("  %1").arg(action);
    }
    return lines.join(QStringLiteral("\n"));
}

QJsonObject RandomActionEngine::summaryToJson(const RunSummary &summary)
{
    QJsonObject obj;
    obj["stopReason"] = summary.stopReason;
    obj["anomaly"] = summary.anomaly;
    obj["totalIterations"] = double(summary.totalIterations);
    obj["sequenceLoopsCompleted"] = double(summary.sequenceLoopsCompleted);
    obj["elapsedMs"] = double(summary.elapsedMs);
    obj["rngSeedUsed"] = double(summary.rngSeedUsed);
    if (!summary.anomalyArtifactTimestamp.isEmpty())
        obj["anomalyArtifactTimestamp"] = summary.anomalyArtifactTimestamp;
    obj["targetCrashed"] = summary.targetCrashed;
    if (summary.crashStepIndex >= 0)
        obj["crashStepIndex"] = summary.crashStepIndex;
    QJsonObject counts;
    for (auto it = summary.actionKindCounts.constBegin(); it != summary.actionKindCounts.constEnd(); ++it)
        counts[it.key()] = double(it.value());
    obj["actionKindCounts"] = counts;
    QJsonArray recentActions;
    for (const QString &action : summary.recentActions)
        recentActions.append(action);
    obj["recentActions"] = recentActions;
    return obj;
}

QString RandomActionEngine::anomalyArtifactsDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) +
           QStringLiteral("/EnduranceTestGUI_Screenshots");
}

namespace
{
// Recording-buffer sizing (SPEC.md 6.7/10, TestConfig::enableScreenRecording):
// 20 frames at 500ms apart keeps roughly the last 10 seconds of on-screen
// activity leading up to an anomaly, bounded regardless of how long the run
// has been going.
constexpr int kRecordingFrameIntervalMs = 500;
constexpr int kMaxRecordingFrames = 20;
// How many of the most recent action descriptions RunSummary::recentActions
// keeps (SPEC.md 6.7/10) -- enough to see the short pattern of operations
// leading up to a crash without ballooning every summary.
constexpr int kRecentActionHistorySize = 15;
// Startup setup-phase safety-check retries (SPEC.md 6.x "起動時セットアップ"
// ④): how many times in a row a single SetupAction may fail its
// target-window/target-active safety check (e.g. the window hasn't finished
// appearing/laying out yet right after launch) before the run gives up and
// stops, and how long to wait between attempts. Bounded and short, since a
// genuinely wrong setup macro (pointing at a widget that will never appear)
// should fail fast rather than stall the run for a long time.
constexpr int kMaxSetupSafetyRetries = 20;
constexpr int kSetupSafetyRetryDelayMs = 300;
}  // namespace

void RandomActionEngine::recordRecentAction(const QString &desc)
{
    m_recentActionDescriptions.append(desc);
    while (m_recentActionDescriptions.size() > kRecentActionHistorySize)
        m_recentActionDescriptions.removeFirst();
}

RandomActionEngine::RandomActionEngine(QObject *parent) : QObject(parent), m_rng(0)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &RandomActionEngine::performRandomAction);

    m_resourceTimer.setInterval(5000);
    connect(&m_resourceTimer, &QTimer::timeout, this, &RandomActionEngine::sampleResourceUsage);

    m_recordingTimer.setInterval(kRecordingFrameIntervalMs);
    connect(&m_recordingTimer, &QTimer::timeout, this, &RandomActionEngine::captureRecordingFrame);

    // 8-second cadence: PlatformAutomation::checkWindowResponsive()'s
    // "trailing evaluation" design means this interval doubles as the
    // grace period before a missed reply counts as one strike (see its
    // header comment) -- long enough that a brief, harmless stall (e.g. a
    // big layout pass) isn't mistaken for a hang, short enough that two
    // consecutive misses (the threshold below) still triggers well before
    // most of a short run's time/iteration budget is wasted spinning
    // against a truly frozen target.
    m_hangCheckTimer.setInterval(8000);
    connect(&m_hangCheckTimer, &QTimer::timeout, this, &RandomActionEngine::checkTargetResponsiveness);
}

void RandomActionEngine::start(const TestConfig &config)
{
    if (m_running)
        stop();

    m_config = config;
    m_iterationCount = 0;
    m_currentStepIndex = 0;
    m_currentStepActionsDone = 0;
    m_currentTaskMemberIndex = 0;
    m_popupDialogWaitStrikes = 0;
    m_sequenceLoopCount = 0;
    m_pausedElapsedMs = 0;
    m_elapsed.restart();
    m_running = true;
    m_paused = false;
    m_hasCpuSample = false;
    m_unexpectedWindowStrikes = 0;
    m_consecutiveUnresponsive = 0;
    m_everRespondedToPing = false;
    m_neverRespondedStrikes = 0;
    m_capturedThisRun = false;
    m_lastScreenshotStepIndex = -1;
    m_lastScreenshotIterationCount = -1;
    m_recordingFrames.clear();
    m_recentActionDescriptions.clear();
    m_inSetupPhase = !m_config.setupActions.isEmpty();
    m_setupActionIndex = 0;
    m_setupSafetyRetryCount = 0;

    // A seed of 0 means "pick a fresh random one" -- but 0 is also a
    // perfectly valid *explicit* seed a user might type back in to
    // reproduce a run, so generate a nonzero replacement when auto-picking.
    quint32 seed = m_config.rngSeed;
    if (seed == 0)
        seed = QRandomGenerator::global()->generate() | 1u;
    m_rng.seed(seed);
    m_rngSeedUsed = seed;
    m_actionKindCounts.clear();
    emit logMessage(I18n::t(QStringLiteral("乱数シード: %1（クラッシュ等の再現に使う場合はこの値を記録してください）"))
                         .arg(seed));

    if (m_config.keepTargetActive)
        PlatformAutomation::activateProcess(m_config.targetPid);

    emit logMessage(I18n::t(QStringLiteral("テストを開始しました（ステップ数: %1）")).arg(m_config.steps.size()));
    emit iterationCountChanged(m_iterationCount);
    if (!m_config.steps.isEmpty())
        emit currentStepChanged(m_currentStepIndex);
    if (m_inSetupPhase) {
        emit logMessage(I18n::t(QStringLiteral("起動時セットアップを開始します（%1件） -- 完了後にランダム操作を開始します"))
                             .arg(m_config.setupActions.size()));
    }
    scheduleNext();
    m_resourceTimer.start();
    m_hangCheckTimer.start();
    if (m_config.enableScreenRecording) {
        emit logMessage(I18n::t(QStringLiteral("画面録画（直近%1秒分をリングバッファ保持）を有効にしました"))
                             .arg(kRecordingFrameIntervalMs * kMaxRecordingFrames / 1000));
        m_recordingTimer.start();
    }
}

void RandomActionEngine::stop()
{
    if (!m_running)
        return;
    doStop(I18n::t(QStringLiteral("ユーザーにより停止されました")));
}

void RandomActionEngine::pause()
{
    if (!m_running || m_paused)
        return;
    m_paused = true;
    m_pausedElapsedMs += m_elapsed.elapsed();
    m_timer.stop();
    m_resourceTimer.stop();
    m_hangCheckTimer.stop();
    emit pausedChanged(true);
    emit logMessage(I18n::t(QStringLiteral("一時停止しました")));
}

void RandomActionEngine::resume()
{
    if (!m_running || !m_paused)
        return;
    m_paused = false;
    m_elapsed.restart();
    emit pausedChanged(false);
    emit logMessage(I18n::t(QStringLiteral("再開しました")));
    scheduleNext();
    m_resourceTimer.start();
    m_hangCheckTimer.start();
}

QString RandomActionEngine::describeActionKind(ActionKind kind) const
{
    switch (kind) {
    case ActionKind::Click: return I18n::t(QStringLiteral("クリック"));
    case ActionKind::DoubleClick: return I18n::t(QStringLiteral("ダブルクリック"));
    case ActionKind::Drag: return I18n::t(QStringLiteral("ドラッグ"));
    case ActionKind::Key: return I18n::t(QStringLiteral("キー入力"));
    case ActionKind::ScrollUp: return I18n::t(QStringLiteral("スクロール(上)"));
    case ActionKind::ScrollDown: return I18n::t(QStringLiteral("スクロール(下)"));
    case ActionKind::ScrollHorizontal: return I18n::t(QStringLiteral("スクロール(横)"));
    case ActionKind::Shortcut: return I18n::t(QStringLiteral("ショートカット"));
    case ActionKind::WindowOp: return I18n::t(QStringLiteral("ウィンドウ操作"));
    }
    return QString();
}

void RandomActionEngine::doStop(const QString &reason, bool isAnomaly, bool targetCrashed)
{
    m_running = false;
    m_paused = false;
    m_timer.stop();
    m_resourceTimer.stop();
    m_hangCheckTimer.stop();
    m_recordingTimer.stop();

    RunSummary summary;
    summary.stopReason = reason;
    summary.anomaly = isAnomaly;
    summary.targetCrashed = targetCrashed;
    summary.crashStepIndex = targetCrashed ? m_currentStepIndex : -1;
    summary.recentActions = m_recentActionDescriptions;
    if (isAnomaly)
        summary.anomalyArtifactTimestamp = captureAnomalyArtifacts(reason);
    m_recordingFrames.clear();  // recording is per-run regardless of whether it just got saved above
    summary.rngSeedUsed = m_rngSeedUsed;
    summary.totalIterations = m_iterationCount;
    summary.sequenceLoopsCompleted = m_sequenceLoopCount;
    // Matches the running duration-limit check's own formula (only exact
    // while not currently paused, which doStop() itself just forced above --
    // a run stopped while genuinely mid-pause may over-count the paused
    // interval slightly, a pre-existing quirk of how pause()/resume() track
    // time, not something this reporting-only field needs to fix).
    summary.elapsedMs = m_pausedElapsedMs + m_elapsed.elapsed();
    for (auto it = m_actionKindCounts.constBegin(); it != m_actionKindCounts.constEnd(); ++it)
        summary.actionKindCounts.insert(describeActionKind(it.key()), it.value());

    emit logMessage(reason);
    emit summaryReady(summary);
    emit finished(reason);
}

QString RandomActionEngine::captureAnomalyArtifacts(const QString &reason)
{
    Q_UNUSED(reason);
    const QString baseDir = anomalyArtifactsDirectory();
    if (!QDir().mkpath(baseDir)) {
        emit logMessage(I18n::t(QStringLiteral("異常停止時の記録の保存先作成に失敗しました: %1")).arg(baseDir));
        return QString();
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QList<QScreen *> screens = QGuiApplication::screens();
    QStringList savedPaths;
    for (int i = 0; i < screens.size(); ++i) {
        const QPixmap pixmap = screens[i]->grabWindow(0);
        if (pixmap.isNull())
            continue;
        const QString path =
            QStringLiteral("%1/anomaly_%2_screen%3.png").arg(baseDir, timestamp).arg(i);
        if (pixmap.save(path))
            savedPaths << path;
    }

    if (!savedPaths.isEmpty()) {
        emit logMessage(
            I18n::t(QStringLiteral("異常検知時のスクリーンショットを保存しました: %1")).arg(savedPaths.join(QStringLiteral(", "))));
    } else {
        emit logMessage(I18n::t(QStringLiteral("スクリーンショットの保存に失敗しました（macOSでは画面収録の権限が必要な場合があります）")));
    }

    if (m_config.enableScreenRecording)
        saveRecordingFrames(timestamp);

    if (m_config.enableCrashDumpCollection) {
        const QString crashReportPath =
            PlatformAutomation::findRecentCrashReport(m_config.targetPid, m_config.targetAppName);
        if (!crashReportPath.isEmpty()) {
            emit logMessage(I18n::t(QStringLiteral("対象アプリのものと思われるクラッシュレポートを見つけました: %1"))
                                 .arg(crashReportPath));
            // Non-admin Linux users commonly can list systemd-coredump's
            // directory (world-searchable) but not read the individual
            // files inside it (typically root-owned, mode 0640 or
            // stricter) -- confirmed by direct testing (SPEC.md "非管理者
            // Linuxユーザーでの動作"). Warn about that now rather than
            // silently recording a path the user won't actually be able to
            // open later.
            if (!QFileInfo(crashReportPath).isReadable()) {
                emit logMessage(I18n::t(QStringLiteral(
                    "このファイルは現在のユーザー権限では読み取れません（root権限、またはcoredumpctl等の"
                    "専用ツールが必要な場合があります）。パスの記録のみ行いました。")));
            }
            const QString refPath =
                QStringLiteral("%1/anomaly_%2_crashreport_location.txt").arg(baseDir, timestamp);
            QFile refFile(refPath);
            if (refFile.open(QIODevice::WriteOnly | QIODevice::Text))
                refFile.write(crashReportPath.toUtf8());
        } else {
            emit logMessage(I18n::t(QStringLiteral("対象アプリのクラッシュレポート/コアダンプは見つかりませんでした"
                "（このシステムでその機能自体が無効になっている可能性があります）")));
        }
    }

    return timestamp;
}

void RandomActionEngine::saveRecordingFrames(const QString &timestamp)
{
    if (m_recordingFrames.isEmpty())
        return;

    const QString dir = QStringLiteral("%1/anomaly_%2_recording").arg(anomalyArtifactsDirectory(), timestamp);
    if (!QDir().mkpath(dir)) {
        emit logMessage(I18n::t(QStringLiteral("録画フレームの保存先作成に失敗しました: %1")).arg(dir));
        return;
    }

    int saved = 0;
    for (int i = 0; i < m_recordingFrames.size(); ++i) {
        const QString path = QStringLiteral("%1/frame_%2.png").arg(dir).arg(i + 1, 4, 10, QChar('0'));
        if (m_recordingFrames[i].save(path))
            ++saved;
    }
    emit logMessage(I18n::t(QStringLiteral("異常停止直前の画面録画（%1フレーム、約%2秒分）を保存しました: %3"))
                         .arg(saved)
                         .arg(saved * kRecordingFrameIntervalMs / 1000)
                         .arg(dir));
}

void RandomActionEngine::captureRecordingFrame()
{
    if (!m_running || m_paused)
        return;
    const QPixmap frame = grabTargetWindowScreenshot();
    if (frame.isNull())
        return;
    m_recordingFrames.append(frame);
    if (m_recordingFrames.size() > kMaxRecordingFrames)
        m_recordingFrames.removeFirst();
}

void RandomActionEngine::checkTargetResponsiveness()
{
    if (!m_running || m_paused)
        return;

    const PlatformAutomation::ResponsivenessCheck result =
        PlatformAutomation::checkWindowResponsive(m_config.targetWindowId, m_config.targetPid);
    if (result == PlatformAutomation::ResponsivenessCheck::Unsupported) {
        // Not supported for this window/platform at all (see SPEC.md 8/10)
        // -- stop polling rather than keep calling a check that can never
        // return anything else.
        m_hangCheckTimer.stop();
        return;
    }
    if (result == PlatformAutomation::ResponsivenessCheck::Pending)
        return;  // first-ever probe just sent; nothing to evaluate until next call

    if (result == PlatformAutomation::ResponsivenessCheck::NotResponding) {
        if (!m_everRespondedToPing) {
            // Never seen this target answer a single ping -- most likely
            // it declares _NET_WM_PING support without actually
            // implementing the reply (see the m_everRespondedToPing
            // comment in the header), not that it's hung from the very
            // first check. Give up on hang-checking for this run instead
            // of treating "never worked" the same as "stopped working".
            ++m_neverRespondedStrikes;
            if (m_neverRespondedStrikes >= 3) {
                emit logMessage(I18n::t(QStringLiteral("対象アプリが応答確認（WM_PING）に一度も応答しないため、ハング検知を無効にします"
                    "（対応していないツールキット/実装の可能性があります）")));
                m_hangCheckTimer.stop();
            }
            return;
        }
        ++m_consecutiveUnresponsive;
        emit logMessage(
            I18n::t(QStringLiteral("対象アプリの応答確認に失敗しました（%1回連続）")).arg(m_consecutiveUnresponsive));
        // Require two consecutive misses (~2 check intervals) before
        // treating this as a real hang rather than one slow/busy moment --
        // see the interval comment in the constructor.
        if (m_consecutiveUnresponsive >= 2) {
            doStop(I18n::t(QStringLiteral("対象アプリが応答していない（ハング）ことを検知したため停止しました")),
                   /*isAnomaly=*/true);
        }
    } else {
        m_everRespondedToPing = true;
        m_neverRespondedStrikes = 0;
        m_consecutiveUnresponsive = 0;
    }
}

void RandomActionEngine::sampleResourceUsage()
{
    if (!m_running)
        return;

    const ProcessStats stats = PlatformAutomation::queryProcessStats(m_config.targetPid);
    if (!stats.ok)
        return;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    double cpuPercent = 0.0;
    if (m_hasCpuSample) {
        const double deltaCpuSec = stats.cpuTimeSeconds - m_lastCpuTimeSeconds;
        const double deltaWallSec = double(nowMs - m_lastCpuSampleMs) / 1000.0;
        if (deltaWallSec > 0.0)
            cpuPercent = qMax(0.0, deltaCpuSec / deltaWallSec * 100.0);
    }
    m_lastCpuTimeSeconds = stats.cpuTimeSeconds;
    m_lastCpuSampleMs = nowMs;
    m_hasCpuSample = true;

    emit resourceUsageUpdated(stats.residentMemoryMB, cpuPercent);
    emit logMessage(I18n::t(QStringLiteral("リソース使用状況: メモリ %1 MB, CPU %2%"))
                         .arg(stats.residentMemoryMB, 0, 'f', 1)
                         .arg(cpuPercent, 0, 'f', 1));
}

bool RandomActionEngine::resolveStepRegion(const RegionStep &step, QList<QRect> &outIncludeRegions,
                                            QList<QRect> &outExcludeRegions)
{
    if (step.targetsPopupDialog) {
        // SPEC.md 6.2追加実装及び修正依頼: operate on whichever top-level
        // window the target process currently has open besides the main
        // one -- presumably a dialog a preceding task member's action just
        // opened. If more than one extra window exists, the first one
        // listWindowIdsForPid() happens to return is used (a known
        // limitation -- see SPEC.md 8 -- since there is no reliable
        // cross-platform "most recently opened" ordering available).
        const QList<quint32> windowIds = PlatformAutomation::listWindowIdsForPid(m_config.targetPid);
        quint32 popupWindowId = 0;
        for (quint32 id : windowIds) {
            if (id != m_config.targetWindowId) {
                popupWindowId = id;
                break;
            }
        }
        if (popupWindowId == 0)
            return false;  // the expected dialog hasn't appeared (yet) -- see runOneAction()

        QRect bounds;
        if (!PlatformAutomation::queryWindowBounds(popupWindowId, m_config.targetPid, bounds))
            return false;
        outIncludeRegions = {bounds};
        outExcludeRegions.clear();
        return true;
    }

    if (step.useWholeWindow) {
        QRect bounds;
        if (!PlatformAutomation::queryWindowBounds(m_config.targetWindowId, m_config.targetPid,
                                                     bounds)) {
            return false;
        }
        outIncludeRegions = {bounds};
        outExcludeRegions.clear();
        return true;
    }

    for (const NamedRegion &region : m_config.namedRegions) {
        if (region.name == step.regionName) {
            if (region.regions.isEmpty())
                return false;
            outIncludeRegions = region.regions;
            outExcludeRegions = region.excludeRegions;

            // SPEC.md 6.3/10: a region created with "対象ウィンドウの移動に
            // 追従させる" stores its rectangles relative to where the
            // target window's top-left was when it was drawn/saved
            // (region.anchorTopLeft) -- translate by how far the window
            // has moved since, so the region keeps tracking the same
            // relative position instead of staying at fixed screen
            // coordinates forever. If the window can't currently be
            // located, fall back to the as-drawn coordinates unshifted
            // (same behavior as a non-following region) rather than
            // failing the whole step.
            if (region.followsTargetWindow) {
                QRect currentBounds;
                if (PlatformAutomation::queryWindowBounds(m_config.targetWindowId, m_config.targetPid,
                                                            currentBounds)) {
                    const QPoint delta = currentBounds.topLeft() - region.anchorTopLeft;
                    if (!delta.isNull()) {
                        for (QRect &r : outIncludeRegions)
                            r.translate(delta);
                        for (QRect &r : outExcludeRegions)
                            r.translate(delta);
                    }
                }
            }
            return true;
        }
    }
    return false;  // referenced named region no longer exists
}

void RandomActionEngine::maybeCaptureRegionScreenshot(const QString &stepLabel, const QString &regionName,
                                                        const QList<QRect> &includeRegions,
                                                        const QList<QRect> &excludeRegions)
{
    bool shouldCapture = false;
    switch (m_config.screenshotCaptureMode) {
    case ScreenshotCaptureMode::OnceAtStart:
        shouldCapture = !m_capturedThisRun;
        break;
    case ScreenshotCaptureMode::PerStepChange:
        shouldCapture = (m_currentStepIndex != m_lastScreenshotStepIndex);
        break;
    case ScreenshotCaptureMode::FixedInterval: {
        const qint64 interval = qMax<qint64>(1, m_config.screenshotCaptureIntervalActions);
        shouldCapture = (m_iterationCount % interval == 0) &&
                         (m_iterationCount != m_lastScreenshotIterationCount);
        break;
    }
    }
    if (!shouldCapture)
        return;

    const QPixmap shot = renderRegionScreenshot(regionName, includeRegions, excludeRegions);
    if (shot.isNull())
        return;

    m_lastCapturedScreenshot = shot;
    m_lastCapturedScreenshotLabel = stepLabel;
    m_lastCapturedScreenshotRegionName = regionName;
    m_hasCapturedScreenshot = true;
    m_capturedThisRun = true;
    m_lastScreenshotStepIndex = m_currentStepIndex;
    m_lastScreenshotIterationCount = m_iterationCount;
    emit regionScreenshotCaptured();
}

QPixmap RandomActionEngine::grabTargetWindowScreenshot() const
{
    QRect windowBounds;
    if (!PlatformAutomation::queryWindowBounds(m_config.targetWindowId, m_config.targetPid, windowBounds))
        return QPixmap();

    QScreen *screen = QGuiApplication::screenAt(windowBounds.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return QPixmap();

    // grabWindow(0, x, y, w, h) takes x/y relative to the given screen's own
    // origin, not the virtual desktop's -- translate windowBounds into that
    // screen's local coordinates before grabbing.
    const QRect localBounds = windowBounds.translated(-screen->geometry().topLeft());
    return screen->grabWindow(0, localBounds.x(), localBounds.y(), localBounds.width(),
                               localBounds.height());
}

namespace
{
// Draws `text` in a small filled background box anchored at `rect`'s
// top-left corner (SPEC.md "保存するPNGの操作領域に名前を付ける") so the
// region/exclude-mask name is legible directly on the saved image, not just
// in its file name. Placed just inside the box's border rather than above
// it, so it stays on-image even when the box touches the screenshot's edge.
void drawRegionLabel(QPainter &painter, const QRect &rect, const QString &text, const QColor &background)
{
    if (text.isEmpty())
        return;
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);
    const QFontMetrics fm(font);
    const QSize textSize = fm.size(Qt::TextSingleLine, text);
    const QRect labelRect(rect.left() + 2, rect.top() + 2, textSize.width() + 6, textSize.height() + 4);
    painter.fillRect(labelRect, background);
    painter.setPen(Qt::white);
    painter.drawText(labelRect, Qt::AlignCenter, text);
}
}  // namespace

QPixmap RandomActionEngine::renderRegionScreenshot(const QString &regionName, const QList<QRect> &includeRegions,
                                                     const QList<QRect> &excludeRegions) const
{
    if (includeRegions.isEmpty())
        return QPixmap();

    QRect windowBounds;
    if (!PlatformAutomation::queryWindowBounds(m_config.targetWindowId, m_config.targetPid, windowBounds))
        return QPixmap();

    QPixmap shot = grabTargetWindowScreenshot();
    if (shot.isNull())
        return shot;

    QPainter painter(&shot);
    const QColor includeColor(0, 200, 0);
    const QColor excludeColor(220, 0, 0);

    QPen includePen(includeColor);
    includePen.setWidth(3);
    painter.setPen(includePen);
    QList<QRect> localIncludeRects;
    for (const QRect &r : includeRegions) {
        const QRect local = r.translated(-windowBounds.topLeft()).adjusted(1, 1, -2, -2);
        painter.drawRect(local);
        localIncludeRects << local;
    }

    QPen excludePen(excludeColor);
    excludePen.setWidth(2);
    excludePen.setStyle(Qt::DashLine);
    painter.setPen(excludePen);
    QList<QRect> localExcludeRects;
    for (const QRect &r : excludeRegions) {
        const QRect local = r.translated(-windowBounds.topLeft()).adjusted(1, 1, -2, -2);
        painter.drawRect(local);
        localExcludeRects << local;
    }

    // Labels drawn after all rectangles so they sit on top of overlapping
    // include/exclude borders rather than getting drawn over by them.
    for (const QRect &local : localIncludeRects)
        drawRegionLabel(painter, local, regionName, includeColor);
    for (const QRect &local : localExcludeRects)
        drawRegionLabel(painter, local, I18n::t(QStringLiteral("除外")), excludeColor);

    return shot;
}

bool RandomActionEngine::pointExcluded(const QPoint &pt, const QList<QRect> &excludeRegions) const
{
    for (const QRect &r : excludeRegions) {
        if (r.contains(pt))
            return true;
    }
    return false;
}

QPoint RandomActionEngine::pickRandomPoint(const QList<QRect> &includeRegions,
                                            const QList<QRect> &excludeRegions, bool &ok)
{
    ok = false;
    qint64 totalArea = 0;
    for (const QRect &r : includeRegions)
        totalArea += qint64(qMax(1, r.width())) * qint64(qMax(1, r.height()));
    if (totalArea <= 0)
        return {};

    constexpr int kMaxAttempts = 30;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        qint64 target = qint64(m_rng.generateDouble() * double(totalArea));
        const QRect *chosen = nullptr;
        for (const QRect &r : includeRegions) {
            const qint64 area = qint64(qMax(1, r.width())) * qint64(qMax(1, r.height()));
            if (target < area) {
                chosen = &r;
                break;
            }
            target -= area;
        }
        if (!chosen)
            chosen = &includeRegions.last();

        const QPoint pt(chosen->x() + int(m_rng.bounded(quint32(qMax(1, chosen->width())))),
                         chosen->y() + int(m_rng.bounded(quint32(qMax(1, chosen->height())))));
        if (!pointExcluded(pt, excludeRegions)) {
            ok = true;
            return pt;
        }
    }
    return {};
}

const ActionParams &RandomActionEngine::effectiveParams(const RegionStep &step) const
{
    return step.useDefaultActionParams ? m_config.defaultActionParams : step.customActionParams;
}

RandomActionEngine::ActionKind RandomActionEngine::pickWeightedActionKind(const RegionStep &step)
{
    struct Entry
    {
        ActionKind kind;
        int weight;
    };
    QList<Entry> entries;
    if (step.enableClick)
        entries << Entry{ActionKind::Click, qMax(1, step.clickWeight)};
    if (step.enableDoubleClick)
        entries << Entry{ActionKind::DoubleClick, qMax(1, step.doubleClickWeight)};
    if (step.enableDrag)
        entries << Entry{ActionKind::Drag, qMax(1, step.dragWeight)};
    if (step.enableKey)
        entries << Entry{ActionKind::Key, qMax(1, step.keyWeight)};
    if (step.enableScrollUp)
        entries << Entry{ActionKind::ScrollUp, qMax(1, step.scrollUpWeight)};
    if (step.enableScrollDown)
        entries << Entry{ActionKind::ScrollDown, qMax(1, step.scrollDownWeight)};
    if (step.enableScrollHorizontal)
        entries << Entry{ActionKind::ScrollHorizontal, qMax(1, step.scrollHorizontalWeight)};
    if (step.enableShortcut)
        entries << Entry{ActionKind::Shortcut, qMax(1, step.shortcutWeight)};
    // Window-level operations always act on the main target window
    // (m_config.targetWindowId), never on whatever this step's region
    // resolved to -- meaningless (and liable to act on the wrong window)
    // for a member that targets a popup dialog instead. See
    // RegionStep::targetsPopupDialog.
    if (step.enableWindowOp && !step.targetsPopupDialog)
        entries << Entry{ActionKind::WindowOp, qMax(1, step.windowOpWeight)};

    int total = 0;
    for (const Entry &e : entries)
        total += e.weight;

    int target = total > 0 ? int(m_rng.bounded(quint32(total))) : 0;
    for (const Entry &e : entries) {
        if (target < e.weight)
            return e.kind;
        target -= e.weight;
    }
    return entries.isEmpty() ? ActionKind::Click : entries.last().kind;
}

void RandomActionEngine::scheduleNext()
{
    const int lo = qMin(m_config.minIntervalMs, m_config.maxIntervalMs);
    const int hi = qMax(m_config.minIntervalMs, m_config.maxIntervalMs);
    const int interval = lo + (hi > lo ? int(m_rng.bounded(quint32(hi - lo + 1))) : 0);
    m_timer.start(qMax(1, interval));
}

void RandomActionEngine::advanceToNextStep()
{
    m_currentStepActionsDone = 0;
    m_currentTaskMemberIndex = 0;
    m_popupDialogWaitStrikes = 0;
    m_currentStepIndex = (m_currentStepIndex + 1) % m_config.steps.size();
    if (m_currentStepIndex == 0) {
        ++m_sequenceLoopCount;
        emit logMessage(I18n::t(QStringLiteral("シーケンス %1 回目の実行を開始します")).arg(m_sequenceLoopCount + 1));
    }
    emit logMessage(I18n::t(QStringLiteral("ステップ %1 へ移行します")).arg(m_currentStepIndex + 1));
    emit currentStepChanged(m_currentStepIndex);
}

bool RandomActionEngine::handlePossibleContextMenu(const ActionParams &params, QString &desc)
{
    // A right-button action (click or drag) may have opened a native
    // context/popup menu, regardless of whether enableContextMenuSelection
    // is on -- that setting only controls whether we try to pick an item
    // out of it, not whether one can appear. Always resolve it (select an
    // item, or dismiss it) before moving on: leaving an unhandled menu open
    // makes every subsequent random action land on/interact with that menu
    // instead of the app, which looks like the whole test "freezing" on a
    // stuck menu.
    QThread::msleep(150);  // let the menu render before introspecting/dismissing it

    // Re-check focus before touching the menu: the introspection below has
    // no way to confirm *whose* menu it found (it just looks at whatever is
    // topmost/visible), so if some other window grabbed focus in the last
    // 150ms, clicking into "the" menu (or even just sending Escape) could
    // affect a different app entirely.
    if (PlatformAutomation::activeProcessPid() != m_config.targetPid) {
        doStop(I18n::t(QStringLiteral("メニュー選択の直前に対象アプリがアクティブでなくなったため、安全のため"
                   "テストを停止しました")),
               /*isAnomaly=*/true);
        return true;
    }

    bool selected = false;
    if (params.enableContextMenuSelection) {
        const QStringList openItems = PlatformAutomation::listOpenContextMenuItems(m_config.targetPid);

        if (params.contextMenuSelectionMode == ContextMenuSelectionMode::ByName) {
            QStringList matches;
            for (const QString &name : openItems) {
                if (params.contextMenuItemNames.contains(name))
                    matches.append(name);
            }
            if (!matches.isEmpty()) {
                const QString chosen = matches[int(m_rng.bounded(quint32(matches.size())))];
                selected = PlatformAutomation::clickContextMenuItem(chosen, m_config.targetPid);
                if (selected)
                    desc += I18n::t(QStringLiteral(" → メニュー項目「%1」を選択")).arg(chosen);
            }
        } else {  // ByIndex
            QList<int> validIndices;
            for (int oneBased : params.contextMenuIndices) {
                const int zeroBased = oneBased - 1;
                if (zeroBased >= 0 && zeroBased < openItems.size())
                    validIndices.append(zeroBased);
            }
            if (!validIndices.isEmpty()) {
                const int chosen = validIndices[int(m_rng.bounded(quint32(validIndices.size())))];
                selected = PlatformAutomation::clickContextMenuItemAt(chosen, m_config.targetPid);
                if (selected)
                    desc += I18n::t(QStringLiteral(" → メニュー項目(上から%1番目)を選択")).arg(chosen + 1);
            }
        }
    }
    // No selectable item was found (or menu-item selection isn't enabled at
    // all) -- immediately close whatever menu might be open so the next
    // random action isn't swallowed by it.
    if (!selected)
        PlatformAutomation::dismissContextMenu();
    return false;
}

namespace
{
// After this many consecutive ticks with an unexpected window still open
// despite Escape attempts to dismiss it, give up and stop the run rather
// than keep burning the iteration/time budget on a window that will never
// go away on its own (see SPEC.md 10 -- this is exactly the "~490 wasted
// clicks" scenario found during v0.40's manual testing).
constexpr int kMaxUnexpectedWindowStrikes = 5;

// Symmetric counterpart for a task member with targetsPopupDialog set
// (RegionStep::targetsPopupDialog, SPEC.md 6.2追加実装及び修正依頼): after
// this many consecutive ticks with no extra window found yet to operate
// on, give up and stop the run rather than retry forever -- a dialog that
// never appears at all is just as worth surfacing as one that never
// closes. Higher than kMaxUnexpectedWindowStrikes above since a dialog can
// legitimately take a moment to render (icons/layout), whereas that retry
// loop is actively working to dismiss something already on screen.
constexpr int kMaxPopupDialogWaitStrikes = 50;
}  // namespace

bool RandomActionEngine::currentActionTargetsPopupDialog() const
{
    if (m_inSetupPhase || m_currentStepIndex < 0 || m_currentStepIndex >= m_config.steps.size())
        return false;
    const RegionStep &step = m_config.steps[m_currentStepIndex];
    if (!step.isTask || m_currentTaskMemberIndex < 0 ||
        m_currentTaskMemberIndex >= step.taskMembers.size())
        return false;
    return step.taskMembers[m_currentTaskMemberIndex].targetsPopupDialog;
}

bool RandomActionEngine::handleUnexpectedWindows()
{
    if (m_config.targetWindowId == 0)
        return false;

    const QList<quint32> windowIds = PlatformAutomation::listWindowIdsForPid(m_config.targetPid);
    bool hasExtra = false;
    for (quint32 id : windowIds) {
        if (id != m_config.targetWindowId) {
            hasExtra = true;
            break;
        }
    }

    if (!hasExtra) {
        m_unexpectedWindowStrikes = 0;
        return false;
    }

    ++m_unexpectedWindowStrikes;
    if (m_unexpectedWindowStrikes >= kMaxUnexpectedWindowStrikes) {
        doStop(I18n::t(QStringLiteral("対象アプリに予期しないウィンドウ（ダイアログ等）が開いたまま閉じられない"
                              "ため、安全のためテストを停止しました")),
               /*isAnomaly=*/true);
        return true;
    }

    emit logMessage(I18n::t(QStringLiteral("対象アプリに予期しないウィンドウ（ダイアログ等）を検出しました。Escapeで閉じてみます"
        "（%1/%2回目）"))
                         .arg(m_unexpectedWindowStrikes)
                         .arg(kMaxUnexpectedWindowStrikes));
    PlatformAutomation::dismissContextMenu();  // generic "send Escape", not menu-specific
    scheduleNext();
    return true;
}

void RandomActionEngine::performRandomAction()
{
    if (!m_running || m_paused)
        return;

    if (m_config.maxDurationSec > 0 &&
        (m_pausedElapsedMs + m_elapsed.elapsed()) / 1000 >= m_config.maxDurationSec) {
        doStop(I18n::t(QStringLiteral("時間制限に達したため停止しました")));
        return;
    }
    if (m_config.maxIterations > 0 && m_iterationCount >= m_config.maxIterations) {
        doStop(I18n::t(QStringLiteral("回数制限に達したため停止しました")));
        return;
    }
    if (m_config.maxSequenceLoops > 0 && m_sequenceLoopCount >= m_config.maxSequenceLoops) {
        doStop(I18n::t(QStringLiteral("シーケンスの繰り返し回数の上限に達したため停止しました")));
        return;
    }
    if (!PlatformAutomation::isProcessRunning(m_config.targetPid)) {
        const QString stepLabel = I18n::t(QStringLiteral("ステップ %1")).arg(m_currentStepIndex + 1);
        doStop(I18n::t(QStringLiteral("%1の実行中に対象アプリケーションの異常終了（クラッシュ）を検知したため停止しました"))
                   .arg(stepLabel),
               /*isAnomaly=*/true, /*targetCrashed=*/true);
        return;
    }
    if (!currentActionTargetsPopupDialog() && handleUnexpectedWindows())
        return;

    if (m_inSetupPhase) {
        performSetupAction();
        return;
    }

    if (m_config.steps.isEmpty()) {
        doStop(I18n::t(QStringLiteral("ステップが設定されていません")));
        return;
    }

    const RegionStep &step = m_config.steps[m_currentStepIndex];

    if (step.isWaitStep) {
        // A pure pause: no region/action-kind fields on this step are
        // meaningful. Doesn't count as an "action" (no iteration-count
        // increment), and the wait itself replaces the usual randomized
        // scheduleNext() delay before the next step's first action.
        emit logMessage(
            I18n::t(QStringLiteral("ステップ %1: %2 ms 待機します")).arg(m_currentStepIndex + 1).arg(step.waitDurationMs));
        advanceToNextStep();
        m_timer.start(qMax(1, step.waitDurationMs));
        return;
    }

    if (step.isGroup) {
        performGroupAction(step);
        return;
    }

    if (step.isTask) {
        performTaskAction(step);
        return;
    }

    QString desc;
    ActionKind kind;
    const ActionOutcome outcome =
        runOneAction(step, I18n::t(QStringLiteral("ステップ %1")).arg(m_currentStepIndex + 1), desc, kind);
    if (outcome == ActionOutcome::StoppedEngine)
        return;
    if (outcome == ActionOutcome::SkippedNoCount) {
        scheduleNext();
        return;
    }

    ++m_iterationCount;
    ++m_currentStepActionsDone;
    ++m_actionKindCounts[kind];
    recordRecentAction(desc);
    emit actionPerformed(desc);
    emit logMessage(desc);
    emit iterationCountChanged(m_iterationCount);

    if (m_currentStepActionsDone >= step.actionCount)
        advanceToNextStep();

    scheduleNext();
}

void RandomActionEngine::performGroupAction(const RegionStep &group)
{
    if (group.groupMembers.isEmpty()) {
        doStop(I18n::t(QStringLiteral("ステップ %1（グループ）にステップが登録されていません")).arg(m_currentStepIndex + 1));
        return;
    }

    const int memberIndex = pickWeightedGroupMemberIndex(group);
    const RegionStep &member = group.groupMembers[memberIndex];
    const QString label = I18n::t(QStringLiteral("ステップ %1（グループ内メンバー %2）"))
                               .arg(m_currentStepIndex + 1)
                               .arg(memberIndex + 1);

    QString desc;
    ActionKind kind;
    const ActionOutcome outcome = runOneAction(member, label, desc, kind);
    if (outcome == ActionOutcome::StoppedEngine)
        return;
    if (outcome == ActionOutcome::SkippedNoCount) {
        scheduleNext();
        return;
    }

    ++m_iterationCount;
    ++m_currentStepActionsDone;
    ++m_actionKindCounts[kind];
    recordRecentAction(desc);
    emit actionPerformed(desc);
    emit logMessage(desc);
    emit iterationCountChanged(m_iterationCount);

    if (m_currentStepActionsDone >= group.groupTotalCallCount)
        advanceToNextStep();

    scheduleNext();
}

void RandomActionEngine::performTaskAction(const RegionStep &task)
{
    if (task.taskMembers.isEmpty()) {
        doStop(I18n::t(QStringLiteral("ステップ %1（タスク）にステップが登録されていません")).arg(m_currentStepIndex + 1));
        return;
    }

    const RegionStep &member = task.taskMembers[m_currentTaskMemberIndex];
    const QString label = I18n::t(QStringLiteral("ステップ %1（タスク内操作 %2/%3）"))
                               .arg(m_currentStepIndex + 1)
                               .arg(m_currentTaskMemberIndex + 1)
                               .arg(task.taskMembers.size());

    QString desc;
    ActionKind kind;
    const ActionOutcome outcome = runOneAction(member, label, desc, kind);
    if (outcome == ActionOutcome::StoppedEngine)
        return;
    if (outcome == ActionOutcome::SkippedNoCount) {
        // Retry the same member next tick rather than skipping ahead --
        // matches a normal step's own behavior when an action is skipped
        // (see performRandomAction()), and keeps a task's fixed order
        // intact (an entry that's momentarily unpickable still gets its
        // turn once it becomes pickable again, instead of being silently
        // dropped from this pass).
        scheduleNext();
        return;
    }

    ++m_actionKindCounts[kind];
    recordRecentAction(desc);
    emit actionPerformed(desc);
    emit logMessage(desc);
    // Deliberately not incrementing m_iterationCount / emitting
    // iterationCountChanged() per member -- see RegionStep::isTask: the
    // whole task counts as exactly one action, credited only once the
    // full pass below completes.

    ++m_currentTaskMemberIndex;
    if (m_currentTaskMemberIndex >= task.taskMembers.size()) {
        ++m_iterationCount;
        emit iterationCountChanged(m_iterationCount);
        advanceToNextStep();  // resets m_currentTaskMemberIndex back to 0
    }

    scheduleNext();
}

int RandomActionEngine::pickWeightedGroupMemberIndex(const RegionStep &group)
{
    int total = 0;
    for (const RegionStep &m : group.groupMembers)
        total += qMax(1, m.groupWeight);
    int target = total > 0 ? int(m_rng.bounded(quint32(total))) : 0;
    for (int i = 0; i < group.groupMembers.size(); ++i) {
        const int w = qMax(1, group.groupMembers[i].groupWeight);
        if (target < w)
            return i;
        target -= w;
    }
    return group.groupMembers.size() - 1;
}

RandomActionEngine::ActionOutcome RandomActionEngine::runOneAction(const RegionStep &step,
                                                                    const QString &stepLabel,
                                                                    QString &outDesc, ActionKind &outKind)
{
    const ActionParams &params = effectiveParams(step);

    QList<QRect> includeRegions;
    QList<QRect> excludeRegions;
    if (!resolveStepRegion(step, includeRegions, excludeRegions) || includeRegions.isEmpty()) {
        if (step.targetsPopupDialog) {
            // The dialog this member expects to operate on may simply not
            // have opened yet (its preceding task member's action might
            // still be in flight, or the target app might just be slow to
            // show it) -- retry rather than fail immediately, but only up
            // to kMaxPopupDialogWaitStrikes attempts, mirroring
            // handleUnexpectedWindows()'s symmetric give-up-and-report
            // behavior: a dialog that never shows up at all is just as
            // legitimate a sign of a target-app bug as one that never
            // closes.
            ++m_popupDialogWaitStrikes;
            if (m_popupDialogWaitStrikes >= kMaxPopupDialogWaitStrikes) {
                doStop(I18n::t(QStringLiteral("%1が対象とする新しいウィンドウ（ダイアログ等）が現れないため、"
                                      "安全のためテストを停止しました"))
                           .arg(stepLabel),
                       /*isAnomaly=*/true);
                return ActionOutcome::StoppedEngine;
            }
            return ActionOutcome::SkippedNoCount;
        }
        doStop(I18n::t(QStringLiteral("%1の対象領域が見つからないため停止しました（対象ウィンドウが"
                              "消失した、または参照している操作領域が削除された可能性があります）"))
                   .arg(stepLabel),
               /*isAnomaly=*/true);
        return ActionOutcome::StoppedEngine;
    }
    if (step.targetsPopupDialog)
        m_popupDialogWaitStrikes = 0;

    const QString regionName =
        step.targetsPopupDialog ? I18n::t(QStringLiteral("新しく出現したダイアログ"))
        : step.useWholeWindow   ? I18n::t(QStringLiteral("対象GUIの全領域"))
                                 : step.regionName;
    maybeCaptureRegionScreenshot(stepLabel, regionName, includeRegions, excludeRegions);

    if (m_config.keepTargetActive)
        PlatformAutomation::activateProcess(m_config.targetPid);

    if (!step.hasAnyActionEnabled()) {
        doStop(I18n::t(QStringLiteral("%1に有効な操作がありません")).arg(stepLabel));
        return ActionOutcome::StoppedEngine;
    }
    const ActionKind kind = pickWeightedActionKind(step);

    bool ok = false;
    const QPoint pt = pickRandomPoint(includeRegions, excludeRegions, ok);
    const bool kindNeedsPoint = kind == ActionKind::Click || kind == ActionKind::DoubleClick ||
                                 kind == ActionKind::Drag || kind == ActionKind::ScrollUp ||
                                 kind == ActionKind::ScrollDown || kind == ActionKind::ScrollHorizontal;
    if (!ok && kindNeedsPoint) {
        emit logMessage(
            I18n::t(QStringLiteral("有効な座標が見つかりませんでした（除外領域が広すぎる可能性があります）")));
        return ActionOutcome::SkippedNoCount;
    }

    // Safety net: right before actually dispatching anything, confirm it
    // will land on the intended target and not some other window/app --
    // e.g. because the target moved, was covered by another window, lost
    // focus, or closed. Fail closed (stop the whole run) on any mismatch
    // or when it can't be positively confirmed -- see SPEC.md 6.7.
    if (kindNeedsPoint) {
        if (PlatformAutomation::windowPidAtPoint(pt) != m_config.targetPid) {
            doStop(I18n::t(QStringLiteral("対象アプリ以外のウィンドウを操作しそうになったため、安全のためテストを停止しました")),
                   /*isAnomaly=*/true);
            return ActionOutcome::StoppedEngine;
        }
    } else if (kind == ActionKind::Key || kind == ActionKind::Shortcut) {
        if (PlatformAutomation::activeProcessPid() != m_config.targetPid) {
            doStop(I18n::t(QStringLiteral("対象アプリがアクティブでないため（キー入力が他アプリに送られる可能性があるため）、"
                       "安全のためテストを停止しました")),
                   /*isAnomaly=*/true);
            return ActionOutcome::StoppedEngine;
        }
    } else {  // WindowOp: identity is checked directly by pid+windowId below
    }

    QString desc;
    switch (kind) {
    case ActionKind::Click: {
        QList<Qt::MouseButton> buttons;
        if (step.enableLeftClick)
            buttons << Qt::LeftButton;
        if (step.enableRightClick)
            buttons << Qt::RightButton;
        if (buttons.isEmpty())
            buttons << Qt::LeftButton;
        const Qt::MouseButton btn = buttons[m_rng.bounded(quint32(buttons.size()))];

        PlatformAutomation::mouseClick(pt, btn);
        desc = I18n::t(QStringLiteral("クリック(%1) at (%2, %3)"))
                   .arg(btn == Qt::RightButton ? I18n::t(QStringLiteral("右")) : I18n::t(QStringLiteral("左")))
                   .arg(pt.x())
                   .arg(pt.y());

        if (btn == Qt::RightButton) {
            if (handlePossibleContextMenu(params, desc))
                return ActionOutcome::StoppedEngine;
        }
        break;
    }
    case ActionKind::DoubleClick: {
        // Always the left button -- see the comment on
        // RegionStep::enableDoubleClick.
        PlatformAutomation::mouseClick(pt, Qt::LeftButton);
        QThread::msleep(80);
        PlatformAutomation::mouseClick(pt, Qt::LeftButton);
        desc = I18n::t(QStringLiteral("ダブルクリック at (%1, %2)")).arg(pt.x()).arg(pt.y());
        break;
    }
    case ActionKind::Drag: {
        const int lo = qMin(params.dragMinDistance, params.dragMaxDistance);
        const int hi = qMax(params.dragMinDistance, params.dragMaxDistance);
        const int dist = lo + (hi > lo ? int(m_rng.bounded(quint32(hi - lo + 1))) : 0);

        int dx = 0, dy = 0;
        switch (params.dragDirection) {
        case DragDirectionMode::Up: dx = 0; dy = -dist; break;
        case DragDirectionMode::Down: dx = 0; dy = dist; break;
        case DragDirectionMode::Left: dx = -dist; dy = 0; break;
        case DragDirectionMode::Right: dx = dist; dy = 0; break;
        case DragDirectionMode::Random:
        default: {
            const double angle = m_rng.generateDouble() * 2.0 * M_PI;
            dx = qRound(std::cos(angle) * dist);
            dy = qRound(std::sin(angle) * dist);
            break;
        }
        }

        QRect bounds = includeRegions.first();
        for (const QRect &r : includeRegions)
            bounds = bounds.united(r);

        QPoint to(pt.x() + dx, pt.y() + dy);
        to.setX(qBound(bounds.left(), to.x(), bounds.right()));
        to.setY(qBound(bounds.top(), to.y(), bounds.bottom()));

        if (PlatformAutomation::windowPidAtPoint(to) != m_config.targetPid) {
            doStop(I18n::t(QStringLiteral("ドラッグ先が対象アプリ以外のウィンドウになりそうなため、安全のためテストを停止しました")),
                   /*isAnomaly=*/true);
            return ActionOutcome::StoppedEngine;
        }

        const Qt::MouseButton btn =
            (step.enableRightClick && m_rng.bounded(2u) == 0) ? Qt::RightButton : Qt::LeftButton;
        PlatformAutomation::mouseDrag(pt, to, btn, 12);
        desc = I18n::t(QStringLiteral("ドラッグ (%1, %2) → (%3, %4)")).arg(pt.x()).arg(pt.y()).arg(to.x()).arg(to.y());

        if (btn == Qt::RightButton) {
            // A right-button drag can pop the same kind of context/popup
            // menu a plain right click does -- resolve it the same way
            // Click does, otherwise it's left open for later actions to
            // land on (see the comment in handlePossibleContextMenu()).
            if (handlePossibleContextMenu(params, desc))
                return ActionOutcome::StoppedEngine;
        }
        break;
    }
    case ActionKind::Key: {
        const QString &chars = params.allowedKeyChars;
        QList<Qt::Key> namedKeys;
        if (params.keyIncludeTab)
            namedKeys << Qt::Key_Tab;
        if (params.keyIncludeReturn)
            namedKeys << Qt::Key_Return;
        if (params.keyIncludeEscape)
            namedKeys << Qt::Key_Escape;
        if (params.keyIncludeBackspace)
            namedKeys << Qt::Key_Backspace;
        if (params.keyIncludeDelete)
            namedKeys << Qt::Key_Delete;
        if (params.keyIncludeArrowKeys)
            namedKeys << Qt::Key_Left << Qt::Key_Right << Qt::Key_Up << Qt::Key_Down;

        const int totalPool = chars.size() + namedKeys.size();
        if (totalPool == 0) {
            emit logMessage(
                I18n::t(QStringLiteral("キー入力の候補がありません（使用文字・名前付きキーのいずれも未設定）")));
            return ActionOutcome::SkippedNoCount;
        }
        const int index = int(m_rng.bounded(quint32(totalPool)));
        if (index < chars.size()) {
            const QChar ch = chars[index];
            PlatformAutomation::keyTap(ch);
            desc = I18n::t(QStringLiteral("キー入力 '%1'")).arg(ch);
        } else {
            const Qt::Key namedKey = namedKeys[index - chars.size()];
            PlatformAutomation::keyTapNamed(namedKey);
            desc = I18n::t(QStringLiteral("キー入力 [%1]")).arg(QKeySequence(namedKey).toString());
        }
        break;
    }
    case ActionKind::ScrollUp: {
        const int lo = qMin(params.scrollUpMinAmount, params.scrollUpMaxAmount);
        const int hi = qMax(params.scrollUpMinAmount, params.scrollUpMaxAmount);
        const int amount = qMax(1, lo + (hi > lo ? int(m_rng.bounded(quint32(hi - lo + 1))) : 0));
        const int dy = amount;  // positive == scroll up, matching both backends' wheel-event convention
        PlatformAutomation::scroll(pt, 0, dy);
        desc = I18n::t(QStringLiteral("スクロール(上) at (%1, %2) dy=%3")).arg(pt.x()).arg(pt.y()).arg(dy);
        break;
    }
    case ActionKind::ScrollDown: {
        const int lo = qMin(params.scrollDownMinAmount, params.scrollDownMaxAmount);
        const int hi = qMax(params.scrollDownMinAmount, params.scrollDownMaxAmount);
        const int amount = qMax(1, lo + (hi > lo ? int(m_rng.bounded(quint32(hi - lo + 1))) : 0));
        const int dy = -amount;  // negative == scroll down
        PlatformAutomation::scroll(pt, 0, dy);
        desc = I18n::t(QStringLiteral("スクロール(下) at (%1, %2) dy=%3")).arg(pt.x()).arg(pt.y()).arg(dy);
        break;
    }
    case ActionKind::ScrollHorizontal: {
        const int lo = qMin(params.scrollHorizontalMinAmount, params.scrollHorizontalMaxAmount);
        const int hi = qMax(params.scrollHorizontalMinAmount, params.scrollHorizontalMaxAmount);
        const int amount = qMax(1, lo + (hi > lo ? int(m_rng.bounded(quint32(hi - lo + 1))) : 0));
        const int dx = m_rng.bounded(2u) == 0 ? amount : -amount;  // random left/right each time
        PlatformAutomation::scroll(pt, dx, 0);
        desc = I18n::t(QStringLiteral("スクロール(横) at (%1, %2) dx=%3")).arg(pt.x()).arg(pt.y()).arg(dx);
        break;
    }
    case ActionKind::Shortcut: {
        if (params.shortcutSequences.isEmpty()) {
            emit logMessage(I18n::t(QStringLiteral("ショートカットが設定されていません")));
            return ActionOutcome::SkippedNoCount;
        }
        const QString seqText =
            params.shortcutSequences[int(m_rng.bounded(quint32(params.shortcutSequences.size())))];
        Qt::Key key = Qt::Key(0);
        Qt::KeyboardModifiers mods;
        if (!parseShortcut(seqText, key, mods)) {
            emit logMessage(I18n::t(QStringLiteral("ショートカット '%1' を解釈できませんでした")).arg(seqText));
            return ActionOutcome::SkippedNoCount;
        }
        if (m_config.keepTargetActive)
            PlatformAutomation::activateProcess(m_config.targetPid);
        PlatformAutomation::keyShortcut(key, mods);
        desc = I18n::t(QStringLiteral("ショートカット '%1'")).arg(seqText);
        break;
    }
    case ActionKind::WindowOp: {
        QRect currentBounds;
        if (!PlatformAutomation::queryWindowBounds(m_config.targetWindowId, m_config.targetPid,
                                                     currentBounds)) {
            doStop(I18n::t(QStringLiteral("対象ウィンドウが見つからないため停止しました")), /*isAnomaly=*/true);
            return ActionOutcome::StoppedEngine;
        }

        QStringList opNames;
        if (params.windowOpMove)
            opNames << QStringLiteral("move");
        if (params.windowOpResize)
            opNames << QStringLiteral("resize");
        if (params.windowOpMinimize)
            opNames << QStringLiteral("minimize");
        if (params.windowOpMaximize)
            opNames << QStringLiteral("maximize");
        if (opNames.isEmpty()) {
            emit logMessage(I18n::t(QStringLiteral("ウィンドウ操作の種類が選択されていません")));
            return ActionOutcome::SkippedNoCount;
        }
        const QString op = opNames[int(m_rng.bounded(quint32(opNames.size())))];

        QScreen *screen = QGuiApplication::screenAt(currentBounds.center());
        if (!screen && !QGuiApplication::screens().isEmpty())
            screen = QGuiApplication::screens().first();
        const QRect screenGeom = screen ? screen->availableGeometry() : currentBounds;

        if (op == QStringLiteral("move")) {
            const int minX = screenGeom.left();
            const int maxX = qMax(minX, screenGeom.right() - currentBounds.width());
            const int minY = screenGeom.top();
            const int maxY = qMax(minY, screenGeom.bottom() - currentBounds.height());
            const QPoint newPos(minX + int(m_rng.bounded(quint32(qMax(1, maxX - minX + 1)))),
                                 minY + int(m_rng.bounded(quint32(qMax(1, maxY - minY + 1)))));
            PlatformAutomation::moveWindow(m_config.targetPid, m_config.targetWindowId, newPos);
            desc = I18n::t(QStringLiteral("ウィンドウ移動 → (%1, %2)")).arg(newPos.x()).arg(newPos.y());
        } else if (op == QStringLiteral("resize")) {
            constexpr int kMinW = 200, kMinH = 150;
            const int maxW = qMax(kMinW, screenGeom.width());
            const int maxH = qMax(kMinH, screenGeom.height());
            const QSize newSize(kMinW + int(m_rng.bounded(quint32(qMax(1, maxW - kMinW + 1)))),
                                 kMinH + int(m_rng.bounded(quint32(qMax(1, maxH - kMinH + 1)))));
            PlatformAutomation::resizeWindow(m_config.targetPid, m_config.targetWindowId, newSize);
            desc = I18n::t(QStringLiteral("ウィンドウリサイズ → %1 x %2")).arg(newSize.width()).arg(newSize.height());
        } else if (op == QStringLiteral("minimize")) {
            PlatformAutomation::minimizeWindow(m_config.targetPid, m_config.targetWindowId);
            desc = I18n::t(QStringLiteral("ウィンドウを最小化"));
        } else {  // maximize
            PlatformAutomation::maximizeWindow(m_config.targetPid, m_config.targetWindowId);
            desc = I18n::t(QStringLiteral("ウィンドウを最大化"));
        }
        break;
    }
    }

    // For point-based actions, append the target-window-relative coordinate
    // (screen coordinates alone mean little once the window has moved from
    // where it was during this run) and, best-effort, the name of whatever
    // UI element was actually at that point -- both purely to make the log
    // more useful for reproducing/diagnosing a bug later (SPEC.md 6.8/10);
    // neither affects what was actually dispatched above. Based on `pt`
    // (drag's starting point) even for Drag, since that's what was clicked
    // down on.
    if (kindNeedsPoint) {
        QRect windowBounds;
        if (PlatformAutomation::queryWindowBounds(m_config.targetWindowId, m_config.targetPid, windowBounds)) {
            const QPoint rel = pt - windowBounds.topLeft();
            desc += I18n::t(QStringLiteral(" [対象ウィンドウ相対: (%1, %2)]")).arg(rel.x()).arg(rel.y());
        }
        const QString widgetName = PlatformAutomation::accessibleNameAtPoint(pt);
        if (!widgetName.isEmpty())
            desc += QStringLiteral(" [%1]").arg(widgetName);
    }

    outDesc = desc;
    outKind = kind;
    return ActionOutcome::Performed;
}

void RandomActionEngine::performSetupAction()
{
    if (m_setupActionIndex >= m_config.setupActions.size()) {
        // Setup phase complete -- hand off to the normal randomized loop.
        m_inSetupPhase = false;
        emit logMessage(I18n::t(QStringLiteral("起動時セットアップが完了しました。ランダム操作を開始します")));
        scheduleNext();
        return;
    }

    const SetupAction &action = m_config.setupActions[m_setupActionIndex];
    QString desc;
    if (!trySetupAction(action, desc))
        return;  // doStop() was called, or a retry was already scheduled

    emit logMessage(I18n::t(QStringLiteral("起動時セットアップ %1/%2: %3"))
                         .arg(m_setupActionIndex + 1)
                         .arg(m_config.setupActions.size())
                         .arg(desc));
    emit actionPerformed(desc);
    recordRecentAction(desc);

    ++m_setupActionIndex;
    m_setupSafetyRetryCount = 0;

    // A Wait action's own waitMs replaces the usual randomized scheduling
    // delay, mirroring how a wait *step* behaves in the main loop (see
    // isWaitStep above in performRandomAction()).
    if (action.type == SetupActionType::Wait)
        m_timer.start(qMax(1, action.waitMs));
    else
        scheduleNext();
}

bool RandomActionEngine::retrySetupOrFail(const QString &stepLabel, const QString &reason)
{
    ++m_setupSafetyRetryCount;
    if (m_setupSafetyRetryCount > kMaxSetupSafetyRetries) {
        doStop(I18n::t(QStringLiteral("%1に失敗しました（%2）。%3回再試行しましたが解決しなかったため、"
                              "安全のためテストを停止しました"))
                   .arg(stepLabel, reason)
                   .arg(kMaxSetupSafetyRetries),
               /*isAnomaly=*/true);
        return false;
    }
    emit logMessage(I18n::t(QStringLiteral("%1を再試行します（%2、%3/%4回目）"))
                         .arg(stepLabel, reason)
                         .arg(m_setupSafetyRetryCount)
                         .arg(kMaxSetupSafetyRetries));
    m_timer.start(kSetupSafetyRetryDelayMs);
    return false;
}

bool RandomActionEngine::trySetupAction(const SetupAction &action, QString &outDesc)
{
    const QString stepLabel = I18n::t(QStringLiteral("起動時セットアップ %1/%2"))
                                   .arg(m_setupActionIndex + 1)
                                   .arg(m_config.setupActions.size());
    const QString labelSuffix =
        action.label.isEmpty() ? QString() : I18n::t(QStringLiteral("（%1）")).arg(action.label);

    if (action.type == SetupActionType::Wait) {
        outDesc = I18n::t(QStringLiteral("待機 %1ms")).arg(action.waitMs) + labelSuffix;
        return true;
    }

    if (action.type == SetupActionType::TypeText || action.type == SetupActionType::KeyPress) {
        if (m_config.keepTargetActive)
            PlatformAutomation::activateProcess(m_config.targetPid);
        // Same fail-closed rationale as runOneAction()'s Key/Shortcut check:
        // don't send keyboard input anywhere unless the target is
        // positively confirmed to be the active process right now.
        if (PlatformAutomation::activeProcessPid() != m_config.targetPid)
            return retrySetupOrFail(stepLabel, I18n::t(QStringLiteral("対象アプリがアクティブになっていません")));

        if (action.type == SetupActionType::TypeText) {
            for (const QChar &ch : action.text)
                PlatformAutomation::keyTap(ch);
            // Deliberately never includes the literal text: this is the exact
            // mechanism SPEC.md's own example uses to type a login password
            // during setup, and this description is what ends up in the
            // on-screen log, the uncapped full-log file written every run,
            // and RunSummary::recentActions -- which is also bundled into the
            // anomaly diagnostic artifacts saved on a crash (SPEC.md 6.7).
            // Logging a character count is enough to confirm "text input
            // happened here" without persisting the credential in plaintext.
            outDesc = I18n::t(QStringLiteral("文字入力（%1文字、内容はログに記録しません）"))
                          .arg(action.text.size()) +
                      labelSuffix;
        } else {
            Qt::Key key = Qt::Key(0);
            Qt::KeyboardModifiers mods;
            if (!parseShortcut(action.keySequence, key, mods)) {
                doStop(I18n::t(QStringLiteral("%1: キー '%2' を解釈できないため、テストを開始できません"))
                           .arg(stepLabel, action.keySequence));
                return false;
            }
            PlatformAutomation::keyShortcut(key, mods);
            outDesc = I18n::t(QStringLiteral("キー入力 '%1'")).arg(action.keySequence) + labelSuffix;
        }
        return true;
    }

    // Click/DoubleClick/RightClick/Drag: point(s) stored relative to the
    // target window's top-left corner, resolved against its *current*
    // bounds every run (TestConfig.h's SetupAction comment) -- not a
    // one-time resolution, since the window may not have finished its
    // initial layout/positioning the first few ticks right after launch.
    QRect windowBounds;
    if (!PlatformAutomation::queryWindowBounds(m_config.targetWindowId, m_config.targetPid, windowBounds))
        return retrySetupOrFail(stepLabel, I18n::t(QStringLiteral("対象ウィンドウが見つかりません")));

    const QPoint pt = windowBounds.topLeft() + action.point;
    // Same fail-closed safety net as runOneAction(): confirm the point
    // actually lands on the target before dispatching anything to it.
    if (PlatformAutomation::windowPidAtPoint(pt) != m_config.targetPid)
        return retrySetupOrFail(stepLabel, I18n::t(QStringLiteral("指定位置に対象アプリのウィンドウが見つかりません")));

    if (m_config.keepTargetActive)
        PlatformAutomation::activateProcess(m_config.targetPid);

    switch (action.type) {
    case SetupActionType::Click:
        PlatformAutomation::mouseClick(pt, Qt::LeftButton);
        outDesc = I18n::t(QStringLiteral("クリック at (%1, %2)")).arg(pt.x()).arg(pt.y()) + labelSuffix;
        break;
    case SetupActionType::DoubleClick:
        PlatformAutomation::mouseClick(pt, Qt::LeftButton);
        QThread::msleep(80);
        PlatformAutomation::mouseClick(pt, Qt::LeftButton);
        outDesc = I18n::t(QStringLiteral("ダブルクリック at (%1, %2)")).arg(pt.x()).arg(pt.y()) + labelSuffix;
        break;
    case SetupActionType::RightClick: {
        PlatformAutomation::mouseClick(pt, Qt::RightButton);
        QString desc = I18n::t(QStringLiteral("右クリック at (%1, %2)")).arg(pt.x()).arg(pt.y());
        // A right click may open a native context/popup menu -- always
        // dismiss it (never try to select an item: setup is meant to be a
        // fixed, deterministic sequence, not a place for the same
        // randomized item-selection runOneAction() does for regular
        // steps), so it can't swallow the next setup action.
        if (handlePossibleContextMenu(ActionParams(), desc))
            return false;  // doStop() was already called
        outDesc = desc + labelSuffix;
        break;
    }
    case SetupActionType::Drag: {
        const QPoint to = windowBounds.topLeft() + action.dragToPoint;
        if (PlatformAutomation::windowPidAtPoint(to) != m_config.targetPid)
            return retrySetupOrFail(stepLabel, I18n::t(QStringLiteral("ドラッグ先に対象アプリのウィンドウが見つかりません")));
        PlatformAutomation::mouseDrag(pt, to, Qt::LeftButton, 12);
        outDesc =
            I18n::t(QStringLiteral("ドラッグ (%1, %2) → (%3, %4)")).arg(pt.x()).arg(pt.y()).arg(to.x()).arg(to.y()) +
            labelSuffix;
        break;
    }
    default:
        break;
    }

    return true;
}
