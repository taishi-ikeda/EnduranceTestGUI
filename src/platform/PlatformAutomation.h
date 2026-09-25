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

// Permission needed for QScreen::grabWindow() to actually capture other
// applications' on-screen content into the pixmap it returns (macOS: Screen
// Recording, required since macOS 10.15 Catalina; Linux/X11: no such
// restriction, always true here). Without it, grabWindow() does not fail or
// return a null pixmap -- it silently returns a blank/black image instead,
// which is what OverlayGeometry::grabVirtualDesktopSnapshot() paints as the
// "see-through" background for RegionSelectorOverlay/PointPickerOverlay
// (SPEC.md 6.3/6.13). The visible symptom is the overlay appearing to make
// every other window vanish behind a black screen, even though nothing is
// actually hidden -- only the snapshot used to fake transparency failed
// (SPEC.md 10 追加実装及び修正依頼). Check-only (never prompts) so it's
// safe to call from a passive UI refresh, matching isAccessibilityTrusted(false)'s
// usage in refreshPermissionLabel().
bool isScreenRecordingTrusted();
void openScreenRecordingSettings();

// Whether a real, live-transparent top-level window (Qt::WA_TranslucentBackground)
// can be trusted to actually render as see-through on this platform, instead of
// silently rendering as solid black the way it does on a Linux window manager
// with no compositor running (see OverlayGeometry::grabVirtualDesktopSnapshot()'s
// comment for that failure mode, and why RegionSelectorOverlay/PointPickerOverlay/
// RegionHighlightOverlay fall back to painting a captured screenshot as an opaque
// background instead). macOS's WindowServer composites every window
// unconditionally -- there is no "no compositor" case to guard against there --
// so real transparency is always safe on macOS and is used in preference to the
// screenshot-based fallback when this returns true.
//
// This also sidesteps a macOS-specific misalignment the screenshot approach had
// (SPEC.md追加実装及び修正依頼, reported as "two dialogs appear" -- #51): these
// overlays size/position themselves using QScreen::geometry() (the screen's full
// pixel bounds, menu bar/Dock rows included), but neither the real menu bar nor
// the real Dock can actually be drawn over by an ordinary window -- so the OS may
// shift where the window is actually placed on screen to avoid overlapping them,
// while the captured screenshot painted inside it still starts from row 0 of the
// *full* screen. The frozen image the user sees then drifts out of alignment
// with the real screen underneath by roughly the menu bar's height, so a click
// aimed at something visible in the (misaligned) image can land on a different
// real control than intended. A genuinely transparent window has no captured
// image to misalign in the first place -- mouse coordinates are simply read
// against whatever is really on screen.
bool supportsWindowTransparency();

// Enumerates on-screen, normal top-level windows owned by other running
// processes/applications. Excludes this process's own windows.
QList<WindowInfo> listWindows();

// Re-reads the current bounds of a specific window, if it still exists.
// Returns false if the window can no longer be found.
bool queryWindowBounds(quint32 windowId, qint64 pid, QRect &outBounds);

// IDs of all currently on-screen top-level windows owned by this pid (the
// same enumeration listWindows() draws from, just pre-filtered to one
// process instead of returning full WindowInfo for every process). Used to
// notice a *second* window belonging to the target appearing next to the
// one the user originally selected -- e.g. an unexpected confirmation
// dialog -- which RandomActionEngine treats as something to dismiss rather
// than something to click on (SPEC.md 6.7/10).
QList<quint32> listWindowIdsForPid(qint64 pid);

bool activateProcess(qint64 pid);

// True if the process still exists (used to detect the target app having
// crashed/quit unexpectedly during a run, distinct from its window merely
// having closed).
bool isProcessRunning(qint64 pid);

// Best-effort, asynchronous request that the process exit (SIGTERM on
// Linux/macOS) -- used by 連続実行 (SPEC.md 10 ⑤) to get rid of a leftover
// target instance before launching a fresh one. Does not wait for the
// process to actually exit: callers must poll isProcessRunning() themselves
// (see MainWindow::killTargetThenRelaunchForContinuousRun()) before treating
// it as gone, since a process can ignore or take time to act on the signal.
// No-op (and safe to call) if the pid is already gone.
void terminateProcess(qint64 pid);

// Current resident memory and cumulative CPU time for this process. Sample
// twice and divide the CPU-time delta by the wall-clock delta to get a CPU
// percentage (see RandomActionEngine's resource-usage sampler).
ProcessStats queryProcessStats(qint64 pid);

enum class ResponsivenessCheck {
    // No prior probe to evaluate yet (the very first call for this
    // windowId) -- not evidence of anything either way. Callers must not
    // treat this the same as Responding: doing so would let one call's
    // "no data yet" get mistaken for a confirmed reply, undermining any
    // logic that (correctly) wants to see at least one *real* Responding
    // result before ever trusting a later NotResponding as a genuine hang.
    Pending,
    Responding,
    NotResponding,
    Unsupported,
};

// Best-effort "is the target's event loop still processing input" check
// (distinct from isProcessRunning() above, which only tells you the
// process hasn't exited -- a hung/deadlocked process is still "running" by
// that measure). Stateful and meant to be polled periodically (see
// RandomActionEngine's hang-check timer): each call both evaluates the
// probe sent by the *previous* call (if any, for this same windowId) and
// sends a new one for next time, trading a tick of latency for never
// blocking the caller waiting on a reply. Returns Unsupported if this
// platform/window doesn't support the underlying protocol at all --
// callers should stop polling in that case rather than treat it as a
// negative result (SPEC.md 8/10).
ResponsivenessCheck checkWindowResponsive(quint32 windowId, qint64 pid);

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

// SPEC.md 6.2追加実装及び修正依頼「タスク内で操作途中で出てくるダイアログの
// ボタンを押したり特定の操作をできるようにしてほしい」: searches for a
// button (or any other actionable widget) with this exact accessible name
// and activates it, using the same accessibility backend as the
// context-menu functions above. On macOS the frontmost window owned by
// `expectedOwnerPid` is searched (same "assume frontmost == the dialog
// that just appeared" technique as findOpenMenuElement()); on Linux the
// search is desktop-wide and not pid-scoped, same caveat as
// listOpenContextMenuItems() above -- the caller's own active-process
// check (RandomActionEngine, right before calling this) is the guard.
// Returns false if no matching, activatable widget could be found.
bool clickButtonByName(const QString &buttonName, qint64 expectedOwnerPid);

// --- Diagnostics (SPEC.md 6.7/6.8/10) -----------------------------------
// Best-effort hints for reproducing/diagnosing a bug found during an
// endurance-test run, gathered the same way the context-menu introspection
// above is: OS accessibility trees and standard crash-report locations.
// Both are allowed to simply return an empty string when nothing useful
// could be found -- callers must treat that as "no information available",
// never as an error.

// Accessible name (or, if it has none, role) of whatever UI element is at
// this screen point, for more readable action logs -- e.g. so a log line
// can say `クリック(左) at (837, 291) [ボタン "OK"]` instead of bare
// coordinates that mean nothing without also knowing the window's on-screen
// position at that exact moment. Uses the same accessibility backend as
// listOpenContextMenuItems() above (macOS Accessibility API / Linux AT-SPI)
// and is just as best-effort/unscoped -- not guaranteed to work with every
// toolkit, and on Linux not scoped to a particular process.
QString accessibleNameAtPoint(const QPoint &pt);

// Searches for a native crash report or core dump generated recently for
// `pid`/`appName` (macOS: ~/Library/Logs/DiagnosticReports; Linux:
// systemd-coredump's /var/lib/systemd/coredump and apport's /var/crash).
// Returns the found file's path, or an empty string if none could be
// located -- which is common: many systems don't keep crash reports at
// all, don't have the relevant service enabled, or this process lacks
// permission to read that location. Meant to be called once, right after
// noticing the target process has crashed (SPEC.md 6.7).
QString findRecentCrashReport(qint64 pid, const QString &appName);

}  // namespace PlatformAutomation
