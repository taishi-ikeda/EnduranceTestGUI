#include "RegionHighlightOverlay.h"
#include "OverlayGeometry.h"
#include "platform/PlatformAutomation.h"

#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QWidget>

using OverlayGeometry::settleDesktopBeforeSnapshot;

// One borderless, click-through, always-on-top window per QScreen, sized
// and positioned to exactly match that screen -- see the class comment in
// RegionHighlightOverlay.h for why this is split per screen instead of one
// window spanning the whole virtual desktop.
class RegionHighlightScreenWindow : public QWidget
{
public:
    explicit RegionHighlightScreenWindow() : QWidget(nullptr)
    {
        // Same window-type flags as RegionSelectorOverlay (known to
        // position itself correctly on screen). Click-through is done
        // purely via the WA_TransparentForMouseEvents widget *attribute*
        // below, not an extra window flag.
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        // Qt::WA_TranslucentBackground only where PlatformAutomation::
        // supportsWindowTransparency() says it will actually render as
        // see-through. Without a compositor running (common on minimal/
        // tiling Linux window managers), a window with this attribute
        // renders as solid black instead of blending against what's behind
        // it -- since this window covers an entire screen, that made the
        // whole screen go black the moment a NamedRegionEditorDialog opened
        // (SPEC.md 6.3/8), even hiding the dialog itself underneath. Where
        // real transparency isn't safe, paintEvent() instead paints a real
        // screenshot of the screen (m_background, grabbed in setContent()
        // below) as an opaque background, and draws the highlight
        // rectangles on top of that -- see OverlayGeometry::
        // grabVirtualDesktopSnapshot()'s comment for the same technique
        // used by RegionSelectorOverlay/PointPickerOverlay. This window is
        // restricted to the screen's *available* geometry (see setContent()
        // below), not its full pixel bounds -- see OverlayGeometry::
        // virtualDesktopGeometry()'s comment for why a captured screenshot
        // (or the window itself) drifts out of alignment with the real
        // screen by roughly the menu bar/Dock/panel strip's size otherwise.
        m_useRealTransparency = PlatformAutomation::supportsWindowTransparency();
        if (m_useRealTransparency)
            setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    void setContent(QScreen *screen, const QString &name, const QList<QRect> &includeRegions,
                     const QList<QRect> &excludeRegions)
    {
        m_screen = screen;
        m_name = name;
        m_includeRegions = includeRegions;
        m_excludeRegions = excludeRegions;
        // Only re-grab the screen if this window isn't already showing on
        // top of it: once it's visible, grabbing would just capture our
        // own window (whatever we last painted) instead of the real
        // desktop content underneath. isVisible() is false the first time
        // this is called and again after every hide() (RegionHighlightOverlay::
        // hide(), e.g. while RegionSelectorOverlay is up, or once the owning
        // dialog closes) -- so a fresh snapshot is grabbed each time this
        // highlight reappears, not just once ever.
        // Restricted to this screen's *available* geometry (excluding the
        // menu bar/Dock/panel strip the OS reserves for itself), not its
        // full physical bounds -- see OverlayGeometry::virtualDesktopGeometry()'s
        // comment for why: an ordinary always-on-top window like this one is
        // not allowed to actually occupy that reserved strip, so requesting
        // the full geometry gets it silently pushed/clipped away from that
        // strip by the window manager, landing its real on-screen position
        // off by roughly the strip's size from what was asked for (and from
        // what this background snapshot/m_origin below assume).
        const QRect screenGeom = m_screen->availableGeometry();
        if (!m_useRealTransparency && !isVisible()) {
            // See settleDesktopBeforeSnapshot()'s comment: this window is
            // typically re-shown right after RegionSelectorOverlay (or this
            // same overlay's previous hide()) has just closed/hidden, and
            // grabbing immediately can otherwise capture that leftover
            // content instead of the real desktop. Cropped to screenGeom so
            // the grabbed content lines up with this window's own bounds --
            // grabWindow(0)'s x/y/w/h are relative to the screen's own
            // top-left, hence subtracting screen->geometry() (not
            // screenGeom itself) here.
            settleDesktopBeforeSnapshot();
            const QPoint availOffsetInScreen = screenGeom.topLeft() - screen->geometry().topLeft();
            m_background = screen->grabWindow(0, availOffsetInScreen.x(), availOffsetInScreen.y(),
                                               screenGeom.width(), screenGeom.height());
        }
        // Order matters on macOS: RegionSelectorOverlay's working pattern
        // is show() *then* setGeometry().
        show();
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

        // No-op when m_useRealTransparency is true (m_background is never
        // grabbed then, so it's a null QPixmap) -- the real screen shows
        // through this window's own transparency instead.
        p.drawPixmap(0, 0, m_background);

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
    bool m_useRealTransparency = false;
    QScreen *m_screen = nullptr;
    QPoint m_origin;
    QString m_name;
    QList<QRect> m_includeRegions;
    QList<QRect> m_excludeRegions;
    QPixmap m_background;
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

    const QList<QScreen *> screens = QGuiApplication::screens();

    // Reused across calls rather than destroyed and recreated every time --
    // an earlier version did that, and creating/destroying several
    // always-on-top native windows on every single list-selection change
    // was slow enough (particularly on macOS, where each is a real
    // NSWindow) to make the app appear to hang while selecting regions.
    // Only rebuilt when the number of screens actually changed (a monitor
    // connected/disconnected), which is rare; otherwise the existing
    // windows are just repositioned/repainted via setContent() below.
    if (m_screenWindows.size() != screens.size()) {
        qDeleteAll(m_screenWindows);
        m_screenWindows.clear();
        for (int i = 0; i < screens.size(); ++i)
            m_screenWindows.append(new RegionHighlightScreenWindow());
    }

    for (int i = 0; i < m_screenWindows.size(); ++i)
        m_screenWindows[i]->setContent(screens[i], m_name, m_includeRegions, m_excludeRegions);
}

void RegionHighlightOverlay::hide()
{
    // Just hidden, not destroyed -- see the comment in showRegion() about
    // why these windows are kept around and reused.
    for (RegionHighlightScreenWindow *w : m_screenWindows)
        w->hide();
}
