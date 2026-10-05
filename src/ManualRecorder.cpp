#include "ManualRecorder.h"

#include <QDateTime>
#include <QDir>
#include <QPixmap>
#include <QRect>

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
}  // namespace

ManualRecorder::ManualRecorder(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(kManualRecordingFrameIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &ManualRecorder::captureFrame);
}

bool ManualRecorder::start(std::uint32_t targetWindowId, qint64 targetPid)
{
    if (m_recording)
        return true;

    QRect bounds;
    if (!PlatformAutomation::queryWindowBounds(targetWindowId, targetPid, bounds))
        return false;

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    m_outputDir = QStringLiteral("%1/recording_%2")
                      .arg(RandomActionEngine::anomalyArtifactsDirectory(), timestamp);
    if (!QDir().mkpath(m_outputDir))
        return false;

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
    // into this app's own "■ 録画停止" button), so only bring the target
    // to the front once, right as recording starts -- a clean starting
    // point. If the user later brings another window (including this one)
    // in front of the target mid-recording, the frames saved during that
    // overlap will show that window instead, same as every other
    // screenshot feature in this app.
    PlatformAutomation::activateProcess(targetPid);

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
