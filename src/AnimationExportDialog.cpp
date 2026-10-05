#include "AnimationExportDialog.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "GifEncoder.h"
#include "I18n.h"

namespace
{
QString ffmpegExecutablePath()
{
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}
}  // namespace

AnimationExportDialog::AnimationExportDialog(const QString &framesDir, QWidget *parent)
    : QDialog(parent), m_framesDir(framesDir)
{
    setWindowTitle(I18n::t(QStringLiteral("アニメーションに変換")));
    setMinimumWidth(440);

    const QDir dir(framesDir);
    const QStringList names =
        dir.entryList(QStringList() << QStringLiteral("frame_*.png"), QDir::Files, QDir::Name);
    for (const QString &n : names)
        m_framePaths << dir.filePath(n);

    auto *layout = new QVBoxLayout(this);

    m_frameCountLabel =
        new QLabel(I18n::t(QStringLiteral("フレーム数: %1")).arg(m_framePaths.size()), this);
    layout->addWidget(m_frameCountLabel);

    auto *form = new QFormLayout;

    m_formatCombo = new QComboBox(this);
    const bool ffmpegFound = !ffmpegExecutablePath().isEmpty();
    auto *model = new QStandardItemModel(m_formatCombo);
    auto addFormat = [&](const QString &label, const QString &data, bool enabled) {
        auto *item = new QStandardItem(label);
        item->setData(data, Qt::UserRole);
        if (!enabled) {
            item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            item->setToolTip(I18n::t(
                QStringLiteral("ffmpegが見つからないため使用できません"
                                "（実行環境にffmpegをインストールすると選べるようになります）")));
        }
        model->appendRow(item);
    };
    addFormat(QStringLiteral("GIF"), QStringLiteral("gif"), true);
    addFormat(QStringLiteral("MP4 (H.264)"), QStringLiteral("mp4"), ffmpegFound);
    addFormat(QStringLiteral("WebM (VP9)"), QStringLiteral("webm"), ffmpegFound);
    m_formatCombo->setModel(model);
    form->addRow(I18n::t(QStringLiteral("出力形式:")), m_formatCombo);

    m_delaySpin = new QSpinBox(this);
    m_delaySpin->setRange(10, 10000);
    m_delaySpin->setValue(500);
    m_delaySpin->setSuffix(QStringLiteral(" ms"));
    m_delaySpin->setToolTip(
        I18n::t(QStringLiteral("各フレームを表示する時間（常時録画の既定の間隔は500msです）")));
    form->addRow(I18n::t(QStringLiteral("フレーム間隔:")), m_delaySpin);

    layout->addLayout(form);

    if (!ffmpegFound) {
        auto *note = new QLabel(
            I18n::t(QStringLiteral("ヒント: ffmpegをインストールすると、MP4/WebM形式でも書き出せるようになります。")),
            this);
        note->setWordWrap(true);
        layout->addWidget(note);
    }

    m_statusLabel = new QLabel(this);
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    m_exportButton = new QPushButton(I18n::t(QStringLiteral("書き出す...")), this);
    m_exportButton->setEnabled(!m_framePaths.isEmpty());
    m_closeButton = new QPushButton(I18n::t(QStringLiteral("閉じる")), this);
    buttonRow->addWidget(m_exportButton);
    buttonRow->addWidget(m_closeButton);
    layout->addLayout(buttonRow);

    connect(m_exportButton, &QPushButton::clicked, this, &AnimationExportDialog::onExportOrCancelClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);

    if (m_framePaths.isEmpty()) {
        m_statusLabel->setText(
            I18n::t(QStringLiteral("フォルダ内にframe_*.pngが見つかりませんでした: %1")).arg(framesDir));
    }
}

void AnimationExportDialog::closeEvent(QCloseEvent *event)
{
    if (m_running) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void AnimationExportDialog::reject()
{
    if (m_running)
        return;  // ignore Escape etc. while an export is in progress -- use キャンセル instead
    QDialog::reject();
}

void AnimationExportDialog::setControlsEnabled(bool enabled)
{
    m_formatCombo->setEnabled(enabled);
    m_delaySpin->setEnabled(enabled);
    m_closeButton->setEnabled(enabled);
}

void AnimationExportDialog::onExportOrCancelClicked()
{
    if (m_running) {
        m_cancelRequested = true;
        if (m_ffmpegProcess)
            m_ffmpegProcess->kill();
        return;
    }

    const QString formatKey = m_formatCombo->currentData(Qt::UserRole).toString();
    QString filter;
    QString suffix;
    if (formatKey == QStringLiteral("mp4")) {
        filter = QStringLiteral("MP4 (*.mp4)");
        suffix = QStringLiteral("mp4");
    } else if (formatKey == QStringLiteral("webm")) {
        filter = QStringLiteral("WebM (*.webm)");
        suffix = QStringLiteral("webm");
    } else {
        filter = I18n::t(QStringLiteral("GIFアニメーション (*.gif)"));
        suffix = QStringLiteral("gif");
    }

    const QString defaultPath = QDir(m_framesDir).filePath(QStringLiteral("animation.%1").arg(suffix));
    QString outputPath =
        QFileDialog::getSaveFileName(this, I18n::t(QStringLiteral("アニメーションを書き出す")), defaultPath, filter);
    if (outputPath.isEmpty())
        return;
    if (QFileInfo(outputPath).suffix().isEmpty())
        outputPath += QStringLiteral(".") + suffix;

    m_running = true;
    m_cancelRequested = false;
    setControlsEnabled(false);
    m_exportButton->setText(I18n::t(QStringLiteral("キャンセル")));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    m_progressBar->setValue(0);

    QString error;
    const bool ok = (formatKey == QStringLiteral("gif")) ? runGifExport(outputPath, error)
                                                           : runFfmpegExport(outputPath, formatKey, error);

    m_running = false;
    setControlsEnabled(true);
    m_exportButton->setText(I18n::t(QStringLiteral("書き出す...")));
    m_progressBar->setVisible(false);
    m_statusLabel->clear();

    if (ok) {
        QMessageBox::information(this, I18n::t(QStringLiteral("変換完了")),
                                  I18n::t(QStringLiteral("書き出しが完了しました: %1")).arg(outputPath));
    } else if (!m_cancelRequested) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("変換に失敗しました")), error);
    }
    // Cancelled: no dialog -- the controls resetting back is feedback enough.
}

bool AnimationExportDialog::runGifExport(const QString &outputPath, QString &error)
{
    QVector<QImage> frames;
    frames.reserve(m_framePaths.size());
    for (const QString &p : m_framePaths)
        frames.push_back(QImage(p));

    m_progressBar->setRange(0, frames.size());

    const auto progressCb = [this](int current, int total) {
        m_progressBar->setValue(current);
        m_statusLabel->setText(I18n::t(QStringLiteral("GIFに変換中... (%1 / %2)")).arg(current).arg(total));
        // Process real (not just internal) events so the キャンセル button
        // click actually reaches us mid-encode -- every other control stays
        // disabled (setControlsEnabled(false) above) for the duration, so
        // there's nothing else re-entrancy could trip over here.
        QCoreApplication::processEvents();
        return !m_cancelRequested;
    };

    const int delayCentiseconds = std::max(1, m_delaySpin->value() / 10);
    return GifEncoder::writeAnimatedGif(outputPath, frames, delayCentiseconds, &error, progressCb);
}

bool AnimationExportDialog::runFfmpegExport(const QString &outputPath, const QString &formatKey, QString &error)
{
    const QString ffmpegPath = ffmpegExecutablePath();
    if (ffmpegPath.isEmpty()) {
        error = I18n::t(QStringLiteral("ffmpegが見つかりません"));
        return false;
    }

    // ffmpeg's image2 demuxer expects every frame in the sequence to share
    // one size; ManualRecorder's frames can differ if the target window
    // was resized mid-recording (see GifEncoder::writeAnimatedGif()'s own
    // comment on the same situation). Normalize into a temp folder the
    // same way -- pad onto a shared canvas, anchored top-left -- before
    // handing anything to ffmpeg.
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        error = I18n::t(QStringLiteral("一時フォルダを作成できませんでした"));
        return false;
    }

    int maxWidth = 0, maxHeight = 0;
    QVector<QImage> frames;
    frames.reserve(m_framePaths.size());
    for (const QString &p : m_framePaths) {
        QImage img(p);
        maxWidth = std::max(maxWidth, img.width());
        maxHeight = std::max(maxHeight, img.height());
        frames.push_back(img);
    }

    m_progressBar->setRange(0, frames.size());
    for (int i = 0; i < frames.size(); ++i) {
        if (m_cancelRequested) {
            error = I18n::t(QStringLiteral("キャンセルされました"));
            return false;
        }
        QImage frame = frames[i];
        if (frame.width() != maxWidth || frame.height() != maxHeight) {
            QImage canvas(maxWidth, maxHeight, QImage::Format_RGB32);
            canvas.fill(Qt::white);
            QPainter painter(&canvas);
            painter.drawImage(0, 0, frame);
            painter.end();
            frame = canvas;
        }
        frame.save(tempDir.filePath(QStringLiteral("frame_%1.png").arg(i + 1, 5, 10, QChar('0'))));
        m_progressBar->setValue(i + 1);
        m_statusLabel->setText(I18n::t(QStringLiteral("フレームを準備中... (%1 / %2)")).arg(i + 1).arg(frames.size()));
        QCoreApplication::processEvents();
    }

    const double fps = 1000.0 / std::max(1, m_delaySpin->value());
    QStringList args;
    args << QStringLiteral("-y") << QStringLiteral("-framerate") << QString::number(fps, 'f', 3)
         << QStringLiteral("-i") << tempDir.filePath(QStringLiteral("frame_%05d.png"))
         << QStringLiteral("-vf") << QStringLiteral("pad=ceil(iw/2)*2:ceil(ih/2)*2")
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p") << QStringLiteral("-c:v")
         << (formatKey == QStringLiteral("mp4") ? QStringLiteral("libx264") : QStringLiteral("libvpx-vp9"))
         << outputPath;

    m_statusLabel->setText(I18n::t(QStringLiteral("ffmpegで変換中...")));
    m_progressBar->setRange(0, 0);  // ffmpeg's own progress isn't easily mapped to a percentage here

    QProcess process;
    m_ffmpegProcess = &process;
    process.start(ffmpegPath, args);
    if (!process.waitForStarted(5000)) {
        m_ffmpegProcess = nullptr;
        error = I18n::t(QStringLiteral("ffmpegを起動できませんでした"));
        return false;
    }

    QEventLoop loop;
    connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    QTimer cancelPollTimer;
    cancelPollTimer.setInterval(100);
    connect(&cancelPollTimer, &QTimer::timeout, this, [this]() {
        if (m_cancelRequested && m_ffmpegProcess)
            m_ffmpegProcess->kill();
    });
    cancelPollTimer.start();
    loop.exec();
    m_ffmpegProcess = nullptr;

    if (m_cancelRequested) {
        QFile::remove(outputPath);
        error = I18n::t(QStringLiteral("キャンセルされました"));
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QFile::remove(outputPath);
        error = I18n::t(QStringLiteral("ffmpegの実行に失敗しました:\n%1"))
                    .arg(QString::fromUtf8(process.readAllStandardError()).right(800));
        return false;
    }
    return true;
}
