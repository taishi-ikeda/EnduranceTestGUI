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
    // (a single picked point, e.g. Click), 2 (a drag's start+end, or a
    // two-point sweep), or more (an N-point sweep -- SPEC.md 追加実装依頼
    // 「操作領域を点列（スイープ）で指定」中点対応 -- drawn as one dashed
    // polyline through every point in order, each with its own label).
    void showPoints(const QList<QPoint> &points, const QList<QString> &labels);

    // Same waypoint markers/polyline as showPoints() above, plus two
    // additions drawn on top (SPEC.md 追加実装依頼「点列の間隔マーク・
    // ランダム幅のエラーバー表示」), so NamedRegionEditorDialog's sweep-mode
    // preview can show exactly what RandomActionEngine::pickSweepPoint()
    // actually walks at runtime, not just the raw waypoints the user
    // clicked:
    //   - `intervalMarks` (screen-absolute, same coordinate space as
    //     `waypoints`): one small dot per point in the interval-spaced
    //     sequence (see SweepPath::pointSequence()) the engine steps
    //     through in order.
    //   - `jitterPx`: if > 0, a cross-shaped error bar (horizontal and
    //     vertical bars of half-length jitterPx, with small end caps) is
    //     drawn centered on each interval mark, depicting the independent
    //     ±jitterPx random offset pickSweepPoint() applies to x and y when
    //     actually operating that point. 0 draws no error bars (the engine
    //     always uses the exact mark position).
    void showSweepPath(const QList<QPoint> &waypoints, const QList<QString> &waypointLabels,
                        const QList<QPoint> &intervalMarks, int jitterPx);
    void hide();

private:
    QList<QPoint> m_points;
    QList<QString> m_labels;
    QList<QPoint> m_intervalMarks;
    int m_jitterPx = 0;
    QList<PointHighlightScreenWindow *> m_screenWindows;
};
