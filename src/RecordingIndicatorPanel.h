#pragma once

#include <QWidget>

// Small always-on-top floating panel shown while the startup setup "記録"
// (record) feature (SPEC.md 6.13) is observing input, so the user always
// has a visible reminder that it's active -- and an on-screen way to end it
// -- even though recording itself works system-wide regardless of which
// window currently has focus (mirrors StopPanel's rationale for a real
// test run). Positioned near the top-right of the primary screen, same as
// StopPanel; the two are never shown at once (recording and a test run are
// mutually exclusive states), so there's no risk of them overlapping.
class RecordingIndicatorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingIndicatorPanel(QWidget *parent = nullptr);

    void setRecordedActionCount(int count);

signals:
    void stopRequested();

private:
    class QLabel *m_countLabel = nullptr;
};
