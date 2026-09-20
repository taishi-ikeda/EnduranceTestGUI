#include "platform/GlobalHotkey.h"

#import <Carbon/Carbon.h>

// Uses the Carbon Event Manager's RegisterEventHotKey -- still fully
// functional on modern macOS despite Carbon's UI toolkit itself being long
// deprecated, and already linked into this app (Carbon.framework, see
// CMakeLists.txt) for the virtual-keycode constants Automation_mac.mm uses.
// Unlike a CGEventTap-based global hotkey, this does not require its own
// separate permission grant beyond the Accessibility trust this app already
// needs for everything else (see PlatformAutomation::isAccessibilityTrusted).
struct GlobalHotkey::Impl
{
    EventHotKeyRef hotKeyRef = nullptr;
    EventHandlerRef handlerRef = nullptr;
};

namespace
{
constexpr OSType kHotkeySignature = 'etgs';  // "EnduranceTestGUI Stop"

OSStatus hotkeyEventHandler(EventHandlerCallRef /*nextHandler*/, EventRef event, void *userData)
{
    auto *self = static_cast<GlobalHotkey *>(userData);
    EventHotKeyID hkID;
    if (GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(hkID),
                           nullptr, &hkID) == noErr &&
        hkID.signature == kHotkeySignature && hkID.id == 1) {
        self->notifyTriggered();
    }
    return noErr;
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
    const EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
    if (InstallApplicationEventHandler(&hotkeyEventHandler, 1, &spec, this, &m_impl->handlerRef) !=
        noErr) {
        m_impl->handlerRef = nullptr;
        return false;
    }

    EventHotKeyID hkID;
    hkID.signature = kHotkeySignature;
    hkID.id = 1;
    // controlKey/optionKey/shiftKey are Carbon modifier-mask constants
    // (distinct from the CGEventFlags used elsewhere in this app's macOS
    // backend) -- Ctrl+Option(Alt)+Shift+Escape.
    const UInt32 modifiers = controlKey | optionKey | shiftKey;
    const OSStatus err = RegisterEventHotKey(kVK_Escape, modifiers, hkID, GetApplicationEventTarget(),
                                              0, &m_impl->hotKeyRef);
    if (err != noErr) {
        m_impl->hotKeyRef = nullptr;
        RemoveEventHandler(m_impl->handlerRef);
        m_impl->handlerRef = nullptr;
        return false;
    }
    return true;
}

void GlobalHotkey::stop()
{
    if (m_impl->hotKeyRef) {
        UnregisterEventHotKey(m_impl->hotKeyRef);
        m_impl->hotKeyRef = nullptr;
    }
    if (m_impl->handlerRef) {
        RemoveEventHandler(m_impl->handlerRef);
        m_impl->handlerRef = nullptr;
    }
}

void GlobalHotkey::notifyTriggered()
{
    emit triggered();
}
