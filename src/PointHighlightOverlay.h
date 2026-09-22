#pragma once

#include <QList>
#include <QObject>
#include <QPoint>
#include <QString>

class PointHighlightScreenWindow;

// Shows small, click-through on-screen markers at one or two specific
// screen-absolute points (each with its own short label), so a picked
// position -- or a drag's start/end pair -- from SetupActionEditorDialog's
// "位置を選択..."/"開始位置を選択..."/"終了位置を選択..." stays visible on
// the target app for as long as that dialog is open, the same way
// RegionHighlightOverlay keeps a named operation region visible while
// NamedRegionEditorDialog is open (SPEC.md 6.3/6.13). Without this, the
// only feedback after picking a point was a small "(452, 124)" text label
// in the dialog itself -- no way to visually confirm on screen where that
// point actually lands relative to the target app's UI.
//
// Same one-window-per-QScreen design as RegionHighlightOverlay, for the
// same multi-monitor/mixed-scale-factor reason (see its header comment).
class PointHighlightOverlay : public QObject
{
    Q_OBJECT

public:
    explicit PointHighlightOverlay(QObject *parent = nullptr);
    ~PointHighlightOverlay() override;

    // `points` and `labels` must be the same size: 0 (nothing shown), 1
    // (a single picked point, e.g. Click), or 2 (a drag's start+end).
    void showPoints(const QList<QPoint> &points, const QList<QString> &labels);
    void hide();

private:
    QList<QPoint> m_points;
    QList<QString> m_labels;
    QList<PointHighlightScreenWindow *> m_screenWindows;
};
