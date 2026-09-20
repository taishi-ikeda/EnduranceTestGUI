#include "platform/GlobalHotkey.h"

#include <QSocketNotifier>

#include <X11/Xlib.h>
#include <X11/keysym.h>

// Deliberately opens its own Xlib connection, separate from the one
// Automation_linux.cpp keeps for input synthesis/window queries -- this one
// needs to sit in an event-driven QSocketNotifier loop waiting for grabbed
// KeyPress events, which is a different usage pattern from that file's
// purely synchronous request/reply calls, so keeping them independent
// avoids tangling the two.
struct GlobalHotkey::Impl
{
    Display *dpy = nullptr;
    Window root = 0;
    unsigned int keycode = 0;
    unsigned int baseMods = 0;
    QSocketNotifier *notifier = nullptr;
};

namespace
{
// XGrabKey errors (e.g. BadAccess when the combo is already grabbed by
// another client) arrive asynchronously and Xlib's default handler aborts
// the process on them -- swallow them for the probe below instead. This is
// installed process-wide only for the brief, XSync()-bounded window around
// the grab attempts (see start()), not left in place afterward, so it
// doesn't mask unrelated X errors elsewhere in the app.
bool g_grabFailed = false;
int hotkeyErrorHandler(Display *, XErrorEvent *)
{
    g_grabFailed = true;
    return 0;
}
}  // namespace

GlobalHotkey::GlobalHotkey(QObject *parent) : QObject(parent), m_impl(new Impl) {}

GlobalHotkey::~GlobalHotkey()
{
    stop();
    delete m_impl;
}

bool GlobalHotkey::start()
{
    m_impl->dpy = XOpenDisplay(nullptr);
    if (!m_impl->dpy)
        return false;

    m_impl->root = DefaultRootWindow(m_impl->dpy);
    m_impl->keycode = XKeysymToKeycode(m_impl->dpy, XK_Escape);
    if (m_impl->keycode == 0) {
        XCloseDisplay(m_impl->dpy);
        m_impl->dpy = nullptr;
        return false;
    }
    m_impl->baseMods = ControlMask | Mod1Mask | ShiftMask;

    // X11 passive grabs match the modifier state exactly, so Caps Lock/Num
    // Lock being toggled on would otherwise silently make the hotkey stop
    // matching -- grab all four combinations of those two "lock" bits too.
    XErrorHandler previousHandler = XSetErrorHandler(hotkeyErrorHandler);
    const unsigned int lockVariants[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    bool anyGrabbed = false;
    for (unsigned int lock : lockVariants) {
        g_grabFailed = false;
        XGrabKey(m_impl->dpy, int(m_impl->keycode), m_impl->baseMods | lock, m_impl->root, True,
                  GrabModeAsync, GrabModeAsync);
        XSync(m_impl->dpy, False);  // force any BadAccess to arrive now, not later
        if (!g_grabFailed)
            anyGrabbed = true;
    }
    XSetErrorHandler(previousHandler);

    if (!anyGrabbed) {
        XCloseDisplay(m_impl->dpy);
        m_impl->dpy = nullptr;
        return false;
    }

    m_impl->notifier = new QSocketNotifier(ConnectionNumber(m_impl->dpy), QSocketNotifier::Read, this);
    connect(m_impl->notifier, &QSocketNotifier::activated, this, [this]() {
        while (XPending(m_impl->dpy) > 0) {
            XEvent ev;
            XNextEvent(m_impl->dpy, &ev);
            if (ev.type == KeyPress && ev.xkey.keycode == m_impl->keycode &&
                (ev.xkey.state & m_impl->baseMods) == m_impl->baseMods) {
                notifyTriggered();
            }
        }
    });
    return true;
}

void GlobalHotkey::stop()
{
    if (!m_impl->dpy)
        return;
    delete m_impl->notifier;
    m_impl->notifier = nullptr;
    // No explicit XUngrabKey: passive grabs are automatically released by
    // the X server when the owning client's connection closes.
    XCloseDisplay(m_impl->dpy);
    m_impl->dpy = nullptr;
}

void GlobalHotkey::notifyTriggered()
{
    emit triggered();
}
