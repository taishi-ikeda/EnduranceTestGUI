#pragma once

#include <QList>
#include <QPoint>

// The fixed sequence of points a sweep-type region's path is divided into
// for interval-based (順番) traversal: walks each consecutive pair in
// `waypoints` in order (waypoints[0]->[1], [1]->[2], ...), subdividing each
// pair into points spaced as close to `intervalPx` apart as possible while
// always including that pair's own end point exactly (the per-segment
// point count is rounded to the nearest whole number of intervals, so the
// actual spacing within a given pair can differ slightly from intervalPx --
// SPEC.md 不具合報告E5/E8「点列の間隔(px)が区間等分のため実測値とずれる」).
// A pair whose two points are identical (or `intervalPx` <= 0) contributes
// just that single point. Returns an empty list if `waypoints` has fewer
// than 2 entries.
//
// Shared by RandomActionEngine (which walks this exact sequence at runtime
// via pickSweepPoint()) and NamedRegionEditorDialog (which draws it as a
// live preview via PointHighlightOverlay::showSweepPath() -- SPEC.md 追加
// 実装依頼「点列の間隔マーク・ランダム幅のエラーバー表示」), so the preview
// always matches actual runtime behavior exactly, imprecision included.
namespace SweepPath
{
QList<QPoint> pointSequence(const QList<QPoint> &waypoints, int intervalPx);
}
