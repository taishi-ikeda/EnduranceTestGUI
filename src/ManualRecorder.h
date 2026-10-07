#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>

// Always-on screen recording of a target window, independent of
// RandomActionEngine: unlike TestConfig::enableScreenRecording (a short
// rolling buffer kept only to explain an anomaly after the fact -- see
// RandomActionEngine::captureRecordingFrame()/saveRecordingFrames()), this
// is started/stopped directly by the user from MainWindow's dedicated
// "録画" button and keeps every frame for as long as it runs, whether or
// not an automated test is in progress -- it works equally well while the
// user is operating the target app by hand.
//
// Frames are saved as sequential PNGs (frame_0001.png, frame_0002.png, ...)
// under a new timestamped subfolder, in the same base directory
// RandomActionEngine::anomalyArtifactsDirectory() uses for its own saved
// artifacts, so everything the app writes out stays in one place.
class ManualRecorder : public QObject
{
    Q_OBJECT

public:
    explicit ManualRecorder(QObject *parent = nullptr);

    // Starts capturing `targetWindowId`/`targetPid`'s current window,
    // re-resolved every frame (see captureFrame()) so the recording keeps
    // following it across manual moves/resizes. First restores the target
    // if it's minimized (see the .cpp), so that alone isn't a reason this
    // fails. Returns false (recording stays off, no signal emitted) if the
    // window's bounds still can't be determined right now -- e.g. no
    // target selected, or it was closed rather than just minimized.
    bool start(std::uint32_t targetWindowId, qint64 targetPid);
    // No-op if not currently recording.
    void stop();
    bool isRecording() const { return m_recording; }
    QString outputDirectory() const { return m_outputDir; }
    int frameCount() const { return m_frameCount; }

signals:
    void recordingStarted(const QString &outputDir);
    // `reason` is empty for a normal, user-requested stop; non-empty when
    // captureFrame() had to stop the recording itself (the target window
    // disappeared mid-recording).
    void recordingStopped(const QString &outputDir, int frameCount, const QString &reason);

private slots:
    void captureFrame();

private:
    QTimer m_timer;
    std::uint32_t m_targetWindowId = 0;
    qint64 m_targetPid = -1;
    QString m_outputDir;
    int m_frameCount = 0;
    bool m_recording = false;
};
