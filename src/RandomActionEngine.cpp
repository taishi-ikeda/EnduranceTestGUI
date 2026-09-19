#include "RandomActionEngine.h"

#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QKeySequence>
#include <QPixmap>
#include <QScreen>
#include <QStandardPaths>
#include <QThread>
#include <QtMath>

#include "platform/PlatformAutomation.h"

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

RandomActionEngine::RandomActionEngine(QObject *parent) : QObject(parent), m_rng(0)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &RandomActionEngine::performRandomAction);

    m_resourceTimer.setInterval(5000);
    connect(&m_resourceTimer, &QTimer::timeout, this, &RandomActionEngine::sampleResourceUsage);
}

void RandomActionEngine::start(const TestConfig &config)
{
    if (m_running)
        stop();

    m_config = config;
    m_iterationCount = 0;
    m_currentStepIndex = 0;
    m_currentStepActionsDone = 0;
    m_sequenceLoopCount = 0;
    m_pausedElapsedMs = 0;
    m_elapsed.restart();
    m_running = true;
    m_paused = false;
    m_hasCpuSample = false;

    // A seed of 0 means "pick a fresh random one" -- but 0 is also a
    // perfectly valid *explicit* seed a user might type back in to
    // reproduce a run, so generate a nonzero replacement when auto-picking.
    quint32 seed = m_config.rngSeed;
    if (seed == 0)
        seed = QRandomGenerator::global()->generate() | 1u;
    m_rng.seed(seed);
    emit logMessage(QStringLiteral("乱数シード: %1（クラッシュ等の再現に使う場合はこの値を記録してください）")
                         .arg(seed));

    if (m_config.keepTargetActive)
        PlatformAutomation::activateProcess(m_config.targetPid);

    emit logMessage(QStringLiteral("テストを開始しました（ステップ数: %1）").arg(m_config.steps.size()));
    emit iterationCountChanged(m_iterationCount);
    if (!m_config.steps.isEmpty())
        emit currentStepChanged(m_currentStepIndex);
    scheduleNext();
    m_resourceTimer.start();
}

void RandomActionEngine::stop()
{
    if (!m_running)
        return;
    doStop(QStringLiteral("ユーザーにより停止されました"));
}

void RandomActionEngine::pause()
{
    if (!m_running || m_paused)
        return;
    m_paused = true;
    m_pausedElapsedMs += m_elapsed.elapsed();
    m_timer.stop();
    m_resourceTimer.stop();
    emit pausedChanged(true);
    emit logMessage(QStringLiteral("一時停止しました"));
}

void RandomActionEngine::resume()
{
    if (!m_running || !m_paused)
        return;
    m_paused = false;
    m_elapsed.restart();
    emit pausedChanged(false);
    emit logMessage(QStringLiteral("再開しました"));
    scheduleNext();
    m_resourceTimer.start();
}

void RandomActionEngine::doStop(const QString &reason, bool isAnomaly)
{
    m_running = false;
    m_paused = false;
    m_timer.stop();
    m_resourceTimer.stop();
    if (isAnomaly)
        captureAnomalyScreenshots(reason);
    emit logMessage(reason);
    emit finished(reason);
}

void RandomActionEngine::captureAnomalyScreenshots(const QString &reason)
{
    Q_UNUSED(reason);
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) +
                             QStringLiteral("/EnduranceTestGUI_Screenshots");
    if (!QDir().mkpath(baseDir)) {
        emit logMessage(QStringLiteral("スクリーンショット保存先の作成に失敗しました: %1").arg(baseDir));
        return;
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
            QStringLiteral("異常検知時のスクリーンショットを保存しました: %1").arg(savedPaths.join(QStringLiteral(", "))));
    } else {
        emit logMessage(QStringLiteral(
            "スクリーンショットの保存に失敗しました（macOSでは画面収録の権限が必要な場合があります）"));
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
    emit logMessage(QStringLiteral("リソース使用状況: メモリ %1 MB, CPU %2%")
                         .arg(stats.residentMemoryMB, 0, 'f', 1)
                         .arg(cpuPercent, 0, 'f', 1));
}

bool RandomActionEngine::resolveStepRegion(const RegionStep &step, QList<QRect> &outIncludeRegions,
                                            QList<QRect> &outExcludeRegions)
{
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
            return true;
        }
    }
    return false;  // referenced named region no longer exists
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
    if (step.enableWindowOp)
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
    m_currentStepIndex = (m_currentStepIndex + 1) % m_config.steps.size();
    if (m_currentStepIndex == 0) {
        ++m_sequenceLoopCount;
        emit logMessage(QStringLiteral("シーケンス %1 回目の実行を開始します").arg(m_sequenceLoopCount + 1));
    }
    emit logMessage(QStringLiteral("ステップ %1 へ移行します").arg(m_currentStepIndex + 1));
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
        doStop(QStringLiteral(
                   "メニュー選択の直前に対象アプリがアクティブでなくなったため、安全のため"
                   "テストを停止しました"),
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
                    desc += QStringLiteral(" → メニュー項目「%1」を選択").arg(chosen);
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
                    desc += QStringLiteral(" → メニュー項目(上から%1番目)を選択").arg(chosen + 1);
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

void RandomActionEngine::performRandomAction()
{
    if (!m_running || m_paused)
        return;

    if (m_config.maxDurationSec > 0 &&
        (m_pausedElapsedMs + m_elapsed.elapsed()) / 1000 >= m_config.maxDurationSec) {
        doStop(QStringLiteral("時間制限に達したため停止しました"));
        return;
    }
    if (m_config.maxIterations > 0 && m_iterationCount >= m_config.maxIterations) {
        doStop(QStringLiteral("回数制限に達したため停止しました"));
        return;
    }
    if (m_config.maxSequenceLoops > 0 && m_sequenceLoopCount >= m_config.maxSequenceLoops) {
        doStop(QStringLiteral("シーケンスの繰り返し回数の上限に達したため停止しました"));
        return;
    }
    if (!PlatformAutomation::isProcessRunning(m_config.targetPid)) {
        doStop(QStringLiteral("対象アプリケーションの異常終了（クラッシュ）を検知したため停止しました"),
               /*isAnomaly=*/true);
        return;
    }
    if (m_config.steps.isEmpty()) {
        doStop(QStringLiteral("ステップが設定されていません"));
        return;
    }

    const RegionStep &step = m_config.steps[m_currentStepIndex];

    if (step.isWaitStep) {
        // A pure pause: no region/action-kind fields on this step are
        // meaningful. Doesn't count as an "action" (no iteration-count
        // increment), and the wait itself replaces the usual randomized
        // scheduleNext() delay before the next step's first action.
        emit logMessage(
            QStringLiteral("ステップ %1: %2 ms 待機します").arg(m_currentStepIndex + 1).arg(step.waitDurationMs));
        advanceToNextStep();
        m_timer.start(qMax(1, step.waitDurationMs));
        return;
    }

    const ActionParams &params = effectiveParams(step);

    QList<QRect> includeRegions;
    QList<QRect> excludeRegions;
    if (!resolveStepRegion(step, includeRegions, excludeRegions) || includeRegions.isEmpty()) {
        doStop(QStringLiteral("ステップ %1 の対象領域が見つからないため停止しました（対象ウィンドウが"
                              "消失した、または参照している操作領域が削除された可能性があります）")
                   .arg(m_currentStepIndex + 1),
               /*isAnomaly=*/true);
        return;
    }

    if (m_config.keepTargetActive)
        PlatformAutomation::activateProcess(m_config.targetPid);

    if (!step.hasAnyActionEnabled()) {
        doStop(QStringLiteral("ステップ %1 に有効な操作がありません").arg(m_currentStepIndex + 1));
        return;
    }
    const ActionKind kind = pickWeightedActionKind(step);

    bool ok = false;
    const QPoint pt = pickRandomPoint(includeRegions, excludeRegions, ok);
    const bool kindNeedsPoint = kind == ActionKind::Click || kind == ActionKind::DoubleClick ||
                                 kind == ActionKind::Drag || kind == ActionKind::ScrollUp ||
                                 kind == ActionKind::ScrollDown || kind == ActionKind::ScrollHorizontal;
    if (!ok && kindNeedsPoint) {
        emit logMessage(
            QStringLiteral("有効な座標が見つかりませんでした（除外領域が広すぎる可能性があります）"));
        scheduleNext();
        return;
    }

    // Safety net: right before actually dispatching anything, confirm it
    // will land on the intended target and not some other window/app --
    // e.g. because the target moved, was covered by another window, lost
    // focus, or closed. Fail closed (stop the whole run) on any mismatch
    // or when it can't be positively confirmed -- see SPEC.md 6.7.
    if (kindNeedsPoint) {
        if (PlatformAutomation::windowPidAtPoint(pt) != m_config.targetPid) {
            doStop(QStringLiteral(
                       "対象アプリ以外のウィンドウを操作しそうになったため、安全のためテストを停止しました"),
                   /*isAnomaly=*/true);
            return;
        }
    } else if (kind == ActionKind::Key || kind == ActionKind::Shortcut) {
        if (PlatformAutomation::activeProcessPid() != m_config.targetPid) {
            doStop(QStringLiteral(
                       "対象アプリがアクティブでないため（キー入力が他アプリに送られる可能性があるため）、"
                       "安全のためテストを停止しました"),
                   /*isAnomaly=*/true);
            return;
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
        desc = QStringLiteral("クリック(%1) at (%2, %3)")
                   .arg(btn == Qt::RightButton ? QStringLiteral("右") : QStringLiteral("左"))
                   .arg(pt.x())
                   .arg(pt.y());

        if (btn == Qt::RightButton) {
            if (handlePossibleContextMenu(params, desc))
                return;
        }
        break;
    }
    case ActionKind::DoubleClick: {
        // Always the left button -- see the comment on
        // RegionStep::enableDoubleClick.
        PlatformAutomation::mouseClick(pt, Qt::LeftButton);
        QThread::msleep(80);
        PlatformAutomation::mouseClick(pt, Qt::LeftButton);
        desc = QStringLiteral("ダブルクリック at (%1, %2)").arg(pt.x()).arg(pt.y());
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
            doStop(QStringLiteral(
                       "ドラッグ先が対象アプリ以外のウィンドウになりそうなため、安全のためテストを停止しました"),
                   /*isAnomaly=*/true);
            return;
        }

        const Qt::MouseButton btn =
            (step.enableRightClick && m_rng.bounded(2u) == 0) ? Qt::RightButton : Qt::LeftButton;
        PlatformAutomation::mouseDrag(pt, to, btn, 12);
        desc = QStringLiteral("ドラッグ (%1, %2) → (%3, %4)").arg(pt.x()).arg(pt.y()).arg(to.x()).arg(to.y());

        if (btn == Qt::RightButton) {
            // A right-button drag can pop the same kind of context/popup
            // menu a plain right click does -- resolve it the same way
            // Click does, otherwise it's left open for later actions to
            // land on (see the comment in handlePossibleContextMenu()).
            if (handlePossibleContextMenu(params, desc))
                return;
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
                QStringLiteral("キー入力の候補がありません（使用文字・名前付きキーのいずれも未設定）"));
            scheduleNext();
            return;
        }
        const int index = int(m_rng.bounded(quint32(totalPool)));
        if (index < chars.size()) {
            const QChar ch = chars[index];
            PlatformAutomation::keyTap(ch);
            desc = QStringLiteral("キー入力 '%1'").arg(ch);
        } else {
            const Qt::Key namedKey = namedKeys[index - chars.size()];
            PlatformAutomation::keyTapNamed(namedKey);
            desc = QStringLiteral("キー入力 [%1]").arg(QKeySequence(namedKey).toString());
        }
        break;
    }
    case ActionKind::ScrollUp: {
        const int lo = qMin(params.scrollUpMinAmount, params.scrollUpMaxAmount);
        const int hi = qMax(params.scrollUpMinAmount, params.scrollUpMaxAmount);
        const int amount = qMax(1, lo + (hi > lo ? int(m_rng.bounded(quint32(hi - lo + 1))) : 0));
        const int dy = amount;  // positive == scroll up, matching both backends' wheel-event convention
        PlatformAutomation::scroll(pt, 0, dy);
        desc = QStringLiteral("スクロール(上) at (%1, %2) dy=%3").arg(pt.x()).arg(pt.y()).arg(dy);
        break;
    }
    case ActionKind::ScrollDown: {
        const int lo = qMin(params.scrollDownMinAmount, params.scrollDownMaxAmount);
        const int hi = qMax(params.scrollDownMinAmount, params.scrollDownMaxAmount);
        const int amount = qMax(1, lo + (hi > lo ? int(m_rng.bounded(quint32(hi - lo + 1))) : 0));
        const int dy = -amount;  // negative == scroll down
        PlatformAutomation::scroll(pt, 0, dy);
        desc = QStringLiteral("スクロール(下) at (%1, %2) dy=%3").arg(pt.x()).arg(pt.y()).arg(dy);
        break;
    }
    case ActionKind::ScrollHorizontal: {
        const int lo = qMin(params.scrollHorizontalMinAmount, params.scrollHorizontalMaxAmount);
        const int hi = qMax(params.scrollHorizontalMinAmount, params.scrollHorizontalMaxAmount);
        const int amount = qMax(1, lo + (hi > lo ? int(m_rng.bounded(quint32(hi - lo + 1))) : 0));
        const int dx = m_rng.bounded(2u) == 0 ? amount : -amount;  // random left/right each time
        PlatformAutomation::scroll(pt, dx, 0);
        desc = QStringLiteral("スクロール(横) at (%1, %2) dx=%3").arg(pt.x()).arg(pt.y()).arg(dx);
        break;
    }
    case ActionKind::Shortcut: {
        if (params.shortcutSequences.isEmpty()) {
            emit logMessage(QStringLiteral("ショートカットが設定されていません"));
            scheduleNext();
            return;
        }
        const QString seqText =
            params.shortcutSequences[int(m_rng.bounded(quint32(params.shortcutSequences.size())))];
        Qt::Key key = Qt::Key(0);
        Qt::KeyboardModifiers mods;
        if (!parseShortcut(seqText, key, mods)) {
            emit logMessage(QStringLiteral("ショートカット '%1' を解釈できませんでした").arg(seqText));
            scheduleNext();
            return;
        }
        if (m_config.keepTargetActive)
            PlatformAutomation::activateProcess(m_config.targetPid);
        PlatformAutomation::keyShortcut(key, mods);
        desc = QStringLiteral("ショートカット '%1'").arg(seqText);
        break;
    }
    case ActionKind::WindowOp: {
        QRect currentBounds;
        if (!PlatformAutomation::queryWindowBounds(m_config.targetWindowId, m_config.targetPid,
                                                     currentBounds)) {
            doStop(QStringLiteral("対象ウィンドウが見つからないため停止しました"), /*isAnomaly=*/true);
            return;
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
            emit logMessage(QStringLiteral("ウィンドウ操作の種類が選択されていません"));
            scheduleNext();
            return;
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
            desc = QStringLiteral("ウィンドウ移動 → (%1, %2)").arg(newPos.x()).arg(newPos.y());
        } else if (op == QStringLiteral("resize")) {
            constexpr int kMinW = 200, kMinH = 150;
            const int maxW = qMax(kMinW, screenGeom.width());
            const int maxH = qMax(kMinH, screenGeom.height());
            const QSize newSize(kMinW + int(m_rng.bounded(quint32(qMax(1, maxW - kMinW + 1)))),
                                 kMinH + int(m_rng.bounded(quint32(qMax(1, maxH - kMinH + 1)))));
            PlatformAutomation::resizeWindow(m_config.targetPid, m_config.targetWindowId, newSize);
            desc = QStringLiteral("ウィンドウリサイズ → %1 x %2").arg(newSize.width()).arg(newSize.height());
        } else if (op == QStringLiteral("minimize")) {
            PlatformAutomation::minimizeWindow(m_config.targetPid, m_config.targetWindowId);
            desc = QStringLiteral("ウィンドウを最小化");
        } else {  // maximize
            PlatformAutomation::maximizeWindow(m_config.targetPid, m_config.targetWindowId);
            desc = QStringLiteral("ウィンドウを最大化");
        }
        break;
    }
    }

    ++m_iterationCount;
    ++m_currentStepActionsDone;
    emit actionPerformed(desc);
    emit logMessage(desc);
    emit iterationCountChanged(m_iterationCount);

    if (m_currentStepActionsDone >= step.actionCount)
        advanceToNextStep();

    scheduleNext();
}
