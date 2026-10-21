#include "platform/PlatformAutomation.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>
#import <CoreGraphics/CoreGraphics.h>

#include <QGuiApplication>
#include <QRegularExpression>
#include <QScreen>

#include <libproc.h>
#include <sys/proc.h>
#include <sys/proc_info.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>

namespace PlatformAutomation
{

namespace
{
// SPEC.md 追加実装依頼「名前付きオブジェクト」: master override, checked by
// every Accessibility-API-touching function below (the pre-existing
// context-menu/dialog-button/accessibleNameAtPoint ones too, not just the
// new findAccessibleObject()/accessibleObjectAtPoint()) before doing
// anything else -- see setAccessibilityFeaturesDisabled()'s own doc comment
// in PlatformAutomation.h. Not atomic: only ever set from the UI thread,
// before a run starts, never concurrently with a run already using it.
bool g_accessibilityDisabled = false;
}  // namespace

void setAccessibilityFeaturesDisabled(bool disabled)
{
    g_accessibilityDisabled = disabled;
}

bool accessibilityFeaturesDisabled()
{
    return g_accessibilityDisabled;
}

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

bool hasOpenableAccessibilitySettings()
{
    return true;
}

bool isScreenRecordingTrusted()
{
    // Preflight-only: unlike CGRequestScreenCaptureAccess(), this never
    // triggers the OS permission prompt, so it's safe to call from a
    // passive/periodic UI refresh (see the header comment).
    return CGPreflightScreenCaptureAccess();
}

void openScreenRecordingSettings()
{
    NSString *urlString =
        @"x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture";
    NSURL *url = [NSURL URLWithString:urlString];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

bool hasOpenableScreenRecordingSettings()
{
    return true;
}

bool supportsWindowTransparency()
{
    // Quartz (the macOS WindowServer) always composites every window, with or
    // without any particular app's involvement -- there is no "bare window
    // manager, no compositor" case on macOS the way there is on Linux (see the
    // header comment).
    return true;
}

void setWindowClickThrough(quint32 /*windowId*/)
{
    // No-op: see this function's declaration comment in PlatformAutomation.h.
    // The X11-specific click-through failure this exists to work around
    // (Qt::WA_TransparentForMouseEvents not reliably making a full-screen,
    // always-on-top window click-through under a bare/non-compositing window
    // manager) has no macOS equivalent -- AppKit always composites, and Qt's
    // own attribute (NSWindow.ignoresMouseEvents) has been sufficient there.
}

// 不具合報告H-20(低、Linuxで確認・この関数はmacOSでは未検証): collectWindows()
// below only ever excluded a window whose owning pid is *this exact
// process's own* pid (getpid()) -- not any OTHER process also running the
// same EnduranceTestGUI binary, which on Linux (see the identical fix and
// comment in Automation_linux.cpp's own collectWindows()) let a second ETG
// instance's own main window appear in, and even become the default
// selection in, ①'s own target-selection list. Comparing each window's
// owning process's executableURL (via NSRunningApplication, which the OS
// resolves from the actual running binary regardless of what window
// title/app name that process happens to show) against this process's own
// identifies "another instance of this exact program" the same way
// /proc/<pid>/exe does on Linux.
static bool isAnotherInstanceOfThisApp(pid_t pid)
{
    if (pid <= 0)
        return false;
    NSURL *selfExe = [NSRunningApplication currentApplication].executableURL;
    if (!selfExe)
        return false;
    NSRunningApplication *other = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    NSURL *otherExe = other.executableURL;
    return otherExe && [otherExe isEqual:selfExe];
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
        // 不具合報告H-20(低): see isAnotherInstanceOfThisApp()'s own comment.
        if (isAnotherInstanceOfThisApp(ownerPid))
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
    // See Automation_linux.cpp's isProcessRunning() for why pid <= 0 must be
    // rejected before ever calling kill(): those values are POSIX's
    // broadcast-to-a-group forms, not a specific-process check, so
    // kill(-1, 0) "succeeds" unconditionally and would otherwise make a
    // window with no owning pid (e.g. one windowPid()-equivalent lookup
    // defaults to -1 for) look permanently alive (SPEC.md 10 不具合報告 D9).
    if (pid <= 0)
        return false;

    // Signal 0 sends nothing but still validates that the pid exists and is
    // reachable; ESRCH means it is gone (crashed/quit), EPERM still means
    // it exists (just owned by another user).
    if (kill((pid_t)pid, 0) != 0 && errno != EPERM)
        return false;

    // kill(pid, 0) alone can't distinguish a zombie (already dead, just not
    // yet reaped by its own parent process) from a genuinely running
    // process -- both return 0, since a zombie's pid is still a valid,
    // allocated table entry. A crashed target can sit as a zombie for a
    // little while (confirmed directly on Linux, where the same underlying
    // issue exists -- see Automation_linux.cpp's isProcessRunning), which
    // would otherwise delay crash detection past this check and into a
    // less specific "window/region not found" stop instead (SPEC.md 6.7).
    // Cross-check the BSD process status via libproc; SZOMB means zombie.
    // Not independently verified on real macOS hardware (see SPEC.md 8) --
    // mirrors the already-used PROC_PIDTASKINFO pattern in
    // queryProcessStats() below.
    struct proc_bsdinfo info;
    const int size = proc_pidinfo((pid_t)pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info));
    if (size == sizeof(info) && info.pbi_status == SZOMB)
        return false;

    return true;
}

bool isBeingDebugged(qint64 pid)
{
    // PROC_FLAG_TRACED mirrors the BSD process flag set while a debugger
    // (gdb/lldb, via ptrace/task_for_pid) is attached -- same proc_bsdinfo
    // struct already used for the zombie check above, just a different
    // field (pbi_flags) from it. Not independently verified on real macOS
    // hardware (see SPEC.md 8) -- mirrors Automation_linux.cpp's
    // isBeingDebugged() in spirit (both read a tracer-attached flag out of
    // the OS's own per-process state), just via libproc instead of /proc.
    struct proc_bsdinfo info;
    const int size = proc_pidinfo((pid_t)pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info));
    if (size != sizeof(info))
        return false;
    return (info.pbi_flags & PROC_FLAG_TRACED) != 0;
}

bool isProcessSuspended(qint64 pid)
{
    if (pid <= 0)
        return false;
    // SSTOP mirrors the BSD process status set while a process is stopped
    // by a job-control signal (SIGSTOP/SIGTSTP/SIGTTIN/SIGTTOU) -- same
    // proc_bsdinfo struct and pattern as the zombie check in
    // isProcessRunning() above, just checking for a different status value.
    // Not independently verified on real macOS hardware (see SPEC.md 8) --
    // mirrors Automation_linux.cpp's isProcessSuspended() in spirit (SPEC.md
    // 10 不具合報告 D10).
    struct proc_bsdinfo info;
    const int size = proc_pidinfo((pid_t)pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info));
    if (size != sizeof(info))
        return false;
    return info.pbi_status == SSTOP;
}

void terminateProcess(qint64 pid)
{
    // Best-effort/asynchronous by design (see the header comment) -- a
    // failure here (e.g. the pid is already gone, or we lack permission for
    // some pid we don't actually own) isn't worth surfacing to the caller,
    // which is going to poll isProcessRunning() afterward anyway. Not
    // independently verified on real macOS hardware (see SPEC.md 8) --
    // mirrors Automation_linux.cpp's terminateProcess(), including the
    // SIGCONT-before-SIGTERM fix for 不具合報告1.2(重大): a process
    // currently stopped (SIGSTOP, or paused under a debugger) just leaves
    // SIGTERM pending indefinitely without ever acting on it.
    kill((pid_t)pid, SIGCONT);
    kill((pid_t)pid, SIGTERM);
}

void forceKillProcess(qint64 pid)
{
    kill((pid_t)pid, SIGKILL);
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

SystemCpuStats querySystemCpuStats()
{
    // Not yet implemented -- see this function's own header comment in
    // PlatformAutomation.h (a correct whole-system reading here needs
    // host_statistics() via Mach APIs). LoadMonitor treats ok == false as
    // "this series isn't available on this platform" and simply omits it,
    // the same graceful-degradation pattern checkWindowResponsive()'s
    // Unsupported result already uses above.
    return SystemCpuStats();
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

qint64 windowPidAtPoint(const QPoint &pt, bool excludeSelf)
{
    // `excludeSelf` (不具合報告E-01, Linux-only so far): this function
    // already unconditionally skips every one of this process's own
    // windows below (see the `ownerPid == selfPid` check and its comment),
    // regardless of `excludeSelf`'s value, for the reasons explained there.
    // The parameter still needs to exist here so this function's signature
    // matches its declaration in PlatformAutomation.h.
    (void)excludeSelf;
    // 不具合報告1.1(重大、Linux上で確認されたが同じ経路のためmacOSにも適用):
    // fail closed for any point outside every connected screen's real
    // bounds -- see the identical comment in Automation_linux.cpp's own
    // windowPidAtPoint() for why this matters alongside RandomActionEngine's
    // own candidate-point clamping.
    bool onAnyScreen = false;
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->geometry().contains(pt)) {
            onAnyScreen = true;
            break;
        }
    }
    if (!onAnyScreen)
        return -1;

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
        // 不具合報告D2(重大、Linuxで確認・この行はmacOSでは未検証):
        // on Linux, unconditionally skipping every one of ETG's own windows
        // here let a click land unnoticed on ETG's own always-on-top
        // stop/pause panel instead of a safety stop (see the fix and
        // comment on this same check in Automation_linux.cpp's
        // windowPidAtPoint()). The `layerNum.intValue != 0` filter above
        // already excludes this app's floating/always-on-top panels here
        // (NSFloatingWindowLevel and above report a non-zero kCGWindowLayer),
        // so the failure mode as reproduced on Linux is not expected to
        // reach this line on macOS. This has not been verified on real
        // macOS hardware (no macOS toolchain in this environment) -- if a
        // self-owned, normal-layer (layer 0) ETG window is ever found to
        // overlap the target during a run, this skip would need the same
        // narrowing the Linux version received.
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

// SPEC.md 追加実装依頼「ビューワー耐久テスト強化」: Qt::MiddleButton ==
// kCGMouseButtonCenter/kCGEventOtherMouse* -- the "other" family covers
// every button beyond left/right, but this project only ever means the
// middle button by it. Shared by mouseClick()/mouseDrag() below and the
// new mouseButtonDown()/mouseButtonUp() (previously each of mouseClick()/
// mouseDrag() duplicated this same isRight ternary independently; a third
// button made a shared mapping worth factoring out).
struct MouseButtonCodes
{
    CGEventType downType;
    CGEventType dragType;
    CGEventType upType;
    CGMouseButton cgButton;
};

MouseButtonCodes mouseButtonCodesFor(Qt::MouseButton button)
{
    if (button == Qt::RightButton)
        return {kCGEventRightMouseDown, kCGEventRightMouseDragged, kCGEventRightMouseUp,
                kCGMouseButtonRight};
    if (button == Qt::MiddleButton)
        return {kCGEventOtherMouseDown, kCGEventOtherMouseDragged, kCGEventOtherMouseUp,
                kCGMouseButtonCenter};
    return {kCGEventLeftMouseDown, kCGEventLeftMouseDragged, kCGEventLeftMouseUp, kCGMouseButtonLeft};
}

void mouseClick(const QPoint &pt, Qt::MouseButton button)
{
    const MouseButtonCodes codes = mouseButtonCodesFor(button);
    const CGPoint cgPt = toCGPoint(pt);

    CGEventRef move = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, cgPt, codes.cgButton);
    CGEventPost(kCGHIDEventTap, move);
    CFRelease(move);

    CGEventRef down = CGEventCreateMouseEvent(nullptr, codes.downType, cgPt, codes.cgButton);
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);

    usleep(15000);

    CGEventRef up = CGEventCreateMouseEvent(nullptr, codes.upType, cgPt, codes.cgButton);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
}

void mouseDrag(const QPoint &from, const QPoint &to, Qt::MouseButton button, int steps)
{
    const MouseButtonCodes codes = mouseButtonCodesFor(button);

    CGEventRef down = CGEventCreateMouseEvent(nullptr, codes.downType, toCGPoint(from), codes.cgButton);
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);

    steps = qMax(1, steps);
    for (int i = 1; i <= steps; ++i) {
        const qreal t = qreal(i) / qreal(steps);
        const QPoint p(from.x() + qRound((to.x() - from.x()) * t),
                       from.y() + qRound((to.y() - from.y()) * t));
        CGEventRef drag = CGEventCreateMouseEvent(nullptr, codes.dragType, toCGPoint(p), codes.cgButton);
        CGEventPost(kCGHIDEventTap, drag);
        CFRelease(drag);
        usleep(8000);
    }

    CGEventRef up = CGEventCreateMouseEvent(nullptr, codes.upType, toCGPoint(to), codes.cgButton);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
}

void mouseButtonDown(const QPoint &pt, Qt::MouseButton button)
{
    const MouseButtonCodes codes = mouseButtonCodesFor(button);
    const CGPoint cgPt = toCGPoint(pt);

    CGEventRef move = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, cgPt, codes.cgButton);
    CGEventPost(kCGHIDEventTap, move);
    CFRelease(move);

    CGEventRef down = CGEventCreateMouseEvent(nullptr, codes.downType, cgPt, codes.cgButton);
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);
}

void mouseButtonUp(Qt::MouseButton button)
{
    // CGEventCreateMouseEvent requires *some* location -- the current
    // pointer location (wherever the most recent moveMouseWhileHeld() call
    // left it) is exactly what a real button-up event would report.
    const MouseButtonCodes codes = mouseButtonCodesFor(button);
    const CGEventRef probe = CGEventCreate(nullptr);
    const CGPoint currentPt = CGEventGetLocation(probe);
    CFRelease(probe);

    CGEventRef up = CGEventCreateMouseEvent(nullptr, codes.upType, currentPt, codes.cgButton);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
}

void moveMouseWhileHeld(const QPoint &pt, Qt::MouseButton button)
{
    // Unlike moveMouse() (posts kCGEventMouseMoved), this must post the
    // button-specific *Dragged event type -- see this function's own
    // declaration comment in PlatformAutomation.h.
    const MouseButtonCodes codes = mouseButtonCodesFor(button);
    CGEventRef drag = CGEventCreateMouseEvent(nullptr, codes.dragType, toCGPoint(pt), codes.cgButton);
    CGEventPost(kCGHIDEventTap, drag);
    CFRelease(drag);
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
    if (accessibilityFeaturesDisabled())
        return nullptr;
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

// SPEC.md 6.2追加実装及び修正依頼: same "assume the frontmost window owned
// by expectedOwnerPid is the one we want" technique as findOpenMenuElement()
// above, but walking up to the nearest AXWindow ancestor (covers both plain
// windows and dialogs -- macOS gives a dialog sheet/panel the same AXWindow
// role, distinguished only by a subrole this doesn't need to check) instead
// of AXMenu.
static AXUIElementRef copyAncestorWithRole(AXUIElementRef start, CFStringRef wantedRole)
{
    AXUIElementRef current = start;
    CFRetain(current);
    for (int hops = 0; hops < 20 && current; ++hops) {
        CFStringRef role = nullptr;
        if (AXUIElementCopyAttributeValue(current, kAXRoleAttribute, (CFTypeRef *)&role) ==
                kAXErrorSuccess &&
            role) {
            const bool matches = CFEqual(role, wantedRole);
            CFRelease(role);
            if (matches)
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

static AXUIElementRef findFrontmostDialogElement(qint64 expectedOwnerPid)
{
    if (accessibilityFeaturesDisabled())
        return nullptr;
    CFArrayRef cfWindows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!cfWindows)
        return nullptr;

    NSArray *windows = (__bridge NSArray *)cfWindows;
    AXUIElementRef dialog = nullptr;
    if (windows.count > 0) {
        NSDictionary *top = windows[0];
        NSNumber *pidNum = top[(id)kCGWindowOwnerPID];
        if (pidNum && pidNum.longLongValue == expectedOwnerPid) {
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
                    dialog = copyAncestorWithRole(element, kAXWindowRole);
                    CFRelease(element);
                }
            }
        }
    }
    CFRelease(cfWindows);
    return dialog;
}

// Bounded recursive search of `root`'s subtree for a descendant whose
// kAXTitleAttribute matches `name` -- used by clickButtonByName() below.
// Same traversal-budget rationale as the Linux AT-SPI counterpart.
static AXUIElementRef findNamedElementRecursive(AXUIElementRef node, const QString &name, int depth,
                                                 int &budget)
{
    if (!node || depth > 15 || budget <= 0)
        return nullptr;
    --budget;

    CFStringRef title = nullptr;
    if (AXUIElementCopyAttributeValue(node, kAXTitleAttribute, (CFTypeRef *)&title) == kAXErrorSuccess &&
        title) {
        const bool matches = QString::fromCFString(title) == name;
        CFRelease(title);
        if (matches) {
            CFRetain(node);
            return node;
        }
    }

    CFArrayRef children = nullptr;
    if (AXUIElementCopyAttributeValue(node, kAXChildrenAttribute, (CFTypeRef *)&children) ==
            kAXErrorSuccess &&
        children) {
        const CFIndex count = CFArrayGetCount(children);
        for (CFIndex i = 0; i < count && budget > 0; ++i) {
            AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
            AXUIElementRef found = findNamedElementRecursive(child, name, depth + 1, budget);
            if (found) {
                CFRelease(children);
                return found;
            }
        }
        CFRelease(children);
    }
    return nullptr;
}

bool clickButtonByName(const QString &buttonName, qint64 expectedOwnerPid)
{
    AXUIElementRef dialog = findFrontmostDialogElement(expectedOwnerPid);
    if (!dialog)
        return false;

    int budget = 4000;
    AXUIElementRef found = findNamedElementRecursive(dialog, buttonName, 0, budget);
    CFRelease(dialog);
    if (!found)
        return false;

    const bool clicked = (AXUIElementPerformAction(found, kAXPressAction) == kAXErrorSuccess);
    CFRelease(found);
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

QString accessibleNameAtPoint(const QPoint &pt, qint64 expectedOwnerPid)
{
    if (accessibilityFeaturesDisabled())
        return QString();
    AXUIElementRef systemWide = AXUIElementCreateSystemWide();
    AXUIElementRef element = nullptr;
    const AXError err =
        AXUIElementCopyElementAtPosition(systemWide, (float)pt.x(), (float)pt.y(), &element);
    CFRelease(systemWide);
    if (err != kAXErrorSuccess || !element)
        return QString();

    // SPEC.md 不具合報告2.5相当(Linux版の修正に合わせたクロスプラットフォーム
    // 対応): expectedOwnerPidが分かっている場合、ヒットした要素が別アプリに
    // 属していないか確認する -- hitTestDesktop()のPIDスコープと同じ考え方。
    if (expectedOwnerPid > 0) {
        pid_t ownerPid = 0;
        if (AXUIElementGetPid(element, &ownerPid) != kAXErrorSuccess ||
            qint64(ownerPid) != expectedOwnerPid) {
            CFRelease(element);
            return QString();
        }
    }

    QString result;
    CFStringRef title = nullptr;
    if (AXUIElementCopyAttributeValue(element, kAXTitleAttribute, (CFTypeRef *)&title) ==
            kAXErrorSuccess &&
        title) {
        result = QString::fromCFString(title);
        CFRelease(title);
    }
    if (result.isEmpty()) {
        CFStringRef description = nullptr;
        if (AXUIElementCopyAttributeValue(element, kAXDescriptionAttribute, (CFTypeRef *)&description) ==
                kAXErrorSuccess &&
            description) {
            result = QString::fromCFString(description);
            CFRelease(description);
        }
    }
    if (result.isEmpty()) {
        CFStringRef role = nullptr;
        if (AXUIElementCopyAttributeValue(element, kAXRoleAttribute, (CFTypeRef *)&role) ==
                kAXErrorSuccess &&
            role) {
            result = QStringLiteral("(%1)").arg(QString::fromCFString(role));
            CFRelease(role);
        }
    }
    CFRelease(element);
    return result;
}

// SPEC.md 追加実装依頼「名前付きオブジェクト」: this node's current
// on-screen bounding box via its kAXPosition/kAXSize attributes, or false if
// either is missing/unreadable -- callers treat that as "this reference is
// now stale" (see AccessibleObjectHandle::currentBounds()). Same
// AXValueGetValue(..., kAXValueCGPointType/kAXValueCGSizeType, ...) pattern
// copyAXWindow() below already uses for a window's own position/size.
static bool getElementBounds(AXUIElementRef node, QRect &out)
{
    AXValueRef posValue = nullptr;
    AXValueRef sizeValue = nullptr;
    CGPoint pos = CGPointZero;
    CGSize size = CGSizeZero;
    bool havePos = false;
    bool haveSize = false;
    if (AXUIElementCopyAttributeValue(node, kAXPositionAttribute, (CFTypeRef *)&posValue) ==
            kAXErrorSuccess &&
        posValue) {
        havePos = AXValueGetValue(posValue, (AXValueType)kAXValueCGPointType, &pos);
        CFRelease(posValue);
    }
    if (AXUIElementCopyAttributeValue(node, kAXSizeAttribute, (CFTypeRef *)&sizeValue) ==
            kAXErrorSuccess &&
        sizeValue) {
        haveSize = AXValueGetValue(sizeValue, (AXValueType)kAXValueCGSizeType, &size);
        CFRelease(sizeValue);
    }
    if (!havePos || !haveSize)
        return false;
    out = QRect(qRound(pos.x), qRound(pos.y), qRound(size.width), qRound(size.height));
    return true;
}

// 不具合報告D28(低、Linux版Automation_linux.cppのmatchesObjectName()と同じ
// 目的): see that function's own comment -- mirrored here for macOS's
// AXUIElement-based backend.
static bool matchesObjectName(const QString &candidate, const QString &pattern, NameMatchMode mode)
{
    // 不具合報告E-11(重大、Linux版と同じ目的): see
    // Automation_linux.cppの同名関数の自分自身のコメント -- 空パターンの
    // 無差別一致を防ぐ。
    if (mode != NameMatchMode::Exact && pattern.isEmpty())
        return false;
    switch (mode) {
    case NameMatchMode::StartsWith:
        return candidate.startsWith(pattern);
    case NameMatchMode::Regex: {
        const QRegularExpression re(pattern);
        return re.isValid() && re.match(candidate).hasMatch();
    }
    case NameMatchMode::Exact:
    default:
        return candidate == pattern;
    }
}

// Bounded recursive search of `node`'s subtree for a descendant whose
// kAXRoleAttribute matches exactly (role as the backend reports it, e.g.
// "AXButton", "AXMenuItem", "AXCheckBox" -- see ObjectTarget::role's comment)
// and whose kAXTitleAttribute matches per nameMatchMode (see
// matchesObjectName() above), visiting matches in document order and
// returning the `occurrenceIndex`'th one (0-based, via `matchesSeen`) --
// used by findAccessibleObject() below. Same bounded-traversal rationale as
// findNamedElementRecursive() above, just also role-checked and not
// restricted to actionable nodes, since a check box or menu-bar item some
// ObjectTarget only wants the bounding box of (useDefaultAction == false)
// need not be "pressable" to begin with.
static AXUIElementRef findRoleNamedElementRecursive(AXUIElementRef node, const QString &role,
                                                     const QString &name, NameMatchMode nameMatchMode,
                                                     int occurrenceIndex, int &matchesSeen, int depth,
                                                     int &budget)
{
    if (!node || depth > 20 || budget <= 0)
        return nullptr;
    --budget;

    bool roleMatches = false;
    CFStringRef nodeRole = nullptr;
    if (AXUIElementCopyAttributeValue(node, kAXRoleAttribute, (CFTypeRef *)&nodeRole) ==
            kAXErrorSuccess &&
        nodeRole) {
        roleMatches = (QString::fromCFString(nodeRole) == role);
        CFRelease(nodeRole);
    }

    bool nameMatches = false;
    if (roleMatches) {
        CFStringRef title = nullptr;
        if (AXUIElementCopyAttributeValue(node, kAXTitleAttribute, (CFTypeRef *)&title) ==
                kAXErrorSuccess &&
            title) {
            nameMatches = matchesObjectName(QString::fromCFString(title), name, nameMatchMode);
            CFRelease(title);
        }
    }

    if (roleMatches && nameMatches) {
        if (matchesSeen == occurrenceIndex) {
            CFRetain(node);
            return node;
        }
        ++matchesSeen;
    }

    CFArrayRef children = nullptr;
    if (AXUIElementCopyAttributeValue(node, kAXChildrenAttribute, (CFTypeRef *)&children) ==
            kAXErrorSuccess &&
        children) {
        const CFIndex count = CFArrayGetCount(children);
        for (CFIndex i = 0; i < count && budget > 0; ++i) {
            AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
            AXUIElementRef found = findRoleNamedElementRecursive(
                child, role, name, nameMatchMode, occurrenceIndex, matchesSeen, depth + 1, budget);
            if (found) {
                CFRelease(children);
                return found;
            }
        }
        CFRelease(children);
    }
    return nullptr;
}

// SPEC.md 追加実装依頼「ファジングテスト: 領域内の全入力項目」: bounded
// recursive search of `node`'s subtree for every descendant whose
// kAXValueAttribute is settable -- the toolkit-agnostic "this accepts typed
// text" signal on macOS, true for a text field, secure text field, text
// area, or editable combo box regardless of its exact kAXRoleAttribute --
// and whose on-screen bounds intersect `regionBounds`. Appends a match for
// each one found, in document order, stopping early once `out` reaches
// maxResults. Same bounded-traversal rationale (depth/budget) as
// findRoleNamedElementRecursive() above, just collecting every match
// instead of returning the first/Nth one.
static void collectEditableElementsRecursive(AXUIElementRef node, const QRect &regionBounds, int depth,
                                              int &budget, QList<EditableFieldInfo> &out, int maxResults)
{
    if (!node || depth > 20 || budget <= 0 || out.size() >= maxResults)
        return;
    --budget;

    Boolean settable = false;
    if (AXUIElementIsAttributeSettable(node, kAXValueAttribute, &settable) == kAXErrorSuccess && settable) {
        QRect bounds;
        if (getElementBounds(node, bounds) && regionBounds.intersects(bounds)) {
            EditableFieldInfo info;
            info.bounds = bounds;
            CFStringRef nodeRole = nullptr;
            if (AXUIElementCopyAttributeValue(node, kAXRoleAttribute, (CFTypeRef *)&nodeRole) ==
                    kAXErrorSuccess &&
                nodeRole) {
                info.role = QString::fromCFString(nodeRole);
                CFRelease(nodeRole);
            }
            out.append(info);
        }
    }

    CFArrayRef children = nullptr;
    if (AXUIElementCopyAttributeValue(node, kAXChildrenAttribute, (CFTypeRef *)&children) ==
            kAXErrorSuccess &&
        children) {
        const CFIndex count = CFArrayGetCount(children);
        for (CFIndex i = 0; i < count && budget > 0 && out.size() < maxResults; ++i) {
            AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
            collectEditableElementsRecursive(child, regionBounds, depth + 1, budget, out, maxResults);
        }
        CFRelease(children);
    }
}

struct AccessibleObjectHandle::Impl
{
    AXUIElementRef element = nullptr;  // owned reference; released in ~Impl()
    bool valid = false;

    ~Impl()
    {
        if (element)
            CFRelease(element);
    }
};

AccessibleObjectHandle::AccessibleObjectHandle() = default;
AccessibleObjectHandle::~AccessibleObjectHandle() = default;
AccessibleObjectHandle::AccessibleObjectHandle(AccessibleObjectHandle &&) noexcept = default;
AccessibleObjectHandle &AccessibleObjectHandle::operator=(AccessibleObjectHandle &&) noexcept = default;

bool AccessibleObjectHandle::isValid() const
{
    return m_impl && m_impl->valid;
}

bool AccessibleObjectHandle::currentBounds(QRect &out) const
{
    if (!isValid())
        return false;
    if (!getElementBounds(m_impl->element, out)) {
        m_impl->valid = false;
        return false;
    }
    return true;
}

bool AccessibleObjectHandle::performDefaultAction() const
{
    if (!isValid())
        return false;
    const bool ok = (AXUIElementPerformAction(m_impl->element, kAXPressAction) == kAXErrorSuccess);
    if (!ok)
        m_impl->valid = false;
    return ok;
}

AccessibleObjectInfo accessibleObjectAtPoint(const QPoint &pt, qint64 expectedOwnerPid)
{
    // Scoped to expectedOwnerPid when given (AXUIElementCopyElementAtPosition
    // accepts either the system-wide accessibility object -- unscoped, finds
    // whatever is frontmost at that screen point in any app -- or a specific
    // application's own accessibility object, which limits the search to
    // that application; see Apple's AXUIElementCopyElementAtPosition docs).
    // Scoping here matters for more than precision: ObjectPickerOverlay is
    // itself a full-screen always-on-top AT-SPI/AX-visible Qt window
    // belonging to this same process, so an unscoped system-wide hit-test
    // could match the overlay's own widget tree at the clicked point instead
    // of the target application's (same reasoning as hitTestDesktop()'s
    // comment in Automation_linux.cpp, which has an equivalent fix).
    AccessibleObjectInfo info;
    if (accessibilityFeaturesDisabled())
        return info;

    AXUIElementRef searchRoot = expectedOwnerPid > 0
                                     ? AXUIElementCreateApplication((pid_t)expectedOwnerPid)
                                     : AXUIElementCreateSystemWide();
    AXUIElementRef element = nullptr;
    const AXError err =
        AXUIElementCopyElementAtPosition(searchRoot, (float)pt.x(), (float)pt.y(), &element);
    CFRelease(searchRoot);
    if (err != kAXErrorSuccess || !element)
        return info;

    QRect bounds;
    if (getElementBounds(element, bounds)) {
        info.found = true;
        info.bounds = bounds;
        CFStringRef title = nullptr;
        if (AXUIElementCopyAttributeValue(element, kAXTitleAttribute, (CFTypeRef *)&title) ==
                kAXErrorSuccess &&
            title) {
            info.name = QString::fromCFString(title);
            CFRelease(title);
        }
        CFStringRef role = nullptr;
        if (AXUIElementCopyAttributeValue(element, kAXRoleAttribute, (CFTypeRef *)&role) ==
                kAXErrorSuccess &&
            role) {
            info.role = QString::fromCFString(role);
            CFRelease(role);
        }
    }
    CFRelease(element);
    return info;
}

AccessibleObjectHandle findAccessibleObject(qint64 expectedOwnerPid, const QString &role,
                                             const QString &name, int occurrenceIndex,
                                             NameMatchMode nameMatchMode)
{
    AccessibleObjectHandle handle;
    if (accessibilityFeaturesDisabled())
        return handle;

    AXUIElementRef app = AXUIElementCreateApplication((pid_t)expectedOwnerPid);
    if (!app)
        return handle;

    int budget = 4000;
    int matchesSeen = 0;
    const int wantIndex = qMax(0, occurrenceIndex);
    AXUIElementRef found = nullptr;

    // SPEC.md 追加実装依頼「メニューバーの項目も対象にしたい」: the app's
    // menu bar is not reachable via its AXChildren the way its windows are
    // -- it needs its own kAXMenuBarAttribute lookup -- so it is searched
    // explicitly, before the windows, rather than relying on the generic
    // recursive search to stumble onto it.
    AXUIElementRef menuBar = nullptr;
    if (AXUIElementCopyAttributeValue(app, kAXMenuBarAttribute, (CFTypeRef *)&menuBar) ==
            kAXErrorSuccess &&
        menuBar) {
        found = findRoleNamedElementRecursive(menuBar, role, name, nameMatchMode, wantIndex, matchesSeen,
                                               0, budget);
        CFRelease(menuBar);
    }

    if (!found) {
        CFArrayRef windows = nullptr;
        if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, (CFTypeRef *)&windows) ==
                kAXErrorSuccess &&
            windows) {
            const CFIndex count = CFArrayGetCount(windows);
            for (CFIndex i = 0; i < count && !found && budget > 0; ++i) {
                AXUIElementRef win = (AXUIElementRef)CFArrayGetValueAtIndex(windows, i);
                found = findRoleNamedElementRecursive(win, role, name, nameMatchMode, wantIndex,
                                                       matchesSeen, 0, budget);
            }
            CFRelease(windows);
        }
    }

    CFRelease(app);
    if (!found)
        return handle;

    handle.m_impl = std::make_unique<AccessibleObjectHandle::Impl>();
    handle.m_impl->element = found;  // adopts the reference findRoleNamedElementRecursive returned
    handle.m_impl->valid = true;
    return handle;
}

QList<EditableFieldInfo> findEditableObjectsInRegion(qint64 expectedOwnerPid, const QRect &regionBounds)
{
    QList<EditableFieldInfo> result;
    if (accessibilityFeaturesDisabled())
        return result;

    AXUIElementRef app = AXUIElementCreateApplication((pid_t)expectedOwnerPid);
    if (!app)
        return result;

    // Bounded result count -- see the Linux implementation's identical
    // comment.
    constexpr int kMaxResults = 200;
    CFArrayRef windows = nullptr;
    if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, (CFTypeRef *)&windows) == kAXErrorSuccess &&
        windows) {
        const CFIndex count = CFArrayGetCount(windows);
        for (CFIndex i = 0; i < count && result.size() < kMaxResults; ++i) {
            AXUIElementRef win = (AXUIElementRef)CFArrayGetValueAtIndex(windows, i);
            int budget = 4000;
            collectEditableElementsRecursive(win, regionBounds, 0, budget, result, kMaxResults);
        }
        CFRelease(windows);
    }
    CFRelease(app);
    return result;
}

QString findRecentCrashReport(qint64 /*pid*/, const QString &appName)
{
    if (appName.isEmpty())
        return QString();

    NSString *dirPath =
        [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Logs/DiagnosticReports"];
    NSFileManager *fm = [NSFileManager defaultManager];
    NSError *error = nil;
    NSArray<NSString *> *entries = [fm contentsOfDirectoryAtPath:dirPath error:&error];
    if (!entries)
        return QString();

    NSString *needle = appName.toNSString();
    NSString *bestPath = nil;
    NSDate *bestDate = nil;
    const NSTimeInterval now = [[NSDate date] timeIntervalSince1970];
    for (NSString *name in entries) {
        // Modern macOS writes ".ips" (JSON) reports; older versions wrote
        // ".crash" (plain text). Match either, by the crashed executable's
        // name, which both formats include as a filename prefix.
        if (![name.lowercaseString hasSuffix:@".ips"] && ![name.lowercaseString hasSuffix:@".crash"])
            continue;
        if ([name rangeOfString:needle options:NSCaseInsensitiveSearch].location == NSNotFound)
            continue;

        NSString *fullPath = [dirPath stringByAppendingPathComponent:name];
        NSDictionary *attrs = [fm attributesOfItemAtPath:fullPath error:nil];
        NSDate *modified = attrs[NSFileModificationDate];
        if (!modified)
            continue;
        // Only trust a match written recently -- see the Linux
        // implementation's equivalent comment.
        if (now - modified.timeIntervalSince1970 > 300.0)
            continue;
        if (!bestDate || [modified compare:bestDate] == NSOrderedDescending) {
            bestDate = modified;
            bestPath = fullPath;
        }
    }
    return bestPath ? QString::fromNSString(bestPath) : QString();
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

// SPEC.md 追加実装依頼「ビューワー耐久テスト強化」: unlike keyShortcut()
// above (which only needs to set CGEventFlags on the one event it sends),
// holding a modifier down across many *separate* subsequent events (mouse
// drags, scrolls, key taps) needs a real, persistent key-down -- posting a
// keyboard event for the modifier's own virtual key code changes macOS's
// global modifier state for every event posted afterward until the
// matching key-up, the same way a real key press would.
void modifierKeysDown(Qt::KeyboardModifiers modifiers)
{
    if (modifiers & Qt::ShiftModifier) {
        CGEventRef e = CGEventCreateKeyboardEvent(nullptr, kVK_Shift, true);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
    if (modifiers & Qt::ControlModifier) {  // Qt's portable Ctrl == Cmd on macOS
        CGEventRef e = CGEventCreateKeyboardEvent(nullptr, kVK_Command, true);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
    if (modifiers & Qt::AltModifier) {
        CGEventRef e = CGEventCreateKeyboardEvent(nullptr, kVK_Option, true);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
    if (modifiers & Qt::MetaModifier) {  // Qt's portable Meta == Ctrl on macOS
        CGEventRef e = CGEventCreateKeyboardEvent(nullptr, kVK_Control, true);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
}

void modifierKeysUp(Qt::KeyboardModifiers modifiers)
{
    if (modifiers & Qt::ShiftModifier) {
        CGEventRef e = CGEventCreateKeyboardEvent(nullptr, kVK_Shift, false);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
    if (modifiers & Qt::ControlModifier) {
        CGEventRef e = CGEventCreateKeyboardEvent(nullptr, kVK_Command, false);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
    if (modifiers & Qt::AltModifier) {
        CGEventRef e = CGEventCreateKeyboardEvent(nullptr, kVK_Option, false);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
    if (modifiers & Qt::MetaModifier) {
        CGEventRef e = CGEventCreateKeyboardEvent(nullptr, kVK_Control, false);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
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
