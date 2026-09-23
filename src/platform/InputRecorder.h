#pragma once

#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>

#include "TestConfig.h"  // SetupActionType, ContextMenuSelectionMode

// Startup-setup "記録" (record) button (SPEC.md 6.13): while active, observes
// the user's mouse clicks/drags and keyboard input *system-wide* -- including
// inside dialogs the target app pops up -- and turns each completed gesture
// into a SetupAction, appended to the startup setup list in real time.
// Recording stops automatically the instant Escape is pressed (also usable
// to cancel a gesture in progress without it being recorded).
//
// Deliberately observes rather than grabs: input still reaches whichever
// window the user is actually clicking/typing into normally (the target
// app, one of its dialogs, ...); this class only watches. That's what makes
// it usable against real dialogs at all -- a global pointer/keyboard grab
// would instead make the target app itself unable to receive the very input
// being recorded, defeating the point ("ダイアログも含めた操作ができるよう
// に" in the original request).
//
// actionRecorded()'s point/dragToPoint are screen-absolute; MainWindow
// translates them to the usual target-window-relative representation
// (matching every other SetupAction/NamedRegion in this app) once each
// action arrives, using whichever window bounds are current *at that
// moment* -- deliberately not baked in here, so a dialog that's still
// being repositioned while being recorded doesn't skew the result.
//
// Scope note: recorded left-clicks are always emitted as a plain Click
// (SetupActionType::Click), never DoubleClick -- distinguishing an
// intentional double-click from two independent quick clicks by timing
// alone is unreliable, so that distinction is left to editing the recorded
// entry afterward in SetupActionEditorDialog, the same as any other field.
// Likewise, non-ASCII characters and dead-key/compose sequences are not
// recognized (silently dropped from recorded text) -- this app's own typed
// text elsewhere (ActionParams::allowedKeyChars) is already ASCII-only by
// default, so this matches its existing scope rather than narrowing it.
//
// Implemented per-platform behind a pimpl, the same shape as GlobalHotkey:
//   - Linux: src/platform/linux/InputRecorder_linux.cpp (XInput2 raw events,
//     which are delivered to every client that selects them regardless of
//     window focus/grabs, and don't interfere with normal event delivery)
//   - macOS: src/platform/macos/InputRecorder_mac.mm (CGEventTap installed
//     as a listen-only tap -- kCGEventTapOptionListenOnly -- so it can't
//     itself block/alter events reaching the real target; not independently
//     verified on real macOS hardware, see SPEC.md 8章)
class InputRecorder : public QObject
{
    Q_OBJECT

public:
    explicit InputRecorder(QObject *parent = nullptr);
    ~InputRecorder() override;

    // Shared setup for members that need `this` to already be a
    // constructed QObject (e.g. m_menuGraceTimer's parent) -- called once
    // from each platform constructor (InputRecorder_linux.cpp/
    // InputRecorder_mac.mm), right after m_impl is allocated, since the
    // constructor body itself is otherwise platform-specific. Defined in
    // the shared InputRecorder.cpp alongside the rest of this class's
    // platform-independent logic.
    void initSharedState();

    // Starts observing. Returns false if the platform hook could not be
    // installed (e.g. the XInput2 extension isn't available, or macOS
    // Accessibility/Input Monitoring permission isn't granted) -- callers
    // should surface that as an error rather than silently recording
    // nothing.
    bool start();
    void stop();
    bool isRecording() const { return m_recording; }

    // Public (not just callable from within this class) so each platform
    // backend's C-style event callback -- which isn't a member function --
    // has a way to feed raw input into the shared, platform-independent
    // click/drag/text-buffering logic below. Not intended to be called
    // from application code.
    void notifyMouseButton(Qt::MouseButton button, bool pressed, const QPoint &screenPos);
    void notifyKeyEvent(int qtKey, const QString &printableText, bool pressed);
    // One wheel "notch" at `screenPos`, in PlatformAutomation::scroll()'s own
    // dx/dy sign convention (see TestConfig.h's SetupAction::scrollDx/Dy
    // comment) -- called once per notch, not once per pixel/line, since
    // that's the smallest unit the platform backends can observe (a wheel
    // button press/release pair on Linux, a discrete scroll-wheel event on
    // macOS). Consecutive notches at (about) the same position accumulate
    // into a single Scroll SetupAction the same way consecutive characters
    // accumulate into one TypeText -- see m_wheelAccumDx/Dy.
    void notifyWheelScroll(const QPoint &screenPos, int dx, int dy);
    // Called by the platform backend the moment Escape is observed while
    // recording, from a context that (unlike the two methods above) needs
    // to end recording entirely -- flushes any pending text, tears the
    // platform hook down, and emits finished(true). Backends must defer
    // this call (e.g. Qt::QueuedConnection) rather than invoke it
    // synchronously from inside their own event callback, since it frees
    // resources that callback is still executing from.
    void notifyEscapePressed();

signals:
    // point/dragToPoint are screen-absolute; see class comment. Unused
    // fields for a given `type` are default-constructed (empty/zero, or 1
    // for menuItemIndex since SetupActionEditorDialog/SetupAction treat 0 as
    // out of range for a 1-based position).
    void actionRecorded(SetupActionType type, QPoint point, QPoint dragToPoint, QString text,
                         QString keySequence, int scrollDx, int scrollDy,
                         ContextMenuSelectionMode menuSelectionMode, QString menuItemName,
                         int menuItemIndex);
    // Recording ended -- true if Escape was what ended it, false if stop()
    // was called programmatically instead (e.g. the panel's own button, or
    // MainWindow tearing down for some other reason).
    void finished(bool escapePressed);

private:
    struct Impl;
    Impl *m_impl = nullptr;

    bool m_recording = false;

    // Mouse gesture in progress (Click vs Drag is only known once the
    // button is released, by comparing positions -- see notifyMouseButton).
    bool m_buttonDown = false;
    Qt::MouseButton m_downButton = Qt::NoButton;
    QPoint m_downPos;

    // Consecutive plain-character key presses (no Ctrl/Alt/Meta held)
    // accumulate here and are flushed as a single TypeText SetupAction as
    // soon as anything else happens (a mouse gesture, a named/modified key,
    // or recording stopping) -- mirrors how a human would describe what
    // they just did ("typed the username, then clicked Next"), rather than
    // emitting one SetupAction per keystroke.
    QString m_textBuffer;

    // Held-modifier tracking, updated from notifyKeyEvent() whenever a
    // modifier key itself is pressed/released (raw key events carry no
    // per-event modifier-state field the way normal Qt/X11 key events do).
    Qt::KeyboardModifiers m_heldModifiers = Qt::NoModifier;

    // Consecutive wheel notches at (about) the same position accumulate
    // here, flushed as a single Scroll SetupAction the same way
    // m_textBuffer is -- see notifyWheelScroll().
    int m_wheelAccumDx = 0;
    int m_wheelAccumDy = 0;
    QPoint m_wheelPos;
    void flushWheelBuffer();

    // SPEC.md 6.13追加実装及び修正依頼「メニューの選択...を記録」: a right-
    // click's matching RightClick SetupAction isn't emitted immediately on
    // release -- instead this flag/position are set and a short grace
    // period (m_menuGraceTimer) starts, giving a *following* left click a
    // chance to arrive. If one does before the timer fires, it's treated as
    // "the user just picked an item from the menu that right-click opened"
    // (see notifyMouseButton()) and the whole gesture becomes a single
    // MenuSelect action instead of a separate RightClick + Click pair. If
    // nothing else arrives in time (or a non-left event arrives first), the
    // pending RightClick is flushed as a plain right-click, same as always.
    bool m_awaitingMenuSelection = false;
    QPoint m_pendingRightClickPos;
    QTimer *m_menuGraceTimer = nullptr;  // parented to `this`, see constructor
    void flushPendingRightClick();
    // Called for the left press that arrives while m_awaitingMenuSelection
    // is true: tries to identify the menu item under `screenPos` via
    // accessibility introspection and, if found, emits a single MenuSelect
    // action for the whole right-click-then-select gesture. If no
    // accessible name can be found there (introspection unsupported/failed
    // -- a real possibility, see PlatformAutomation::accessibleNameAtPoint's
    // own "best-effort" contract), falls back to emitting the pending
    // RightClick on its own and lets this left press continue through the
    // normal click/drag tracking below, so nothing recorded is silently
    // lost even when the smart merge can't be done.
    void handlePossibleMenuSelectionClick(const QPoint &screenPos);

    void flushTextBuffer();
    // Shared: resets recording state and emits finished(). Called by stop()
    // and, from the platform backend, right after teardownHook() when
    // Escape was what ended recording.
    void finishRecording(bool escapePressed);
    // Platform-specific: tears down the OS-level hook installed by start().
    // Implemented in InputRecorder_linux.cpp/InputRecorder_mac.mm alongside
    // start() itself and the pimpl's construction/destruction.
    void teardownHook();
};
