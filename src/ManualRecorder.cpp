#include "ManualRecorder.h"

#include <QDateTime>
#include <QDir>
#include <QPixmap>
#include <QRect>
#include <QThread>

#include "I18n.h"
#include "OverlayGeometry.h"
#include "RandomActionEngine.h"
#include "platform/PlatformAutomation.h"

namespace
{
// Same cadence as TestConfig::enableScreenRecording's anomaly ring buffer
// (RandomActionEngine.cpp) -- two frames a second is enough to follow along
// with ordinary manual operation without flooding the output folder with
// files on a recording left running for a long session.
constexpr int kManualRecordingFrameIntervalMs = 500;

// start()'s retry window for a target window that was just restored from
// minimized (see activateProcess() there): restoring the OS-level window
// state (on Linux, the window manager mapping the window back in reaction
// to the _NET_ACTIVE_WINDOW message activateProcess() sends; on macOS, the
// deminiaturize animation activateProcess() there explicitly triggers)
// happens on another process's own schedule, not synchronously with our
// request -- measured at roughly 60ms for openbox in this project's own
// sandbox (the macOS path is, like other macOS-specific behavior in this
// project, unverified on real hardware -- its deminiaturize animation may
// take longer). 15 attempts at 50ms apart (750ms worst case) leaves a
// comfortable margin for a slower window manager/system without stalling
// the UI noticeably on the (common) case where the window was already
// visible and the very first attempt succeeds.
constexpr int kRestoreRetryAttempts = 15;
constexpr int kRestoreRetryDelayMs = 50;
}  // namespace

ManualRecorder::ManualRecorder(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(kManualRecordingFrameIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &ManualRecorder::captureFrame);
}

bool ManualRecorder::start(std::uint32_t targetWindowId, qint64 targetPid, QString *errorOut)
{
    if (m_recording)
        return true;

    // Restore the target first if it's minimized/iconified -- queryWindowBounds()
    // only finds currently-mapped/viewable windows (see its own platform
    // implementation's comments), so a minimized target would otherwise be
    // reported as "can't find the window" below even though it's simply
    // hidden, not actually gone. activateProcess() explicitly handles
    // restoring a minimized window (see its own comment on each platform),
    // mirroring RandomActionEngine::start()'s unconditional activateProcess()
    // call at the very beginning of a run (TestConfig::keepTargetActive) --
    // without this, "▶ 開始" tolerates a minimized target but "● 録画" did
    // not, a user-visible inconsistency between the two (reported as
    // ManualRecorder::start() failing with "対象ウィンドウの位置・サイズを
    // 取得できませんでした" right after selecting a target that happened to
    // be minimized). A no-op (false, silently ignored) if the target has no
    // window at all -- queryWindowBounds() below still correctly fails in
    // that case.
    PlatformAutomation::activateProcess(targetPid);

    // If the target was actually minimized, the restore above has very
    // likely not taken effect yet the very first time bounds are checked
    // (see kRestoreRetryAttempts' comment) -- poll briefly rather than
    // failing immediately. A target that was never minimized (the common
    // case) succeeds on the first attempt and never sleeps at all.
    QRect bounds;
    bool found = PlatformAutomation::queryWindowBounds(targetWindowId, targetPid, bounds);
    for (int attempt = 0; !found && attempt < kRestoreRetryAttempts; ++attempt) {
        QThread::msleep(kRestoreRetryDelayMs);
        found = PlatformAutomation::queryWindowBounds(targetWindowId, targetPid, bounds);
    }
    if (!found) {
        if (errorOut)
            *errorOut = I18n::t(QStringLiteral("対象ウィンドウの位置・サイズを取得できませんでした。"));
        return false;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    m_outputDir = QStringLiteral("%1/recording_%2")
                      .arg(RandomActionEngine::anomalyArtifactsDirectory(), timestamp);
    if (!QDir().mkpath(m_outputDir)) {
        // A completely different failure from "couldn't find the window"
        // above -- e.g. QStandardPaths::writableLocation(PicturesLocation)
        // (anomalyArtifactsDirectory()) returning empty or some other
        // unwritable path in an unusual environment (no $HOME, no XDG user
        // dirs configured, a read-only filesystem, ...). Surfacing the
        // actual path here, rather than silently falling through to the
        // same generic "position/size" message as the window-not-found
        // case above, is the whole point of this branch existing
        // separately -- see errorOut's own comment in the header.
        if (errorOut) {
            *errorOut = I18n::t(QStringLiteral("録画の保存先フォルダを作成できませんでした: %1")).arg(m_outputDir);
        }
        return false;
    }

    m_targetWindowId = targetWindowId;
    m_targetPid = targetPid;
    m_frameCount = 0;
    m_recording = true;

    // Every frame here is a screen-composite grab (QScreen::grabWindow(0, ...),
    // same as grabTargetWindowScreenshot()/OverlayGeometry::grabWindowSnapshot()
    // elsewhere in this app -- not an off-screen capture of the target
    // window's own buffer), so whatever is actually topmost on screen at
    // each frame is what gets captured. A RandomActionEngine run guards
    // against this by periodically re-activating the target while
    // TestConfig::keepTargetActive is set; this manual recording has no
    // such ongoing guard (it would otherwise fight the user clicking back
    // into this app's own "■ 録画停止" button), so the activateProcess()
    // call above (needed up front anyway, to restore a minimized target
    // before the bounds check) is the only one for the whole recording --
    // a clean starting point, not an ongoing guarantee. If the user later
    // brings another window (including this one) in front of the target
    // mid-recording, the frames saved during that overlap will show that
    // window instead, same as every other screenshot feature in this app.

    m_timer.start();
    captureFrame();  // first frame immediately, not after the first interval
    emit recordingStarted(m_outputDir);
    return true;
}

void ManualRecorder::stop()
{
    if (!m_recording)
        return;
    m_timer.stop();
    m_recording = false;
    emit recordingStopped(m_outputDir, m_frameCount, QString());
}

void ManualRecorder::captureFrame()
{
    QRect bounds;
    if (!PlatformAutomation::queryWindowBounds(m_targetWindowId, m_targetPid, bounds)) {
        // The target window went away mid-recording (closed/crashed) --
        // stop rather than keep failing silently every 500ms, but still
        // tell the caller why, unlike a normal user-requested stop().
        m_timer.stop();
        m_recording = false;
        emit recordingStopped(m_outputDir, m_frameCount,
                               I18n::t(QStringLiteral("対象ウィンドウが見つからなくなったため録画を停止しました")));
        return;
    }

    const QPixmap frame = OverlayGeometry::grabWindowSnapshot(bounds);
    if (frame.isNull())
        return;  // transient grab failure -- try again next tick rather than aborting the recording

    ++m_frameCount;
    const QString path = QStringLiteral("%1/frame_%2.png").arg(m_outputDir).arg(m_frameCount, 5, 10, QChar('0'));
    frame.save(path);
}
