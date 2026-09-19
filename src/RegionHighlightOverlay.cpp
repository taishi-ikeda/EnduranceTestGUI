#include "RegionHighlightOverlay.h"

#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWidget>

// One borderless, click-through, always-on-top window per QScreen, sized
// and positioned to exactly match that screen -- see the class comment in
// RegionHighlightOverlay.h for why this is split per screen instead of one
// window spanning the whole virtual desktop.
class RegionHighlightScreenWindow : public QWidget
{
public:
    explicit RegionHighlightScreenWindow(QScreen *screen) : QWidget(nullptr), m_screen(screen)
    {
        // Same window-type flags as RegionSelectorOverlay (known to
        // position itself correctly on screen). Click-through is done
        // purely via the WA_TransparentForMouseEvents widget *attribute*
        // below, not an extra window flag.
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    void setContent(const QString &name, const QList<QRect> &includeRegions,
                     const QList<QRect> &excludeRegions)
    {
        m_name = name;
        m_includeRegions = includeRegions;
        m_excludeRegions = excludeRegions;
        // Order matters on macOS: RegionSelectorOverlay's working pattern
        // is show() *then* setGeometry().
        show();
        const QRect screenGeom = m_screen->geometry();
        setGeometry(screenGeom);
        // Captured ourselves rather than re-read back via geometry() when
        // painting, in case the window system hasn't finished applying the
        // above by the time the paint actually happens.
        m_origin = screenGeom.topLeft();
        raise();
        update();
    }

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        auto drawRectList = [&](const QList<QRect> &rects, const QColor &fill, const QColor &border) {
            for (const QRect &r : rects) {
                // Rects that don't overlap this screen at all just draw
                // (partially or fully) outside the widget's own bounds,
                // where QPainter clips them away -- exactly what we want
                // when a region spans more than one screen.
                const QRect local = r.translated(-m_origin);
                p.setPen(QPen(border, 3));
                p.setBrush(fill);
                p.drawRect(local);
            }
        };

        drawRectList(m_includeRegions, QColor(0, 200, 0, 60), QColor(0, 255, 0));
        drawRectList(m_excludeRegions, QColor(220, 0, 0, 80), QColor(255, 60, 60));

        if (!m_includeRegions.isEmpty()) {
            const QRect first = m_includeRegions.first().translated(-m_origin);
            p.setFont(QFont(font().family(), 12, QFont::Bold));
            const QString label = QStringLiteral("操作領域「%1」").arg(m_name);
            const QRect labelBg(first.left(), qMax(0, first.top() - 26), qMax(160, first.width()), 24);
            p.fillRect(labelBg, QColor(0, 150, 0, 220));
            p.setPen(Qt::white);
            p.drawText(labelBg, Qt::AlignCenter, label);
        }
    }

private:
    QScreen *m_screen;
    QPoint m_origin;
    QString m_name;
    QList<QRect> m_includeRegions;
    QList<QRect> m_excludeRegions;
};

RegionHighlightOverlay::RegionHighlightOverlay(QObject *parent) : QObject(parent) {}

RegionHighlightOverlay::~RegionHighlightOverlay()
{
    qDeleteAll(m_screenWindows);
}

void RegionHighlightOverlay::showRegion(const QString &name, const QList<QRect> &includeRegions,
                                         const QList<QRect> &excludeRegions)
{
    m_name = name;
    m_includeRegions = includeRegions;
    m_excludeRegions = excludeRegions;

    // Rebuilt from scratch every time rather than reused/resized: the set
    // of screens can change (a monitor connected/disconnected) between
    // selections, and a stale QScreen* would be a dangling pointer if one
    // was unplugged. This is cheap since it only happens on a list
    // selection change, not per frame.
    qDeleteAll(m_screenWindows);
    m_screenWindows.clear();
    for (QScreen *screen : QGuiApplication::screens()) {
        auto *window = new RegionHighlightScreenWindow(screen);
        window->setContent(m_name, m_includeRegions, m_excludeRegions);
        m_screenWindows.append(window);
    }
}

void RegionHighlightOverlay::hide()
{
    qDeleteAll(m_screenWindows);
    m_screenWindows.clear();
}
