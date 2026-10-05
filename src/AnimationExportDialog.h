#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QComboBox;
class QSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QProcess;
class QCloseEvent;

// Converts a folder of frame_NNNNN.png files (as ManualRecorder writes --
// see its own header for the always-on manual/automated recording
// feature this is the post-processing step for) into a single animation
// file. GIF is always available (GifEncoder's own dependency-free
// encoder); MP4/WebM are offered only when ffmpeg happens to be found on
// PATH at runtime, since this app otherwise has no external tool
// dependency. Shows progress and supports cancelling mid-export.
class AnimationExportDialog : public QDialog
{
    Q_OBJECT

public:
    // `framesDir` is scanned immediately for frame_*.png files; the
    // dialog reports how many it found and disables export if none.
    explicit AnimationExportDialog(const QString &framesDir, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private slots:
    void onExportOrCancelClicked();

private:
    QString m_framesDir;
    QStringList m_framePaths;  // sorted frame_*.png absolute paths

    QComboBox *m_formatCombo = nullptr;
    QSpinBox *m_delaySpin = nullptr;
    QLabel *m_frameCountLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPushButton *m_exportButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    bool m_running = false;
    bool m_cancelRequested = false;
    QProcess *m_ffmpegProcess = nullptr;  // only non-null while runFfmpegExport() is on the stack

    void setControlsEnabled(bool enabled);
    bool runGifExport(const QString &outputPath, QString &error);
    bool runFfmpegExport(const QString &outputPath, const QString &formatKey, QString &error);
};
