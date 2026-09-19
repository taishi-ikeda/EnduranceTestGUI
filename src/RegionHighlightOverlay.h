#pragma once

#include <QList>
#include <QObject>
#include <QRect>
#include <QString>

class RegionHighlightScreenWindow;

// Shows a translucent, click-through highlight over a NamedRegion's
// include/exclude rectangles, so selecting a region in the "①対象選択"
// panel's operation-region list shows where it actually is on screen
// (SPEC.md 6.3).
//
// Manages one small top-level window *per QScreen* rather than a single
// window spanning the whole virtual desktop. A single widget that spans
// screens with different scale factors (a common setup: a built-in Retina
// display next to a non-Retina external monitor) can't be rendered
// correctly as one window in Qt, since a window's backing store has one
// scale factor for its whole area -- content positioned past the boundary
// into a differently-scaled screen ends up visibly offset/mis-scaled.
// Splitting per screen means each window only ever renders on the one
// screen it was sized to match, so this can't happen.
class RegionHighlightOverlay : public QObject
{
    Q_OBJECT

public:
    explicit RegionHighlightOverlay(QObject *parent = nullptr);
    ~RegionHighlightOverlay() override;

    void showRegion(const QString &name, const QList<QRect> &includeRegions,
                     const QList<QRect> &excludeRegions);
    void hide();

private:
    QString m_name;
    QList<QRect> m_includeRegions;
    QList<QRect> m_excludeRegions;
    QList<RegionHighlightScreenWindow *> m_screenWindows;
};
