#pragma once

// Platform facade over OS-level accessibility / input-injection APIs used to
// drive endurance-test input into another process's window.
//
// Implemented per-platform:
//   - macOS: src/platform/macos/Automation_mac.mm   (Objective-C++, Quartz Event Services)
//   - Linux: src/platform/linux/Automation_linux.cpp (Xlib + XTest extension)
//
// This header must stay plain, portable C++ (no Objective-C or X11 types),
// since it is included from MainWindow.cpp and RandomActionEngine.cpp on
// every platform.

#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <Qt>

struct WindowInfo
{
    qint64 pid = -1;
    quint32 windowId = 0;
    QString appName;
    QString title;
    QRect bounds;  // screen coordinates, top-left origin, y down
};

// A snapshot of one process's resource usage, for the endurance-test log
// (memory-leak/CPU-runaway detection over a long run -- see SPEC.md 6.7/8.1).
struct ProcessStats
{
    bool ok = false;             // false if the process/stats couldn't be read
    double residentMemoryMB = 0.0;
    double cpuTimeSeconds = 0.0;  // cumulative user+system CPU time consumed so far
};

namespace PlatformAutomation
{

// Permission needed to post synthetic input into another application
// (macOS: Accessibility; Linux/X11: none beyond X server access, so this
// is always true there). promptIfNeeded(true) may trigger the OS's own
// permission dialog on first call.
bool isAccessibilityTrusted(bool promptIfNeeded);
void openAccessibilitySettings();

// Enumerates on-screen, normal top-level windows owned by other running
// processes/applications. Excludes this process's own windows.
QList<WindowInfo> listWindows();

// Re-reads the current bounds of a specific window, if it still exists.
// Returns false if the window can no longer be found.
bool queryWindowBounds(quint32 windowId, qint64 pid, QRect &outBounds);

bool activateProcess(qint64 pid);

// True if the process still exists (used to detect the target app having
// crashed/quit unexpectedly during a run, distinct from its window merely
// having closed).
bool isProcessRunning(qint64 pid);

// Current resident memory and cumulative CPU time for this process. Sample
// twice and divide the CPU-time delta by the wall-clock delta to get a CPU
// percentage (see RandomActionEngine's resource-usage sampler).
ProcessStats queryProcessStats(qint64 pid);

// --- Safety checks -----------------------------------------------------
// Used immediately before dispatching each action to confirm it will
// actually land on the intended target and not some other window/app --
// e.g. because the target moved, was covered by another window, lost
// focus, or closed. If either check fails, RandomActionEngine stops the
// run rather than risk operating an unintended application (see SPEC.md
// 6.7).

// pid of the topmost on-screen window at this point (screen coordinates),
// or -1 if none (e.g. empty desktop, or only this process's own windows
// are there). Used to verify a click/drag/scroll is about to hit the
// intended target window.
qint64 windowPidAtPoint(const QPoint &pt);

// pid of the currently active/frontmost application, or -1 if unknown.
// Used to verify keyboard input (key taps, shortcuts) is actually going
// to the intended target, since those aren't aimed at a screen point.
qint64 activeProcessPid();

void moveMouse(const QPoint &pt);
void mouseClick(const QPoint &pt, Qt::MouseButton button);
void mouseDrag(const QPoint &from, const QPoint &to, Qt::MouseButton button, int steps);
void scroll(const QPoint &pt, int dx, int dy);

// Sends a single key press+release for a printable character. Characters
// this platform's backend cannot map to a key event are silently ignored.
void keyTap(QChar ch);

// Sends a modifier-combination shortcut (e.g. Qt::Key_C with
// Qt::ControlModifier). On macOS, Qt's portable Ctrl modifier maps to Cmd
// as usual. Keys this platform's backend cannot map are silently ignored.
void keyShortcut(Qt::Key key, Qt::KeyboardModifiers modifiers);

// Sends a single press+release of a named/functional key with no
// modifiers (Tab, Return, Escape, Backspace, Delete, arrow keys). These
// have no printable character of their own, unlike keyTap(QChar), so they
// need their own platform key-code mapping. Keys this platform's backend
// cannot map are silently ignored.
void keyTapNamed(Qt::Key key);

// --- Window-level operations --------------------------------------------
// Move/resize/minimize/maximize the target's own window (as opposed to
// everything above, which operates on content *inside* a window). All
// take the pid+windowId pair so the backend can verify it still identifies
// the intended target's window before touching it; they return false
// (doing nothing) if that window can no longer be found for that pid.

bool moveWindow(qint64 pid, quint32 windowId, const QPoint &newTopLeft);
bool resizeWindow(qint64 pid, quint32 windowId, const QSize &newSize);
bool minimizeWindow(qint64 pid, quint32 windowId);

// Resizes+repositions the window to fill the available area of whichever
// screen it's currently on (rather than relying on a per-window-manager
// native "maximized" state, so the effect is uniform across platforms).
bool maximizeWindow(qint64 pid, quint32 windowId);

// --- Experimental: context/popup menu introspection -----------------
// Used to pick a specific item out of a menu that just opened (e.g. after
// a synthetic right-click). Best-effort: relies on OS accessibility trees
// (macOS Accessibility API / Linux AT-SPI, the latter only when built with
// AT-SPI development headers available) and has not been exhaustively
// verified against every toolkit -- see SPEC.md "既知の制約".

// `expectedOwnerPid` is the target process's pid; on macOS the backend
// verifies the topmost window it treats as "the menu" is actually owned
// by that pid before introspecting/clicking it (returning as if no menu
// were found otherwise), so a menu belonging to some other app that
// happened to be topmost isn't mistaken for the target's. (On Linux, the
// AT-SPI search is desktop-wide and not scoped to a pid; the caller's own
// active-process focus check is the primary guard there -- see SPEC.md
// "既知の制約".)

// Names of the items in the currently-open popup/context menu, if any.
// Returns an empty list if no menu is open or it could not be introspected.
QStringList listOpenContextMenuItems(qint64 expectedOwnerPid);

// Activates (clicks) the open context/popup menu item with this exact
// name. Returns false if no such item could be found/activated.
bool clickContextMenuItem(const QString &itemName, qint64 expectedOwnerPid);

// Activates (clicks) the open context/popup menu item at this 0-based
// index (position from the top). Returns false if there is no open menu
// or the index is out of range.
bool clickContextMenuItemAt(int index, qint64 expectedOwnerPid);

// Dismisses an open popup/context menu (sends Escape).
void dismissContextMenu();

}  // namespace PlatformAutomation
