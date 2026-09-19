#include "RegionHighlightOverlay.h"

#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

namespace
{
QRect virtualDesktopGeometry()
{
    QRect all;
    for (QScreen *screen : QGuiApplication::screens())
        all = all.united(screen->geometry());
    return all;
}
}  // namespace

RegionHighlightOverlay::RegionHighlightOverlay(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool |
                    Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setGeometry(virtualDesktopGeometry());
}

void RegionHighlightOverlay::showRegion(const QString &name, const QList<QRect> &includeRegions,
                                         const QList<QRect> &excludeRegions)
{
    m_name = name;
    m_includeRegions = includeRegions;
    m_excludeRegions = excludeRegions;
    setGeometry(virtualDesktopGeometry());
    show();
    raise();
    update();
}

void RegionHighlightOverlay::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPoint origin = geometry().topLeft();

    auto drawRectList = [&](const QList<QRect> &rects, const QColor &fill, const QColor &border) {
        for (const QRect &r : rects) {
            const QRect local = r.translated(-origin);
            p.setPen(QPen(border, 3));
            p.setBrush(fill);
            p.drawRect(local);
        }
    };

    drawRectList(m_includeRegions, QColor(0, 200, 0, 60), QColor(0, 255, 0));
    drawRectList(m_excludeRegions, QColor(220, 0, 0, 80), QColor(255, 60, 60));

    if (!m_includeRegions.isEmpty()) {
        const QRect first = m_includeRegions.first().translated(-origin);
        p.setFont(QFont(font().family(), 12, QFont::Bold));
        const QString label = QStringLiteral("操作領域「%1」").arg(m_name);
        const QRect labelBg(first.left(), qMax(0, first.top() - 26), qMax(160, first.width()), 24);
        p.fillRect(labelBg, QColor(0, 150, 0, 220));
        p.setPen(Qt::white);
        p.drawText(labelBg, Qt::AlignCenter, label);
    }
}
