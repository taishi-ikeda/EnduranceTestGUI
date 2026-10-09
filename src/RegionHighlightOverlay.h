#pragma once

#include <QList>
#include <QObject>
#include <QRect>
#include <QString>

class RegionHighlightScreenWindow;

// One named region's rectangles, as drawn by RegionHighlightOverlay::
// showRegions() below -- each entry gets its own label, independent of how
// many others are shown at the same time.
struct RegionHighlightEntry
{
    QString name;
    QList<QRect> includeRegions;
    QList<QRect> excludeRegions;
};

// Shows a translucent, click-through highlight over one or more
// NamedRegions' include/exclude rectangles at once, so selecting a region
// in the "①対象選択" panel's operation-region list shows where it
// actually is on screen (SPEC.md 6.3), or so MainWindow's "操作領域を
// 確認" button (SPEC.md 10) can show every registered region at once.
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

    // Single-region convenience wrapper -- equivalent to showRegions()
    // with one entry.
    void showRegion(const QString &name, const QList<QRect> &includeRegions,
                     const QList<QRect> &excludeRegions);
    // Shows every entry at once, each with its own label. Replaces
    // whatever was shown by a previous showRegion()/showRegions() call
    // (it does not accumulate).
    void showRegions(const QList<RegionHighlightEntry> &entries);
    void hide();

private:
    QList<RegionHighlightEntry> m_entries;
    QList<RegionHighlightScreenWindow *> m_screenWindows;
};
