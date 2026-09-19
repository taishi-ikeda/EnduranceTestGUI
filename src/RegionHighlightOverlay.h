#pragma once

#include <QList>
#include <QRect>
#include <QString>
#include <QWidget>

// Non-interactive, click-through overlay that draws a translucent
// highlight over a NamedRegion's include/exclude rectangles, so selecting
// a region in the "①対象選択" panel's operation-region list shows where it
// actually is on screen (SPEC.md 6.3). Unlike RegionSelectorOverlay, this
// never captures mouse/keyboard input and doesn't block -- MainWindow just
// shows/updates/hides it as the list selection changes.
class RegionHighlightOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit RegionHighlightOverlay(QWidget *parent = nullptr);

    void showRegion(const QString &name, const QList<QRect> &includeRegions,
                     const QList<QRect> &excludeRegions);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_name;
    QList<QRect> m_includeRegions;
    QList<QRect> m_excludeRegions;
};
