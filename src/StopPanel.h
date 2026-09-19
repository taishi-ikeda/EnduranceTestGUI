#pragma once

#include <QWidget>

// Small always-on-top floating panel shown while a test is running, so the
// user can abort it even though keyboard/mouse focus has been moved to the
// target application. Positioned near the top-right of the primary screen.
class StopPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StopPanel(QWidget *parent = nullptr);

    void setIterationCount(qint64 count);

signals:
    void stopRequested();

private:
    class QLabel *m_countLabel = nullptr;
};
