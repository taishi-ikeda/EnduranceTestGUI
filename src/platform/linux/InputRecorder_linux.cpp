#include "platform/InputRecorder.h"

#include <QSocketNotifier>
#include <QTimer>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>

// Deliberately opens its own Xlib connection, the same rationale as
// GlobalHotkey_linux.cpp: this one sits in an event-driven QSocketNotifier
// loop, a different usage pattern from Automation_linux.cpp's purely
// synchronous request/reply calls.
//
// Uses XInput2 *raw* events (XI_RawButtonPress/Release, XI_RawKeyPress/
// Release) selected on the root window. Raw events are delivered to every
// client that selects them regardless of which window currently has focus
// or grabs the device -- unlike a normal passive grab, this neither
// requires nor causes any interference with the input actually reaching
// whatever window the user is interacting with (the target app, one of its
// dialogs, ...), which is exactly what recording setup actions "including
// dialogs" (SPEC.md 6.13追加実装及び修正依頼) needs.
struct InputRecorder::Impl
{
    Display *dpy = nullptr;
    Window root = 0;
    int xiOpcode = 0;
    QSocketNotifier *notifier = nullptr;
    // Tracked here (not via the shared Qt::ShiftModifier bit InputRecorder
    // itself tracks for Ctrl/Alt/Meta) because it's needed *before*
    // translating a keycode to a keysym/Qt::Key at all, to pick the
    // shifted vs. unshifted symbol -- see qtKeyFromKeycode() below.
    bool shiftHeld = false;
    // Fallback for XI_RawButtonRelease: confirmed via direct testing that
    // at least one virtual/nested X server (Xvfb, used by this project's
    // own Xvfb+openbox test environment -- see SPEC.md 8章/TEST_CASES.md)
    // reliably emits XI_RawButtonPress for a synthetic (XTestFakeButtonEvent)
    // click but never the matching XI_RawButtonRelease, even though the
    // ordinary *core* ButtonPress/ButtonRelease pair is delivered correctly
    // to actual windows the whole rest of this app's automation relies on.
    // Real key press/release raw events were unaffected in that same test,
    // so this appears specific to synthetic button release specifically,
    // not raw events in general. Since a recording session can't tell
    // which kind of X server it's running against, this timer polls
    // XQueryPointer's button mask while a button is down and synthesizes
    // the release itself the moment the mask clears, as a backstop
    // alongside (not instead of) the raw-event path above -- whichever
    // fires first wins; the other is a safe no-op (see
    // InputRecorder::notifyMouseButton()'s own m_buttonDown guard).
    QTimer *buttonPollTimer = nullptr;
};

namespace
{
// Maps a raw X11 keycode (as XkbKeycodeToKeysym's level-0/level-1 symbol)
// to a Qt::Key plus, for plain printable characters, the character itself.
// Covers exactly the set SetupAction/RandomActionEngine's own key-handling
// already recognizes elsewhere in this app (see Automation_linux.cpp's
// qtKeyToX11Keysym, the reverse of this) plus the four modifier keys this
// class tracks. Returns 0 (and leaves outText untouched) for anything else
// -- non-ASCII/dead-key input is out of scope (see InputRecorder.h).
int qtKeyFromKeycode(Display *dpy, unsigned int keycode, bool shiftHeld, QString *outText)
{
    const KeySym sym = XkbKeycodeToKeysym(dpy, keycode, 0, shiftHeld ? 1 : 0);
    switch (sym) {
    case XK_Tab: return Qt::Key_Tab;
    case XK_ISO_Left_Tab: return Qt::Key_Backtab;
    case XK_Return: return Qt::Key_Return;
    case XK_KP_Enter: return Qt::Key_Enter;
    case XK_Escape: return Qt::Key_Escape;
    case XK_BackSpace: return Qt::Key_Backspace;
    case XK_Delete: return Qt::Key_Delete;
    case XK_Left: return Qt::Key_Left;
    case XK_Right: return Qt::Key_Right;
    case XK_Up: return Qt::Key_Up;
    case XK_Down: return Qt::Key_Down;
    case XK_Shift_L: case XK_Shift_R: return Qt::Key_Shift;
    case XK_Control_L: case XK_Control_R: return Qt::Key_Control;
    case XK_Alt_L: case XK_Alt_R: return Qt::Key_Alt;
    case XK_Super_L: case XK_Super_R: return Qt::Key_Meta;
    default:
        if (sym >= 0x20 && sym <= 0x7e) {
            // ASCII range: keysym == Latin-1 code point == Qt::Key value,
            // the same identity Automation_linux.cpp's qtKeyToX11Keysym
            // relies on in the other direction.
            if (outText)
                *outText = QString(QChar(uint(sym)));
            return int(sym);
        }
        return 0;
    }
}
}  // namespace

InputRecorder::InputRecorder(QObject *parent) : QObject(parent), m_impl(new Impl)
{
    initSharedState();
}

InputRecorder::~InputRecorder()
{
    stop();
    delete m_impl;
}

bool InputRecorder::start()
{
    if (m_recording)
        return true;

    m_impl->dpy = XOpenDisplay(nullptr);
    if (!m_impl->dpy)
        return false;

    int event = 0, error = 0, major = 2, minor = 0;
    if (!XQueryExtension(m_impl->dpy, "XInputExtension", &m_impl->xiOpcode, &event, &error) ||
        XIQueryVersion(m_impl->dpy, &major, &minor) != Success) {
        XCloseDisplay(m_impl->dpy);
        m_impl->dpy = nullptr;
        return false;
    }

    m_impl->root = DefaultRootWindow(m_impl->dpy);
    m_impl->shiftHeld = false;

    unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(mask, XI_RawButtonPress);
    XISetMask(mask, XI_RawButtonRelease);
    XISetMask(mask, XI_RawKeyPress);
    XISetMask(mask, XI_RawKeyRelease);
    XIEventMask evmask;
    evmask.deviceid = XIAllMasterDevices;
    evmask.mask_len = sizeof(mask);
    evmask.mask = mask;
    XISelectEvents(m_impl->dpy, m_impl->root, &evmask, 1);
    XFlush(m_impl->dpy);

    m_recording = true;
    m_buttonDown = false;
    m_downButton = Qt::NoButton;
    m_heldModifiers = Qt::NoModifier;
    m_textBuffer.clear();

    // See the Impl::buttonPollTimer comment: backstop for XI_RawButtonRelease
    // not being delivered on at least one real X server this app is tested
    // against. 40ms is frequent enough that the fallback-detected release
    // lands within the same drag-vs-click "feel" as a real event would,
    // without polling aggressively while otherwise idle (only runs at all
    // while a button is actually down, which is brief).
    m_impl->buttonPollTimer = new QTimer(this);
    m_impl->buttonPollTimer->setInterval(40);
    connect(m_impl->buttonPollTimer, &QTimer::timeout, this, [this]() {
        if (!m_buttonDown || !m_impl->dpy) {
            m_impl->buttonPollTimer->stop();
            return;
        }
        Window rootRet, childRet;
        int rootX = 0, rootY = 0, winX = 0, winY = 0;
        unsigned int maskRet = 0;
        XQueryPointer(m_impl->dpy, m_impl->root, &rootRet, &childRet, &rootX, &rootY, &winX, &winY,
                      &maskRet);
        const unsigned int buttonBit = (m_downButton == Qt::LeftButton) ? Button1Mask : Button3Mask;
        if (!(maskRet & buttonBit)) {
            notifyMouseButton(m_downButton, /*pressed=*/false, QPoint(rootX, rootY));
            m_impl->buttonPollTimer->stop();
        }
    });

    m_impl->notifier = new QSocketNotifier(ConnectionNumber(m_impl->dpy), QSocketNotifier::Read, this);
    connect(m_impl->notifier, &QSocketNotifier::activated, this, [this]() {
        while (m_impl->dpy && XPending(m_impl->dpy) > 0) {
            XEvent ev;
            XNextEvent(m_impl->dpy, &ev);
            if (ev.xcookie.type != GenericEvent || ev.xcookie.extension != m_impl->xiOpcode ||
                !XGetEventData(m_impl->dpy, &ev.xcookie)) {
                continue;
            }
            auto *revent = static_cast<XIRawEvent *>(ev.xcookie.data);
            const bool pressed = ev.xcookie.evtype == XI_RawButtonPress || ev.xcookie.evtype == XI_RawKeyPress;

            if (ev.xcookie.evtype == XI_RawButtonPress || ev.xcookie.evtype == XI_RawButtonRelease) {
                // Wheel "clicks" arrive as button press/release pairs for
                // buttons 4-7 (standard X11 wheel mapping, the same one
                // Automation_linux.cpp's scroll() synthesizes when
                // *dispatching* a scroll -- see notifyWheelScroll()'s dx/dy
                // sign convention comment in InputRecorder.h). Recorded on
                // press only: the release is an inherent, immediate part of
                // the same physical/synthetic wheel "notch", not a
                // separately meaningful gesture the way a button hold is.
                if (pressed && (revent->detail == 4 || revent->detail == 5 || revent->detail == 6 ||
                                revent->detail == 7)) {
                    Window rootRet, childRet;
                    int rootX = 0, rootY = 0, winX = 0, winY = 0;
                    unsigned int maskRet = 0;
                    XQueryPointer(m_impl->dpy, m_impl->root, &rootRet, &childRet, &rootX, &rootY, &winX,
                                  &winY, &maskRet);
                    int dx = 0, dy = 0;
                    switch (revent->detail) {
                    case 4: dy = 1; break;
                    case 5: dy = -1; break;
                    case 6: dx = 1; break;
                    case 7: dx = -1; break;
                    }
                    notifyWheelScroll(QPoint(rootX, rootY), dx, dy);
                    XFreeEventData(m_impl->dpy, &ev.xcookie);
                    continue;
                }
                Qt::MouseButton button = Qt::NoButton;
                if (revent->detail == 1)
                    button = Qt::LeftButton;
                else if (revent->detail == 3)
                    button = Qt::RightButton;
                if (button != Qt::NoButton) {
                    Window rootRet, childRet;
                    int rootX = 0, rootY = 0, winX = 0, winY = 0;
                    unsigned int maskRet = 0;
                    XQueryPointer(m_impl->dpy, m_impl->root, &rootRet, &childRet, &rootX, &rootY, &winX,
                                  &winY, &maskRet);
                    notifyMouseButton(button, pressed, QPoint(rootX, rootY));
                    if (pressed)
                        m_impl->buttonPollTimer->start();
                    else
                        m_impl->buttonPollTimer->stop();
                }
            } else if (ev.xcookie.evtype == XI_RawKeyPress || ev.xcookie.evtype == XI_RawKeyRelease) {
                const unsigned int keycode = revent->detail;
                // Shift state must be updated *before* translating this
                // very keycode when it's Shift itself being pressed (so a
                // held Shift affects the *next* key), and the translation
                // of Shift_L/R itself doesn't depend on shift level anyway.
                QString text;
                const int qtKey = qtKeyFromKeycode(m_impl->dpy, keycode, m_impl->shiftHeld, &text);
                if (qtKey == Qt::Key_Shift)
                    m_impl->shiftHeld = pressed;
                if (qtKey != 0) {
                    notifyKeyEvent(qtKey, text, pressed);
                    if (pressed && qtKey == Qt::Key_Escape) {
                        // Deferred: tearing the hook down (closing m_impl->
                        // dpy, deleting this very QSocketNotifier) must not
                        // happen synchronously from inside its own
                        // activated() handler/the XPending() loop above.
                        QMetaObject::invokeMethod(this, &InputRecorder::notifyEscapePressed,
                                                   Qt::QueuedConnection);
                    }
                }
            }
            XFreeEventData(m_impl->dpy, &ev.xcookie);
        }
    });
    return true;
}

void InputRecorder::teardownHook()
{
    if (!m_impl->dpy)
        return;
    delete m_impl->buttonPollTimer;
    m_impl->buttonPollTimer = nullptr;
    delete m_impl->notifier;
    m_impl->notifier = nullptr;
    XCloseDisplay(m_impl->dpy);
    m_impl->dpy = nullptr;
}
