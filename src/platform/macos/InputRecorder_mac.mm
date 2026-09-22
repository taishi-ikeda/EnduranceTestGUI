#include "platform/InputRecorder.h"

#import <ApplicationServices/ApplicationServices.h>
// TISCopyCurrentKeyboardLayoutInputSource/TISGetInputSourceProperty/
// kTISPropertyUnicodeKeyLayoutData (Text Input Sources) and the deprecated
// LMGetKbdType() used by qtKeyFromCGKeyCode() below are declared here, not
// in ApplicationServices -- same header Automation_mac.mm already needs for
// its own virtual-keycode handling.
#import <Carbon/Carbon.h>

// Not independently verified on real macOS hardware (see SPEC.md 8章, same
// caveat as the rest of this app's macOS backend). Mirrors
// InputRecorder_linux.cpp's design and doc comments; see InputRecorder.h
// for the shared click/drag/text-buffering logic this feeds into.
//
// Uses a CGEventTap installed with kCGEventTapOptionListenOnly at the
// kCGHIDEventTap location. "Listen only" is the macOS equivalent of the
// Linux backend's XInput2 *raw* events: the tap only observes events, never
// consumes/blocks/rewrites them, so input still reaches whichever
// application/dialog the user is actually interacting with -- required for
// recording to work against the target app's own dialogs, not just
// EnduranceTestGUI's own window. Requires the same Accessibility
// permission this app already needs for automation (SPEC.md 5章); if that
// permission isn't granted, CGEventTapCreate returns nullptr and start()
// reports failure the same way the rest of this app's permission-gated
// features do.
struct InputRecorder::Impl
{
    CFMachPortRef tap = nullptr;
    CFRunLoopSourceRef runLoopSource = nullptr;
};

namespace
{
// Maps a macOS virtual keycode (CGKeyCode) to a Qt::Key plus, for plain
// printable characters, the character itself -- the macOS counterpart of
// InputRecorder_linux.cpp's qtKeyFromKeycode(). Covers the same set: the
// named keys SetupAction/RandomActionEngine recognize elsewhere, the four
// modifier keys this class tracks, and printable ASCII. Layout-dependent
// (assumes a US-like physical layout for the letter/digit rows, via
// UCKeyTranslate against the current keyboard layout) rather than a fixed
// keycode table, so it degrades gracefully on non-US layouts for the named
// keys (fixed virtual keycodes on Mac hardware) while still doing its best
// for printable characters.
int qtKeyFromCGKeyCode(CGKeyCode keycode, CGEventFlags flags, QString *outText)
{
    switch (keycode) {
    case 48: return Qt::Key_Tab;
    case 36: return Qt::Key_Return;
    case 76: return Qt::Key_Enter;
    case 53: return Qt::Key_Escape;
    case 51: return Qt::Key_Backspace;
    case 117: return Qt::Key_Delete;
    case 123: return Qt::Key_Left;
    case 124: return Qt::Key_Right;
    case 126: return Qt::Key_Up;
    case 125: return Qt::Key_Down;
    case 56: case 60: return Qt::Key_Shift;
    case 59: case 62: return Qt::Key_Control;
    case 58: case 61: return Qt::Key_Alt;
    case 55: case 54: return Qt::Key_Meta;
    default:
        break;
    }

    // Printable character: translate via the current keyboard layout so
    // Shift-produced symbols (e.g. Shift+1 -> '!' on a US layout) resolve
    // correctly, the same intent as the Linux backend's shift-aware
    // XkbKeycodeToKeysym level argument.
    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
    if (!source)
        return 0;
    CFDataRef layoutData = static_cast<CFDataRef>(
        TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
    if (!layoutData) {
        CFRelease(source);
        return 0;
    }
    const UCKeyboardLayout *layout =
        reinterpret_cast<const UCKeyboardLayout *>(CFDataGetBytePtr(layoutData));
    UInt32 deadKeyState = 0;
    UniChar chars[4];
    UniCharCount length = 0;
    const UInt32 modifierKeyState = (flags & kCGEventFlagMaskShift) ? (1 << 1) : 0;
    UCKeyTranslate(layout, keycode, kUCKeyActionDown, modifierKeyState, LMGetKbdType(),
                    kUCKeyTranslateNoDeadKeysBit, &deadKeyState, 4, &length, chars);
    CFRelease(source);
    if (length == 0)
        return 0;
    const QChar ch(chars[0]);
    if (ch.unicode() < 0x20 || ch.unicode() > 0x7e)
        return 0;  // out of scope, see InputRecorder.h
    if (outText)
        *outText = QString(ch);
    return int(ch.unicode());
}
}  // namespace

InputRecorder::InputRecorder(QObject *parent) : QObject(parent), m_impl(new Impl) {}

InputRecorder::~InputRecorder()
{
    stop();
    delete m_impl;
}

namespace
{
CGEventRef inputRecorderTapCallback(CGEventTapProxy, CGEventType type, CGEventRef event, void *refcon)
{
    auto *self = static_cast<InputRecorder *>(refcon);
    const CGPoint loc = CGEventGetLocation(event);
    const QPoint screenPos(int(loc.x), int(loc.y));

    if (type == kCGEventLeftMouseDown || type == kCGEventLeftMouseUp) {
        self->notifyMouseButton(Qt::LeftButton, type == kCGEventLeftMouseDown, screenPos);
    } else if (type == kCGEventRightMouseDown || type == kCGEventRightMouseUp) {
        self->notifyMouseButton(Qt::RightButton, type == kCGEventRightMouseDown, screenPos);
    } else if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
        const CGKeyCode keycode =
            CGKeyCode(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
        const CGEventFlags flags = CGEventGetFlags(event);
        QString text;
        const int qtKey = qtKeyFromCGKeyCode(keycode, flags, &text);
        if (qtKey != 0 && qtKey != Qt::Key_Escape) {
            self->notifyKeyEvent(qtKey, text, type == kCGEventKeyDown);
        } else if (qtKey == Qt::Key_Escape && type == kCGEventKeyDown) {
            self->notifyKeyEvent(qtKey, text, true);
            // See InputRecorder_linux.cpp's identical rationale: tearing
            // the tap down must not happen synchronously from inside this
            // very callback.
            QMetaObject::invokeMethod(self, &InputRecorder::notifyEscapePressed, Qt::QueuedConnection);
        }
    } else if (type == kCGEventFlagsChanged) {
        // Modifier key press/release arrives as a single flags-changed
        // event rather than distinct key-down/up for the modifier itself;
        // recover which modifier and whether it went down or up by
        // checking its own bit in the current flags.
        const CGKeyCode keycode =
            CGKeyCode(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
        QString unused;
        const int qtKey = qtKeyFromCGKeyCode(keycode, 0, &unused);
        if (qtKey == Qt::Key_Shift || qtKey == Qt::Key_Control || qtKey == Qt::Key_Alt ||
            qtKey == Qt::Key_Meta) {
            CGEventFlags mask = 0;
            switch (qtKey) {
            case Qt::Key_Shift: mask = kCGEventFlagMaskShift; break;
            case Qt::Key_Control: mask = kCGEventFlagMaskControl; break;
            case Qt::Key_Alt: mask = kCGEventFlagMaskAlternate; break;
            case Qt::Key_Meta: mask = kCGEventFlagMaskCommand; break;
            default: break;
            }
            const bool pressed = (CGEventGetFlags(event) & mask) != 0;
            self->notifyKeyEvent(qtKey, QString(), pressed);
        }
    }

    // Listen-only tap: returning the event unmodified is required (a
    // listen-only tap's return value is actually ignored by the OS, but
    // returning it rather than nullptr keeps this callback correct even if
    // the tap type were ever changed).
    return event;
}
}  // namespace

bool InputRecorder::start()
{
    if (m_recording)
        return true;

    CGEventMask mask = CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventLeftMouseUp) |
                        CGEventMaskBit(kCGEventRightMouseDown) | CGEventMaskBit(kCGEventRightMouseUp) |
                        CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                        CGEventMaskBit(kCGEventFlagsChanged);
    m_impl->tap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
                                    mask, inputRecorderTapCallback, this);
    if (!m_impl->tap)
        return false;

    m_impl->runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_impl->tap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), m_impl->runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(m_impl->tap, true);

    m_recording = true;
    m_buttonDown = false;
    m_downButton = Qt::NoButton;
    m_heldModifiers = Qt::NoModifier;
    m_textBuffer.clear();
    return true;
}

void InputRecorder::teardownHook()
{
    if (!m_impl->tap)
        return;
    CGEventTapEnable(m_impl->tap, false);
    if (m_impl->runLoopSource) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), m_impl->runLoopSource, kCFRunLoopCommonModes);
        CFRelease(m_impl->runLoopSource);
        m_impl->runLoopSource = nullptr;
    }
    CFRelease(m_impl->tap);
    m_impl->tap = nullptr;
}
