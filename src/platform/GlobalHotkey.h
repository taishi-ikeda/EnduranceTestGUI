#pragma once

#include <QObject>

// Cross-platform "emergency stop" global hotkey: Ctrl+Alt+Shift+Escape,
// active system-wide regardless of which application currently has focus.
// A backstop for SPEC.md 6.7/10's StopPanel button, for the case where the
// target application has grabbed keyboard/mouse input in a way that makes
// even that always-on-top panel hard to reach (e.g. a fullscreen/kiosk-mode
// target, or one that's stopped responding to normal input routing).
//
// Best-effort: start() returns false if the combo could not be registered
// (e.g. already claimed by the desktop environment or another app) --
// callers should treat that as "no global hotkey available this session"
// and keep relying on the floating panel, not as a fatal error.
//
// Implemented per-platform behind a pimpl (Impl), so this header stays
// portable (no X11/Cocoa types) the same way PlatformAutomation.h does:
//   - Linux: src/platform/linux/GlobalHotkey_linux.cpp (Xlib XGrabKey)
//   - macOS: src/platform/macos/GlobalHotkey_mac.mm (Carbon RegisterEventHotKey)
class GlobalHotkey : public QObject
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    bool start();
    void stop();

    // Public (not just callable from within this class) so each platform
    // backend's C-style event callback -- which isn't a member function
    // and so cannot call the protected `triggered` signal directly -- has
    // a way to raise it. Not intended to be called from application code.
    void notifyTriggered();

signals:
    void triggered();

private:
    struct Impl;
    Impl *m_impl = nullptr;
};
