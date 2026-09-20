#include "platform/PlatformAutomation.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>

#include <libproc.h>
#include <sys/proc_info.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>

namespace PlatformAutomation
{

bool isAccessibilityTrusted(bool promptIfNeeded)
{
    NSDictionary *options = @{
        (__bridge id)kAXTrustedCheckOptionPrompt : promptIfNeeded ? @YES : @NO
    };
    return AXIsProcessTrustedWithOptions((CFDictionaryRef)options);
}

void openAccessibilitySettings()
{
    NSString *urlString =
        @"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility";
    NSURL *url = [NSURL URLWithString:urlString];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

static QList<WindowInfo> collectWindows()
{
    QList<WindowInfo> result;

    CFArrayRef cfWindowList = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!cfWindowList)
        return result;

    NSArray *windowList = (__bridge NSArray *)cfWindowList;
    const pid_t selfPid = getpid();

    for (NSDictionary *info in windowList) {
        NSNumber *layerNum = info[(id)kCGWindowLayer];
        if (!layerNum || layerNum.intValue != 0)
            continue;  // only "normal" application windows

        NSNumber *pidNum = info[(id)kCGWindowOwnerPID];
        if (!pidNum)
            continue;
        pid_t ownerPid = pidNum.intValue;
        if (ownerPid == selfPid)
            continue;

        NSDictionary *boundsDict = info[(id)kCGWindowBounds];
        if (!boundsDict)
            continue;
        CGRect cgBounds;
        if (!CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)boundsDict, &cgBounds))
            continue;
        if (cgBounds.size.width < 10 || cgBounds.size.height < 10)
            continue;

        NSNumber *windowNumberNum = info[(id)kCGWindowNumber];
        NSString *ownerName = info[(id)kCGWindowOwnerName];
        NSString *windowName = info[(id)kCGWindowName];  // may be nil without Screen Recording permission

        WindowInfo w;
        w.pid = ownerPid;
        w.windowId = windowNumberNum ? windowNumberNum.unsignedIntValue : 0;
        w.appName = ownerName ? QString::fromNSString(ownerName) : QString();
        w.title = windowName ? QString::fromNSString(windowName) : QString();
        w.bounds = QRect(qRound(cgBounds.origin.x), qRound(cgBounds.origin.y),
                          qRound(cgBounds.size.width), qRound(cgBounds.size.height));
        result.append(w);
    }

    CFRelease(cfWindowList);
    return result;
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

QList<quint32> listWindowIdsForPid(qint64 pid)
{
    QList<quint32> result;
    for (const WindowInfo &w : collectWindows()) {
        if (w.pid == pid)
            result.append(w.windowId);
    }
    return result;
}

bool activateProcess(qint64 pid)
{
    NSRunningApplication *app =
        [NSRunningApplication runningApplicationWithProcessIdentifier:(pid_t)pid];
    if (!app)
        return false;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    const bool activated = [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
#pragma clang diagnostic pop

    // Activating the app doesn't necessarily deminiaturize a specific
    // window that was minimized (e.g. by a WindowOp action); do that
    // explicitly for all of this pid's windows so minimize doesn't
    // permanently take the target out of reach.
    AXUIElementRef appElement = AXUIElementCreateApplication((pid_t)pid);
    if (appElement) {
        CFArrayRef windows = nullptr;
        if (AXUIElementCopyAttributeValue(appElement, kAXWindowsAttribute, (CFTypeRef *)&windows) ==
                kAXErrorSuccess &&
            windows) {
            const CFIndex count = CFArrayGetCount(windows);
            for (CFIndex i = 0; i < count; ++i) {
                AXUIElementRef win = (AXUIElementRef)CFArrayGetValueAtIndex(windows, i);
                AXUIElementSetAttributeValue(win, kAXMinimizedAttribute, kCFBooleanFalse);
            }
            CFRelease(windows);
        }
        CFRelease(appElement);
    }

    return activated;
}

bool isProcessRunning(qint64 pid)
{
    // Signal 0 sends nothing but still validates that the pid exists and is
    // reachable; ESRCH means it is gone (crashed/quit), EPERM still means
    // it exists (just owned by another user).
    return kill((pid_t)pid, 0) == 0 || errno == EPERM;
}

ProcessStats queryProcessStats(qint64 pid)
{
    ProcessStats stats;
    struct proc_taskinfo info;
    const int size = proc_pidinfo((pid_t)pid, PROC_PIDTASKINFO, 0, &info, sizeof(info));
    if (size != sizeof(info))
        return stats;
    stats.ok = true;
    stats.residentMemoryMB = double(info.pti_resident_size) / (1024.0 * 1024.0);
    stats.cpuTimeSeconds = double(info.pti_total_user + info.pti_total_system) / 1e9;
    return stats;
}

ResponsivenessCheck checkWindowResponsive(quint32 /*windowId*/, qint64 /*pid*/)
{
    // Activity Monitor / "System Events... is not responding" get this from
    // an undocumented private CoreGraphics/SkyLight symbol
    // (CGSEventIsAppUnresponsive), not any public API. Rather than depend
    // on a private symbol that Apple could change or remove without notice
    // in an OS update, hang detection is intentionally left unimplemented
    // on macOS -- RandomActionEngine treats Unsupported as "skip
    // hang-checking, degrade gracefully" the same way it already does when
    // AT-SPI is unavailable on Linux (see SPEC.md 8/10).
    return ResponsivenessCheck::Unsupported;
}

static CGPoint toCGPoint(const QPoint &pt)
{
    return CGPointMake(pt.x(), pt.y());
}

qint64 windowPidAtPoint(const QPoint &pt)
{
    CFArrayRef cfWindowList =
        CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!cfWindowList)
        return -1;

    NSArray *windowList = (__bridge NSArray *)cfWindowList;
    const pid_t selfPid = getpid();
    const CGPoint cgPt = toCGPoint(pt);
    qint64 result = -1;

    // CGWindowListCopyWindowInfo returns windows in front-to-back order, so
    // the first (normal-layer) match is the topmost window at this point.
    for (NSDictionary *info in windowList) {
        NSNumber *layerNum = info[(id)kCGWindowLayer];
        if (!layerNum || layerNum.intValue != 0)
            continue;

        NSNumber *pidNum = info[(id)kCGWindowOwnerPID];
        if (!pidNum)
            continue;
        const pid_t ownerPid = pidNum.intValue;
        if (ownerPid == selfPid)
            continue;

        NSDictionary *boundsDict = info[(id)kCGWindowBounds];
        if (!boundsDict)
            continue;
        CGRect cgBounds;
        if (!CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)boundsDict, &cgBounds))
            continue;

        if (CGRectContainsPoint(cgBounds, cgPt)) {
            result = ownerPid;
            break;
        }
    }

    CFRelease(cfWindowList);
    return result;
}

qint64 activeProcessPid()
{
    NSRunningApplication *app = [[NSWorkspace sharedWorkspace] frontmostApplication];
    return app ? (qint64)app.processIdentifier : -1;
}

void moveMouse(const QPoint &pt)
{
    CGEventRef event = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, toCGPoint(pt),
                                                kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}

void mouseClick(const QPoint &pt, Qt::MouseButton button)
{
    const bool isRight = (button == Qt::RightButton);
    const CGEventType downType = isRight ? kCGEventRightMouseDown : kCGEventLeftMouseDown;
    const CGEventType upType = isRight ? kCGEventRightMouseUp : kCGEventLeftMouseUp;
    const CGMouseButton cgButton = isRight ? kCGMouseButtonRight : kCGMouseButtonLeft;
    const CGPoint cgPt = toCGPoint(pt);

    CGEventRef move = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, cgPt, cgButton);
    CGEventPost(kCGHIDEventTap, move);
    CFRelease(move);

    CGEventRef down = CGEventCreateMouseEvent(nullptr, downType, cgPt, cgButton);
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);

    usleep(15000);

    CGEventRef up = CGEventCreateMouseEvent(nullptr, upType, cgPt, cgButton);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
}

void mouseDrag(const QPoint &from, const QPoint &to, Qt::MouseButton button, int steps)
{
    const bool isRight = (button == Qt::RightButton);
    const CGEventType downType = isRight ? kCGEventRightMouseDown : kCGEventLeftMouseDown;
    const CGEventType dragType = isRight ? kCGEventRightMouseDragged : kCGEventLeftMouseDragged;
    const CGEventType upType = isRight ? kCGEventRightMouseUp : kCGEventLeftMouseUp;
    const CGMouseButton cgButton = isRight ? kCGMouseButtonRight : kCGMouseButtonLeft;

    CGEventRef down = CGEventCreateMouseEvent(nullptr, downType, toCGPoint(from), cgButton);
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);

    steps = qMax(1, steps);
    for (int i = 1; i <= steps; ++i) {
        const qreal t = qreal(i) / qreal(steps);
        const QPoint p(from.x() + qRound((to.x() - from.x()) * t),
                       from.y() + qRound((to.y() - from.y()) * t));
        CGEventRef drag = CGEventCreateMouseEvent(nullptr, dragType, toCGPoint(p), cgButton);
        CGEventPost(kCGHIDEventTap, drag);
        CFRelease(drag);
        usleep(8000);
    }

    CGEventRef up = CGEventCreateMouseEvent(nullptr, upType, toCGPoint(to), cgButton);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
}

void scroll(const QPoint &pt, int dx, int dy)
{
    moveMouse(pt);
    CGEventRef event = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 2,
                                                       (int32_t)dy, (int32_t)dx);
    CGEventSetLocation(event, toCGPoint(pt));
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}

void keyTap(QChar ch)
{
    UniChar uc = ch.unicode();

    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, 0, true);
    CGEventKeyboardSetUnicodeString(down, 1, &uc);
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);

    usleep(10000);

    CGEventRef up = CGEventCreateKeyboardEvent(nullptr, 0, false);
    CGEventKeyboardSetUnicodeString(up, 1, &uc);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
}

// Maps a Qt::Key to a macOS virtual key code (US layout). Only covers the
// keys relevant to shortcut testing (letters, digits, a handful of named
// keys) -- shortcuts are dispatched by physical key + modifier flags
// (unlike keyTap's unicode-string trick), since that is what macOS'
// shortcut recognition actually keys off of.
static bool qtKeyToVirtualKeyCode(Qt::Key key, CGKeyCode &outCode)
{
    switch (key) {
    case Qt::Key_A: outCode = kVK_ANSI_A; return true;
    case Qt::Key_B: outCode = kVK_ANSI_B; return true;
    case Qt::Key_C: outCode = kVK_ANSI_C; return true;
    case Qt::Key_D: outCode = kVK_ANSI_D; return true;
    case Qt::Key_E: outCode = kVK_ANSI_E; return true;
    case Qt::Key_F: outCode = kVK_ANSI_F; return true;
    case Qt::Key_G: outCode = kVK_ANSI_G; return true;
    case Qt::Key_H: outCode = kVK_ANSI_H; return true;
    case Qt::Key_I: outCode = kVK_ANSI_I; return true;
    case Qt::Key_J: outCode = kVK_ANSI_J; return true;
    case Qt::Key_K: outCode = kVK_ANSI_K; return true;
    case Qt::Key_L: outCode = kVK_ANSI_L; return true;
    case Qt::Key_M: outCode = kVK_ANSI_M; return true;
    case Qt::Key_N: outCode = kVK_ANSI_N; return true;
    case Qt::Key_O: outCode = kVK_ANSI_O; return true;
    case Qt::Key_P: outCode = kVK_ANSI_P; return true;
    case Qt::Key_Q: outCode = kVK_ANSI_Q; return true;
    case Qt::Key_R: outCode = kVK_ANSI_R; return true;
    case Qt::Key_S: outCode = kVK_ANSI_S; return true;
    case Qt::Key_T: outCode = kVK_ANSI_T; return true;
    case Qt::Key_U: outCode = kVK_ANSI_U; return true;
    case Qt::Key_V: outCode = kVK_ANSI_V; return true;
    case Qt::Key_W: outCode = kVK_ANSI_W; return true;
    case Qt::Key_X: outCode = kVK_ANSI_X; return true;
    case Qt::Key_Y: outCode = kVK_ANSI_Y; return true;
    case Qt::Key_Z: outCode = kVK_ANSI_Z; return true;
    case Qt::Key_0: outCode = kVK_ANSI_0; return true;
    case Qt::Key_1: outCode = kVK_ANSI_1; return true;
    case Qt::Key_2: outCode = kVK_ANSI_2; return true;
    case Qt::Key_3: outCode = kVK_ANSI_3; return true;
    case Qt::Key_4: outCode = kVK_ANSI_4; return true;
    case Qt::Key_5: outCode = kVK_ANSI_5; return true;
    case Qt::Key_6: outCode = kVK_ANSI_6; return true;
    case Qt::Key_7: outCode = kVK_ANSI_7; return true;
    case Qt::Key_8: outCode = kVK_ANSI_8; return true;
    case Qt::Key_9: outCode = kVK_ANSI_9; return true;
    case Qt::Key_Return: outCode = kVK_Return; return true;
    case Qt::Key_Tab: outCode = kVK_Tab; return true;
    case Qt::Key_Space: outCode = kVK_Space; return true;
    case Qt::Key_Escape: outCode = kVK_Escape; return true;
    case Qt::Key_Backspace: outCode = kVK_Delete; return true;
    case Qt::Key_Delete: outCode = kVK_ForwardDelete; return true;
    case Qt::Key_Left: outCode = kVK_LeftArrow; return true;
    case Qt::Key_Right: outCode = kVK_RightArrow; return true;
    case Qt::Key_Up: outCode = kVK_UpArrow; return true;
    case Qt::Key_Down: outCode = kVK_DownArrow; return true;
    default: return false;
    }
}

// --- Experimental: context/popup menu introspection ---------------------
//
// macOS does not expose "the currently open context menu" as a simple API.
// The technique used here: assume the frontmost on-screen window (as
// reported by the window server, which lists windows front-to-back) *is*
// the open popup menu -- true immediately after we've just synthesized a
// right-click and nothing else has taken focus -- probe an Accessibility
// element near its top via AXUIElementCopyElementAtPosition, then walk up
// the AX parent chain looking for an AXMenu ancestor. This mirrors the
// approach used by AppleScript's "System Events" / accessibility-based
// automation tools. It has not been exhaustively verified against every
// app/toolkit; if it can't find a menu it simply returns nothing.

static AXUIElementRef copyMenuAncestor(AXUIElementRef start)
{
    AXUIElementRef current = start;
    CFRetain(current);
    for (int hops = 0; hops < 12 && current; ++hops) {
        CFStringRef role = nullptr;
        if (AXUIElementCopyAttributeValue(current, kAXRoleAttribute, (CFTypeRef *)&role) ==
                kAXErrorSuccess &&
            role) {
            const bool isMenu = CFEqual(role, kAXMenuRole);
            CFRelease(role);
            if (isMenu)
                return current;  // caller releases
        }
        AXUIElementRef parent = nullptr;
        const AXError err =
            AXUIElementCopyAttributeValue(current, kAXParentAttribute, (CFTypeRef *)&parent);
        CFRelease(current);
        current = (err == kAXErrorSuccess) ? parent : nullptr;
    }
    return nullptr;
}

static AXUIElementRef findOpenMenuElement(qint64 expectedOwnerPid)
{
    CFArrayRef cfWindows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!cfWindows)
        return nullptr;

    NSArray *windows = (__bridge NSArray *)cfWindows;
    AXUIElementRef menu = nullptr;
    if (windows.count > 0) {
        NSDictionary *top = windows[0];

        // Refuse to treat this as "the menu" unless it's actually owned by
        // the process we expect to have just opened one -- otherwise some
        // other app's topmost window could be mistaken for our target's
        // menu (see PlatformAutomation.h).
        NSNumber *pidNum = top[(id)kCGWindowOwnerPID];
        if (!pidNum || pidNum.longLongValue != expectedOwnerPid) {
            CFRelease(cfWindows);
            return nullptr;
        }

        NSDictionary *boundsDict = top[(id)kCGWindowBounds];
        CGRect bounds;
        if (boundsDict && CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)boundsDict, &bounds)) {
            const CGPoint probe = CGPointMake(bounds.origin.x + bounds.size.width * 0.5,
                                               bounds.origin.y + MIN(20.0, bounds.size.height * 0.5));
            AXUIElementRef systemWide = AXUIElementCreateSystemWide();
            AXUIElementRef element = nullptr;
            const AXError err =
                AXUIElementCopyElementAtPosition(systemWide, (float)probe.x, (float)probe.y, &element);
            CFRelease(systemWide);
            if (err == kAXErrorSuccess && element) {
                menu = copyMenuAncestor(element);
                CFRelease(element);
            }
        }
    }
    CFRelease(cfWindows);
    return menu;
}

QStringList listOpenContextMenuItems(qint64 expectedOwnerPid)
{
    QStringList names;
    AXUIElementRef menu = findOpenMenuElement(expectedOwnerPid);
    if (!menu)
        return names;

    CFArrayRef children = nullptr;
    if (AXUIElementCopyAttributeValue(menu, kAXChildrenAttribute, (CFTypeRef *)&children) ==
            kAXErrorSuccess &&
        children) {
        const CFIndex count = CFArrayGetCount(children);
        for (CFIndex i = 0; i < count; ++i) {
            AXUIElementRef item = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
            CFStringRef title = nullptr;
            if (AXUIElementCopyAttributeValue(item, kAXTitleAttribute, (CFTypeRef *)&title) ==
                    kAXErrorSuccess &&
                title) {
                names.append(QString::fromCFString(title));
                CFRelease(title);
            }
        }
        CFRelease(children);
    }
    CFRelease(menu);
    return names;
}

bool clickContextMenuItem(const QString &itemName, qint64 expectedOwnerPid)
{
    AXUIElementRef menu = findOpenMenuElement(expectedOwnerPid);
    if (!menu)
        return false;

    bool clicked = false;
    CFArrayRef children = nullptr;
    if (AXUIElementCopyAttributeValue(menu, kAXChildrenAttribute, (CFTypeRef *)&children) ==
            kAXErrorSuccess &&
        children) {
        const CFIndex count = CFArrayGetCount(children);
        for (CFIndex i = 0; i < count && !clicked; ++i) {
            AXUIElementRef item = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
            CFStringRef title = nullptr;
            if (AXUIElementCopyAttributeValue(item, kAXTitleAttribute, (CFTypeRef *)&title) ==
                    kAXErrorSuccess &&
                title) {
                if (QString::fromCFString(title) == itemName) {
                    AXUIElementPerformAction(item, kAXPressAction);
                    clicked = true;
                }
                CFRelease(title);
            }
        }
        CFRelease(children);
    }
    CFRelease(menu);
    return clicked;
}

bool clickContextMenuItemAt(int index, qint64 expectedOwnerPid)
{
    AXUIElementRef menu = findOpenMenuElement(expectedOwnerPid);
    if (!menu)
        return false;

    bool clicked = false;
    CFArrayRef children = nullptr;
    if (AXUIElementCopyAttributeValue(menu, kAXChildrenAttribute, (CFTypeRef *)&children) ==
            kAXErrorSuccess &&
        children) {
        if (index >= 0 && index < CFArrayGetCount(children)) {
            AXUIElementRef item = (AXUIElementRef)CFArrayGetValueAtIndex(children, index);
            clicked = (AXUIElementPerformAction(item, kAXPressAction) == kAXErrorSuccess);
        }
        CFRelease(children);
    }
    CFRelease(menu);
    return clicked;
}

void dismissContextMenu()
{
    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, kVK_Escape, true);
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);
    usleep(10000);
    CGEventRef up = CGEventCreateKeyboardEvent(nullptr, kVK_Escape, false);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
}

void keyShortcut(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    CGKeyCode keyCode = 0;
    if (!qtKeyToVirtualKeyCode(key, keyCode))
        return;

    CGEventFlags flags = 0;
    if (modifiers & Qt::ShiftModifier)
        flags |= kCGEventFlagMaskShift;
    if (modifiers & Qt::ControlModifier)
        flags |= kCGEventFlagMaskCommand;  // Qt's portable Ctrl == Cmd on macOS
    if (modifiers & Qt::AltModifier)
        flags |= kCGEventFlagMaskAlternate;
    if (modifiers & Qt::MetaModifier)
        flags |= kCGEventFlagMaskControl;  // Qt's portable Meta == Ctrl on macOS

    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, keyCode, true);
    CGEventSetFlags(down, flags);
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);

    usleep(10000);

    CGEventRef up = CGEventCreateKeyboardEvent(nullptr, keyCode, false);
    CGEventSetFlags(up, flags);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
}

void keyTapNamed(Qt::Key key)
{
    CGKeyCode keyCode = 0;
    if (!qtKeyToVirtualKeyCode(key, keyCode))
        return;

    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, keyCode, true);
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);

    usleep(10000);

    CGEventRef up = CGEventCreateKeyboardEvent(nullptr, keyCode, false);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
}

// --- Window-level operations ---------------------------------------------
//
// Matches a pid+windowId (from our own WindowInfo/CGWindowList bookkeeping)
// to its Accessibility (AXUIElement) window object by comparing current
// position/size, since AXUIElement has no direct "give me the element for
// this CGWindowID" lookup in the public API. Returns a retained reference
// the caller must CFRelease, or nullptr if no match was found (e.g. the
// window closed).
static AXUIElementRef copyAXWindow(qint64 pid, quint32 windowId)
{
    QRect bounds;
    if (!queryWindowBounds(windowId, pid, bounds))
        return nullptr;

    AXUIElementRef app = AXUIElementCreateApplication((pid_t)pid);
    if (!app)
        return nullptr;

    CFArrayRef windows = nullptr;
    if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, (CFTypeRef *)&windows) !=
            kAXErrorSuccess ||
        !windows) {
        CFRelease(app);
        return nullptr;
    }

    AXUIElementRef match = nullptr;
    const CFIndex count = CFArrayGetCount(windows);
    for (CFIndex i = 0; i < count; ++i) {
        AXUIElementRef win = (AXUIElementRef)CFArrayGetValueAtIndex(windows, i);
        AXValueRef posValue = nullptr, sizeValue = nullptr;
        CGPoint pos = CGPointZero;
        CGSize size = CGSizeZero;
        if (AXUIElementCopyAttributeValue(win, kAXPositionAttribute, (CFTypeRef *)&posValue) ==
                kAXErrorSuccess &&
            posValue) {
            AXValueGetValue(posValue, (AXValueType)kAXValueCGPointType, &pos);
            CFRelease(posValue);
        }
        if (AXUIElementCopyAttributeValue(win, kAXSizeAttribute, (CFTypeRef *)&sizeValue) ==
                kAXErrorSuccess &&
            sizeValue) {
            AXValueGetValue(sizeValue, (AXValueType)kAXValueCGSizeType, &size);
            CFRelease(sizeValue);
        }

        const QRect winBounds(qRound(pos.x), qRound(pos.y), qRound(size.width), qRound(size.height));
        // Allow a couple of pixels of slack: CGWindowList and the
        // Accessibility API occasionally round window geometry slightly
        // differently for the same window.
        const bool approxEqual = qAbs(winBounds.x() - bounds.x()) <= 2 &&
                                  qAbs(winBounds.y() - bounds.y()) <= 2 &&
                                  qAbs(winBounds.width() - bounds.width()) <= 2 &&
                                  qAbs(winBounds.height() - bounds.height()) <= 2;
        if (approxEqual) {
            match = win;
            CFRetain(match);
            break;
        }
    }

    CFRelease(windows);
    CFRelease(app);
    return match;
}

bool moveWindow(qint64 pid, quint32 windowId, const QPoint &newTopLeft)
{
    AXUIElementRef win = copyAXWindow(pid, windowId);
    if (!win)
        return false;
    CGPoint pos = CGPointMake(newTopLeft.x(), newTopLeft.y());
    AXValueRef value = AXValueCreate((AXValueType)kAXValueCGPointType, &pos);
    const bool ok = value && AXUIElementSetAttributeValue(win, kAXPositionAttribute, value) ==
                                  kAXErrorSuccess;
    if (value)
        CFRelease(value);
    CFRelease(win);
    return ok;
}

bool resizeWindow(qint64 pid, quint32 windowId, const QSize &newSize)
{
    AXUIElementRef win = copyAXWindow(pid, windowId);
    if (!win)
        return false;
    CGSize size = CGSizeMake(newSize.width(), newSize.height());
    AXValueRef value = AXValueCreate((AXValueType)kAXValueCGSizeType, &size);
    const bool ok = value && AXUIElementSetAttributeValue(win, kAXSizeAttribute, value) ==
                                 kAXErrorSuccess;
    if (value)
        CFRelease(value);
    CFRelease(win);
    return ok;
}

bool minimizeWindow(qint64 pid, quint32 windowId)
{
    AXUIElementRef win = copyAXWindow(pid, windowId);
    if (!win)
        return false;
    const bool ok = AXUIElementSetAttributeValue(win, kAXMinimizedAttribute, kCFBooleanTrue) ==
                     kAXErrorSuccess;
    CFRelease(win);
    return ok;
}

bool maximizeWindow(qint64 pid, quint32 windowId)
{
    QRect bounds;
    if (!queryWindowBounds(windowId, pid, bounds))
        return false;

    // NSScreen frames are in AppKit's coordinate system (origin at the
    // main screen's bottom-left, Y pointing up); CGWindowList/AXPosition
    // (and therefore `bounds`) use Quartz's global display coordinates
    // (origin at the main screen's top-left, Y pointing down). Convert
    // each candidate screen's frame to the latter before comparing, using
    // screens[0]'s full frame height as the flip reference (screens[0] is
    // always the screen containing the menu bar, i.e. Quartz's origin).
    NSArray<NSScreen *> *screens = [NSScreen screens];
    if (screens.count == 0)
        return false;
    const CGFloat mainScreenHeight = screens.firstObject.frame.size.height;

    auto toQuartzRect = [mainScreenHeight](NSRect appKitRect) {
        return QRect(qRound(appKitRect.origin.x),
                     qRound(mainScreenHeight - appKitRect.origin.y - appKitRect.size.height),
                     qRound(appKitRect.size.width), qRound(appKitRect.size.height));
    };

    QRect bestVisible;
    int bestArea = -1;
    for (NSScreen *screen in screens) {
        const QRect screenQuartzFrame = toQuartzRect(screen.frame);
        const QRect intersection = screenQuartzFrame.intersected(bounds);
        const int area = intersection.width() * intersection.height();
        if (area > bestArea) {
            bestArea = area;
            bestVisible = toQuartzRect(screen.visibleFrame);
        }
    }
    if (bestArea < 0)
        return false;

    const bool moved = moveWindow(pid, windowId, bestVisible.topLeft());
    const bool resized = resizeWindow(pid, windowId, bestVisible.size());
    return moved && resized;
}

}  // namespace PlatformAutomation
