#pragma once

#include <QWidget>

// Small always-on-top floating panel shown while a test is running, so the
// user can abort or pause/resume it even though keyboard/mouse focus has
// been moved to the target application (e.g. by "対象アプリを常に最前面に
// 保つ" raising it over the main window, hiding its own Stop/Pause buttons).
// Positioned near the top-right of the primary screen.
class StopPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StopPanel(QWidget *parent = nullptr);

    void setIterationCount(qint64 count);
    void setPaused(bool paused);

signals:
    void stopRequested();
    void pauseResumeRequested();

private:
    class QLabel *m_countLabel = nullptr;
    class QPushButton *m_pauseResumeButton = nullptr;
};
