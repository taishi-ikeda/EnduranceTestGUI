#include "platform/PlatformAutomation.h"

#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QVector>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#ifdef HAVE_ATSPI
#include <atspi/atspi.h>
#endif

#include <errno.h>
#include <signal.h>
#include <unistd.h>

namespace PlatformAutomation
{

namespace
{

Display *display()
{
    static Display *d = XOpenDisplay(nullptr);
    return d;
}

QString atomText(Display *dpy, Window w, Atom atom, Atom utf8, bool *ok)
{
    *ok = false;
    Atom type;
    int format;
    unsigned long nItems, bytesAfter;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(dpy, w, atom, 0, 1024, False, AnyPropertyType, &type, &format,
                            &nItems, &bytesAfter, &data) != Success || !data) {
        return {};
    }
    QString result;
    if (type == utf8 || type == XA_STRING)
        result = QString::fromUtf8(reinterpret_cast<char *>(data), int(nItems));
    XFree(data);
    *ok = !result.isEmpty();
    return result;
}

qint64 windowPid(Display *dpy, Window w, Atom netWmPid)
{
    Atom type;
    int format;
    unsigned long nItems, bytesAfter;
    unsigned char *data = nullptr;
    qint64 pid = -1;
    if (XGetWindowProperty(dpy, w, netWmPid, 0, 1, False, XA_CARDINAL, &type, &format, &nItems,
                            &bytesAfter, &data) == Success && data) {
        if (nItems > 0)
            pid = *reinterpret_cast<long *>(data);
        XFree(data);
    }
    return pid;
}

bool windowScreenBounds(Display *dpy, Window w, QRect &outBounds)
{
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, w, &attrs))
        return false;
    if (attrs.map_state != IsViewable)
        return false;

    Window root = DefaultRootWindow(dpy);
    int absX = 0, absY = 0;
    Window child;
    if (!XTranslateCoordinates(dpy, w, root, 0, 0, &absX, &absY, &child))
        return false;

    if (attrs.width < 10 || attrs.height < 10)
        return false;

    outBounds = QRect(absX, absY, attrs.width, attrs.height);
    return true;
}

QList<WindowInfo> collectWindows()
{
    QList<WindowInfo> result;
    Display *dpy = display();
    if (!dpy)
        return result;

    Window root = DefaultRootWindow(dpy);
    Atom netClientList = XInternAtom(dpy, "_NET_CLIENT_LIST", True);
    Atom netWmPid = XInternAtom(dpy, "_NET_WM_PID", True);
    Atom netWmName = XInternAtom(dpy, "_NET_WM_NAME", True);
    Atom utf8String = XInternAtom(dpy, "UTF8_STRING", True);
    if (netClientList == None)
        return result;

    Atom type;
    int format;
    unsigned long nItems, bytesAfter;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(dpy, root, netClientList, 0, ~0L, False, XA_WINDOW, &type, &format,
                            &nItems, &bytesAfter, &data) != Success || !data) {
        return result;
    }

    Window *windows = reinterpret_cast<Window *>(data);
    const pid_t selfPid = getpid();

    for (unsigned long i = 0; i < nItems; ++i) {
        Window w = windows[i];

        qint64 pid = netWmPid != None ? windowPid(dpy, w, netWmPid) : -1;
        if (pid == selfPid)
            continue;

        QRect bounds;
        if (!windowScreenBounds(dpy, w, bounds))
            continue;

        QString title;
        bool ok = false;
        if (netWmName != None)
            title = atomText(dpy, w, netWmName, utf8String, &ok);
        if (!ok) {
            char *name = nullptr;
            if (XFetchName(dpy, w, &name) && name) {
                title = QString::fromUtf8(name);
                XFree(name);
            }
        }

        QString appName = title;
        XClassHint classHint;
        if (XGetClassHint(dpy, w, &classHint)) {
            if (classHint.res_class)
                appName = QString::fromUtf8(classHint.res_class);
            if (classHint.res_name)
                XFree(classHint.res_name);
            if (classHint.res_class)
                XFree(classHint.res_class);
        }

        WindowInfo info;
        info.pid = pid;
        info.windowId = quint32(w);
        info.appName = appName;
        info.title = title;
        info.bounds = bounds;
        result.append(info);
    }

    XFree(data);
    return result;
}

unsigned int buttonNumber(Qt::MouseButton button)
{
    return button == Qt::RightButton ? 3 : 1;
}

// Named/functional keys have no printable character of their own, so their
// Qt::Key values don't line up numerically with an X11 keysym the way
// letters/digits do (see keyShortcut()'s comment). Used by both
// keyShortcut() and keyTapNamed().
KeySym qtKeyToX11Keysym(Qt::Key key)
{
    switch (key) {
    case Qt::Key_Tab: return XK_Tab;
    case Qt::Key_Backtab: return XK_ISO_Left_Tab;
    case Qt::Key_Return: return XK_Return;
    case Qt::Key_Enter: return XK_KP_Enter;
    case Qt::Key_Escape: return XK_Escape;
    case Qt::Key_Backspace: return XK_BackSpace;
    case Qt::Key_Delete: return XK_Delete;
    case Qt::Key_Left: return XK_Left;
    case Qt::Key_Right: return XK_Right;
    case Qt::Key_Up: return XK_Up;
    case Qt::Key_Down: return XK_Down;
    case Qt::Key_Space: return XK_space;
    default:
        // Letters/digits/ASCII punctuation: Qt::Key values in this range
        // are the same as their Latin-1 code points, which are also valid
        // X11 keysyms directly.
        return KeySym(key);
    }
}

}  // namespace

bool isAccessibilityTrusted(bool /*promptIfNeeded*/)
{
    Display *dpy = display();
    if (!dpy)
        return false;
    int major, minor, event, error;
    return XTestQueryExtension(dpy, &event, &error, &major, &minor);
}

void openAccessibilitySettings()
{
    // No OS-level permission gate on X11; nothing to open.
}

QList<WindowInfo> listWindows()
{
    return collectWindows();
}

bool queryWindowBounds(quint32 windowId, qint64 pid, QRect &outBounds)
{
    const QList<WindowInfo> windows = collectWindows();
    for (const WindowInfo &w : windows) {
        if (w.windowId == windowId && w.pid == pid) {
            outBounds = w.bounds;
            return true;
        }
    }
    return false;
}

// Unlike collectWindows(), this isn't filtered to only currently-viewable
// (mapped) windows, so it can still find a window that's been minimized/
// iconified -- needed so activateProcess() can restore one.
static Window findAnyWindowForPid(Display *dpy, qint64 pid)
{
    Atom netClientList = XInternAtom(dpy, "_NET_CLIENT_LIST", True);
    Atom netWmPid = XInternAtom(dpy, "_NET_WM_PID", True);
    if (netClientList == None || netWmPid == None)
        return 0;

    Atom type;
    int format;
    unsigned long nItems, bytesAfter;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), netClientList, 0, ~0L, False, XA_WINDOW,
                            &type, &format, &nItems, &bytesAfter, &data) != Success || !data) {
        return 0;
    }

    Window *windows = reinterpret_cast<Window *>(data);
    Window target = 0;
    for (unsigned long i = 0; i < nItems; ++i) {
        if (windowPid(dpy, windows[i], netWmPid) == pid) {
            target = windows[i];
            break;
        }
    }
    XFree(data);
    return target;
}

bool activateProcess(qint64 pid)
{
    Display *dpy = display();
    if (!dpy)
        return false;

    const Window target = findAnyWindowForPid(dpy, pid);
    if (!target)
        return false;

    // Restore it if it was minimized/iconified (XIconifyWindow's
    // counterpart is simply mapping the window back).
    XMapWindow(dpy, target);

    Atom netActiveWindow = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True);
    if (netActiveWindow != None) {
        XEvent event = {};
        event.xclient.type = ClientMessage;
        event.xclient.window = target;
        event.xclient.message_type = netActiveWindow;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 1;  // source: application
        event.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, DefaultRootWindow(dpy), False,
                   SubstructureNotifyMask | SubstructureRedirectMask, &event);
    }
    XRaiseWindow(dpy, target);
    XSetInputFocus(dpy, target, RevertToParent, CurrentTime);
    XFlush(dpy);
    return true;
}

bool isProcessRunning(qint64 pid)
{
    return kill((pid_t)pid, 0) == 0 || errno == EPERM;
}

ProcessStats queryProcessStats(qint64 pid)
{
    ProcessStats stats;

    QFile statusFile(QStringLiteral("/proc/%1/status").arg(pid));
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QByteArray content = statusFile.readAll();
        for (const QByteArray &line : content.split('\n')) {
            if (line.startsWith("VmRSS:")) {
                const QByteArray digits = line.mid(6).trimmed().split(' ').first();
                stats.residentMemoryMB = digits.toDouble() / 1024.0;  // VmRSS is in kB
                break;
            }
        }
    }

    QFile statFile(QStringLiteral("/proc/%1/stat").arg(pid));
    if (statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Fields are space-separated; the process name (field 2) is in
        // parentheses and may itself contain spaces, so split after it.
        const QString content = QString::fromUtf8(statFile.readAll());
        const int closeParen = content.lastIndexOf(')');
        if (closeParen >= 0) {
            const QStringList rest = content.mid(closeParen + 1).trimmed().split(' ');
            // After the name: fields 3.. => rest[0] is field 3 (state).
            // utime = field 14 = rest[11], stime = field 15 = rest[12].
            if (rest.size() > 12) {
                const long clockTicksPerSec = sysconf(_SC_CLK_TCK) > 0 ? sysconf(_SC_CLK_TCK) : 100;
                const double utime = rest[11].toDouble();
                const double stime = rest[12].toDouble();
                stats.cpuTimeSeconds = (utime + stime) / double(clockTicksPerSec);
            }
        }
    }

    stats.ok = statusFile.isOpen() || statFile.isOpen() ||
               QFile::exists(QStringLiteral("/proc/%1").arg(pid));
    return stats;
}

qint64 windowPidAtPoint(const QPoint &pt)
{
    Display *dpy = display();
    if (!dpy)
        return -1;

    Atom netClientListStacking = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", True);
    if (netClientListStacking == None)
        return -1;
    Atom netWmPid = XInternAtom(dpy, "_NET_WM_PID", True);

    Atom type;
    int format;
    unsigned long nItems, bytesAfter;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), netClientListStacking, 0, ~0L, False,
                            XA_WINDOW, &type, &format, &nItems, &bytesAfter, &data) != Success ||
        !data) {
        return -1;
    }

    Window *windows = reinterpret_cast<Window *>(data);
    const pid_t selfPid = getpid();
    qint64 result = -1;

    // _NET_CLIENT_LIST_STACKING is bottom-to-top, so scan in reverse to
    // find the topmost window at this point first.
    for (long i = long(nItems) - 1; i >= 0; --i) {
        const Window w = windows[i];
        const qint64 pid = netWmPid != None ? windowPid(dpy, w, netWmPid) : -1;
        if (pid == selfPid)
            continue;
        QRect bounds;
        if (!windowScreenBounds(dpy, w, bounds))
            continue;
        if (bounds.contains(pt)) {
            result = pid;
            break;
        }
    }

    XFree(data);
    return result;
}

qint64 activeProcessPid()
{
    Display *dpy = display();
    if (!dpy)
        return -1;

    Atom netActiveWindow = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True);
    if (netActiveWindow == None)
        return -1;

    Atom type;
    int format;
    unsigned long nItems, bytesAfter;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), netActiveWindow, 0, 1, False, XA_WINDOW,
                            &type, &format, &nItems, &bytesAfter, &data) != Success ||
        !data) {
        return -1;
    }

    const Window activeWindow = nItems > 0 ? *reinterpret_cast<Window *>(data) : 0;
    XFree(data);
    if (!activeWindow)
        return -1;

    Atom netWmPid = XInternAtom(dpy, "_NET_WM_PID", True);
    return netWmPid != None ? windowPid(dpy, activeWindow, netWmPid) : -1;
}

void moveMouse(const QPoint &pt)
{
    Display *dpy = display();
    if (!dpy)
        return;
    XTestFakeMotionEvent(dpy, -1, pt.x(), pt.y(), CurrentTime);
    XFlush(dpy);
}

void mouseClick(const QPoint &pt, Qt::MouseButton button)
{
    Display *dpy = display();
    if (!dpy)
        return;
    const unsigned int btn = buttonNumber(button);

    XTestFakeMotionEvent(dpy, -1, pt.x(), pt.y(), CurrentTime);
    XFlush(dpy);
    XTestFakeButtonEvent(dpy, btn, True, CurrentTime);
    XFlush(dpy);
    usleep(15000);
    XTestFakeButtonEvent(dpy, btn, False, CurrentTime);
    XFlush(dpy);
}

void mouseDrag(const QPoint &from, const QPoint &to, Qt::MouseButton button, int steps)
{
    Display *dpy = display();
    if (!dpy)
        return;
    const unsigned int btn = buttonNumber(button);

    XTestFakeMotionEvent(dpy, -1, from.x(), from.y(), CurrentTime);
    XFlush(dpy);
    XTestFakeButtonEvent(dpy, btn, True, CurrentTime);
    XFlush(dpy);

    steps = qMax(1, steps);
    for (int i = 1; i <= steps; ++i) {
        const qreal t = qreal(i) / qreal(steps);
        const int x = from.x() + qRound((to.x() - from.x()) * t);
        const int y = from.y() + qRound((to.y() - from.y()) * t);
        XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);
        XFlush(dpy);
        usleep(8000);
    }

    XTestFakeButtonEvent(dpy, btn, False, CurrentTime);
    XFlush(dpy);
}

void scroll(const QPoint &pt, int dx, int dy)
{
    Display *dpy = display();
    if (!dpy)
        return;

    XTestFakeMotionEvent(dpy, -1, pt.x(), pt.y(), CurrentTime);
    XFlush(dpy);

    // Wheel "clicks" are synthesized as button presses: 4/5 = vertical
    // up/down, 6/7 = horizontal left/right (standard X11 wheel mapping).
    const unsigned int vButton = dy >= 0 ? 4 : 5;
    for (int i = 0; i < qAbs(dy); ++i) {
        XTestFakeButtonEvent(dpy, vButton, True, CurrentTime);
        XTestFakeButtonEvent(dpy, vButton, False, CurrentTime);
    }
    const unsigned int hButton = dx >= 0 ? 6 : 7;
    for (int i = 0; i < qAbs(dx); ++i) {
        XTestFakeButtonEvent(dpy, hButton, True, CurrentTime);
        XTestFakeButtonEvent(dpy, hButton, False, CurrentTime);
    }
    XFlush(dpy);
}

void keyTap(QChar ch)
{
    Display *dpy = display();
    if (!dpy)
        return;

    const KeySym keysym = KeySym(ch.unicode());
    const KeyCode keycode = XKeysymToKeycode(dpy, keysym);
    if (keycode == 0)
        return;

    const KeySym baseKeysym = XkbKeycodeToKeysym(dpy, keycode, 0, 0);
    const bool needsShift = (baseKeysym != keysym);

    if (needsShift) {
        const KeyCode shiftCode = XKeysymToKeycode(dpy, XK_Shift_L);
        XTestFakeKeyEvent(dpy, shiftCode, True, CurrentTime);
    }
    XTestFakeKeyEvent(dpy, keycode, True, CurrentTime);
    XFlush(dpy);
    usleep(10000);
    XTestFakeKeyEvent(dpy, keycode, False, CurrentTime);
    if (needsShift) {
        const KeyCode shiftCode = XKeysymToKeycode(dpy, XK_Shift_L);
        XTestFakeKeyEvent(dpy, shiftCode, False, CurrentTime);
    }
    XFlush(dpy);
}

void dismissContextMenu()
{
    Display *dpy = display();
    if (!dpy)
        return;
    const KeyCode keycode = XKeysymToKeycode(dpy, XK_Escape);
    if (keycode == 0)
        return;
    XTestFakeKeyEvent(dpy, keycode, True, CurrentTime);
    XFlush(dpy);
    usleep(10000);
    XTestFakeKeyEvent(dpy, keycode, False, CurrentTime);
    XFlush(dpy);
}

#ifdef HAVE_ATSPI

namespace
{
// Experimental: best-effort search of the AT-SPI accessibility tree for a
// currently-visible popup/context menu (role MENU, state VISIBLE). Bounds
// the traversal (depth and total nodes visited) since walking the entire
// desktop tree could otherwise be slow. Requires the app that opened the
// menu to expose it via its toolkit's AT-SPI bridge (GTK/Qt accessibility);
// not guaranteed to work with every app. Returns a new reference the
// caller must g_object_unref, or nullptr if none found.
AtspiAccessible *findOpenMenuRecursive(AtspiAccessible *node, int depth, int &budget)
{
    if (!node || depth > 15 || budget <= 0)
        return nullptr;
    --budget;

    AtspiRole role = atspi_accessible_get_role(node, nullptr);
    if (role == ATSPI_ROLE_MENU) {
        AtspiStateSet *states = atspi_accessible_get_state_set(node);
        const bool visible = states && atspi_state_set_contains(states, ATSPI_STATE_VISIBLE);
        if (states)
            g_object_unref(states);
        if (visible) {
            g_object_ref(node);
            return node;
        }
    }

    const gint childCount = atspi_accessible_get_child_count(node, nullptr);
    for (gint i = 0; i < childCount && budget > 0; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, nullptr);
        if (!child)
            continue;
        AtspiAccessible *found = findOpenMenuRecursive(child, depth + 1, budget);
        g_object_unref(child);
        if (found)
            return found;
    }
    return nullptr;
}

AtspiAccessible *findOpenMenu()
{
    static bool inited = false;
    if (!inited) {
        atspi_init();
        inited = true;
    }
    AtspiAccessible *desktop = atspi_get_desktop(0);
    if (!desktop)
        return nullptr;
    int budget = 4000;
    AtspiAccessible *menu = findOpenMenuRecursive(desktop, 0, budget);
    g_object_unref(desktop);
    return menu;
}
}  // namespace

QStringList listOpenContextMenuItems(qint64 /*expectedOwnerPid*/)
{
    // Not scoped to expectedOwnerPid: see PlatformAutomation.h -- the
    // caller's active-process focus check is the primary guard on Linux.
    QStringList names;
    AtspiAccessible *menu = findOpenMenu();
    if (!menu)
        return names;

    const gint childCount = atspi_accessible_get_child_count(menu, nullptr);
    for (gint i = 0; i < childCount; ++i) {
        AtspiAccessible *item = atspi_accessible_get_child_at_index(menu, i, nullptr);
        if (!item)
            continue;
        gchar *name = atspi_accessible_get_name(item, nullptr);
        if (name) {
            names.append(QString::fromUtf8(name));
            g_free(name);
        }
        g_object_unref(item);
    }
    g_object_unref(menu);
    return names;
}

bool clickContextMenuItem(const QString &itemName, qint64 /*expectedOwnerPid*/)
{
    AtspiAccessible *menu = findOpenMenu();
    if (!menu)
        return false;

    bool clicked = false;
    const gint childCount = atspi_accessible_get_child_count(menu, nullptr);
    for (gint i = 0; i < childCount && !clicked; ++i) {
        AtspiAccessible *item = atspi_accessible_get_child_at_index(menu, i, nullptr);
        if (!item)
            continue;
        gchar *name = atspi_accessible_get_name(item, nullptr);
        if (name && itemName == QString::fromUtf8(name)) {
            AtspiAction *action = atspi_accessible_get_action_iface(item);
            if (action) {
                clicked = atspi_action_do_action(action, 0, nullptr);
                g_object_unref(action);
            }
        }
        if (name)
            g_free(name);
        g_object_unref(item);
    }
    g_object_unref(menu);
    return clicked;
}

bool clickContextMenuItemAt(int index, qint64 /*expectedOwnerPid*/)
{
    AtspiAccessible *menu = findOpenMenu();
    if (!menu)
        return false;

    bool clicked = false;
    const gint childCount = atspi_accessible_get_child_count(menu, nullptr);
    if (index >= 0 && index < childCount) {
        AtspiAccessible *item = atspi_accessible_get_child_at_index(menu, index, nullptr);
        if (item) {
            AtspiAction *action = atspi_accessible_get_action_iface(item);
            if (action) {
                clicked = atspi_action_do_action(action, 0, nullptr);
                g_object_unref(action);
            }
            g_object_unref(item);
        }
    }
    g_object_unref(menu);
    return clicked;
}

#else  // !HAVE_ATSPI

QStringList listOpenContextMenuItems(qint64 /*expectedOwnerPid*/)
{
    return {};  // AT-SPI not available at build time; see CMakeLists.txt.
}

bool clickContextMenuItem(const QString & /*itemName*/, qint64 /*expectedOwnerPid*/)
{
    return false;
}

bool clickContextMenuItemAt(int /*index*/, qint64 /*expectedOwnerPid*/)
{
    return false;
}

#endif  // HAVE_ATSPI

void keyShortcut(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    Display *dpy = display();
    if (!dpy)
        return;

    const KeySym keysym = qtKeyToX11Keysym(key);
    const KeyCode keycode = XKeysymToKeycode(dpy, keysym);
    if (keycode == 0)
        return;

    QVector<KeyCode> modifierCodes;
    if (modifiers & Qt::ShiftModifier)
        modifierCodes.append(XKeysymToKeycode(dpy, XK_Shift_L));
    if (modifiers & Qt::ControlModifier)
        modifierCodes.append(XKeysymToKeycode(dpy, XK_Control_L));
    if (modifiers & Qt::AltModifier)
        modifierCodes.append(XKeysymToKeycode(dpy, XK_Alt_L));
    if (modifiers & Qt::MetaModifier)
        modifierCodes.append(XKeysymToKeycode(dpy, XK_Super_L));

    for (KeyCode code : modifierCodes)
        XTestFakeKeyEvent(dpy, code, True, CurrentTime);

    XTestFakeKeyEvent(dpy, keycode, True, CurrentTime);
    XFlush(dpy);
    usleep(10000);
    XTestFakeKeyEvent(dpy, keycode, False, CurrentTime);

    for (KeyCode code : modifierCodes)
        XTestFakeKeyEvent(dpy, code, False, CurrentTime);
    XFlush(dpy);
}

void keyTapNamed(Qt::Key key)
{
    Display *dpy = display();
    if (!dpy)
        return;

    const KeySym keysym = qtKeyToX11Keysym(key);
    const KeyCode keycode = XKeysymToKeycode(dpy, keysym);
    if (keycode == 0)
        return;

    XTestFakeKeyEvent(dpy, keycode, True, CurrentTime);
    XFlush(dpy);
    usleep(10000);
    XTestFakeKeyEvent(dpy, keycode, False, CurrentTime);
    XFlush(dpy);
}

// --- Window-level operations ---------------------------------------------

static bool verifyWindowOwner(Display *dpy, Window w, qint64 expectedPid)
{
    Atom netWmPid = XInternAtom(dpy, "_NET_WM_PID", True);
    if (netWmPid == None)
        return false;
    return windowPid(dpy, w, netWmPid) == expectedPid;
}

bool moveWindow(qint64 pid, quint32 windowId, const QPoint &newTopLeft)
{
    Display *dpy = display();
    if (!dpy)
        return false;
    const Window w = Window(windowId);
    if (!verifyWindowOwner(dpy, w, pid))
        return false;
    XMoveWindow(dpy, w, newTopLeft.x(), newTopLeft.y());
    XFlush(dpy);
    return true;
}

bool resizeWindow(qint64 pid, quint32 windowId, const QSize &newSize)
{
    Display *dpy = display();
    if (!dpy)
        return false;
    const Window w = Window(windowId);
    if (!verifyWindowOwner(dpy, w, pid))
        return false;
    XResizeWindow(dpy, w, (unsigned int)qMax(1, newSize.width()),
                  (unsigned int)qMax(1, newSize.height()));
    XFlush(dpy);
    return true;
}

bool minimizeWindow(qint64 pid, quint32 windowId)
{
    Display *dpy = display();
    if (!dpy)
        return false;
    const Window w = Window(windowId);
    if (!verifyWindowOwner(dpy, w, pid))
        return false;
    // XIconifyWindow is the standard ICCCM way to request minimization;
    // widely supported across window managers.
    return XIconifyWindow(dpy, w, DefaultScreen(dpy)) != 0;
}

bool maximizeWindow(qint64 pid, quint32 windowId)
{
    Display *dpy = display();
    if (!dpy)
        return false;
    const Window w = Window(windowId);
    if (!verifyWindowOwner(dpy, w, pid))
        return false;

    QRect bounds;
    if (!windowScreenBounds(dpy, w, bounds))
        return false;

    // Use Qt's own screen geometry (already available since this app links
    // Qt Widgets) rather than querying Xinerama/RandR directly -- simpler,
    // and consistent with how the rest of the app reasons about screens.
    QScreen *best = nullptr;
    int bestArea = -1;
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect intersection = screen->geometry().intersected(bounds);
        const int area = intersection.width() * intersection.height();
        if (area > bestArea) {
            bestArea = area;
            best = screen;
        }
    }
    if (!best)
        return false;

    const QRect target = best->availableGeometry();
    XMoveResizeWindow(dpy, w, target.x(), target.y(), (unsigned int)target.width(),
                       (unsigned int)target.height());
    XFlush(dpy);
    return true;
}

}  // namespace PlatformAutomation
