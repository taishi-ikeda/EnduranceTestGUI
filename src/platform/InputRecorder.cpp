#include "InputRecorder.h"

#include <QKeySequence>

#include "platform/PlatformAutomation.h"

// Shared (platform-independent) half of InputRecorder: the click/drag/
// text-buffering logic that turns raw button/key notifications into
// SetupAction-shaped signals. See InputRecorder.h for why this is split
// from the per-platform hook itself (InputRecorder_linux.cpp/
// InputRecorder_mac.mm), which own start()/teardownHook() and the pimpl's
// definition/lifetime (this file only ever sees InputRecorder::Impl as an
// opaque forward-declared pointer, and never touches it).

namespace
{
// Minimum screen-pixel distance between a mouse-down and its matching
// mouse-up to count as a drag rather than a click -- small enough that a
// deliberate drag is never misread as a click, large enough that ordinary
// hand tremor/imprecision on a plain click doesn't get misread as a drag.
constexpr int kDragThresholdPx = 6;

// How long to wait, after a right-click's release, for a following left
// click to arrive before giving up and just recording a plain right-click
// (SPEC.md 6.13追加実装及び修正依頼) -- long enough to comfortably cover a
// human picking a menu item they can see (menus render essentially
// instantly), short enough that a right-click *not* followed by any menu
// interaction doesn't feel delayed in the recorded list.
constexpr int kMenuSelectionGraceMs = 600;

// A trailing-fields "this action doesn't use any of the Scroll/MenuSelect
// fields" helper -- most actionRecorded() emissions only care about their
// own first few parameters; spelling out the same four neutral defaults at
// every such call site would just be noise.
void emitSimpleAction(InputRecorder *self, SetupActionType type, const QPoint &point,
                       const QPoint &dragToPoint, const QString &text, const QString &keySequence)
{
    emit self->actionRecorded(type, point, dragToPoint, text, keySequence, 0, 0,
                               ContextMenuSelectionMode::ByName, QString(), 1);
}
}  // namespace

void InputRecorder::initSharedState()
{
    m_menuGraceTimer = new QTimer(this);
    m_menuGraceTimer->setSingleShot(true);
    connect(m_menuGraceTimer, &QTimer::timeout, this, &InputRecorder::flushPendingRightClick);
}

void InputRecorder::stop()
{
    if (!m_recording)
        return;
    teardownHook();
    finishRecording(/*escapePressed=*/false);
}

void InputRecorder::notifyEscapePressed()
{
    if (!m_recording)
        return;
    teardownHook();
    finishRecording(/*escapePressed=*/true);
}

void InputRecorder::finishRecording(bool escapePressed)
{
    flushPendingRightClick();
    flushWheelBuffer();
    flushTextBuffer();
    m_recording = false;
    m_buttonDown = false;
    m_downButton = Qt::NoButton;
    m_heldModifiers = Qt::NoModifier;
    emit finished(escapePressed);
}

void InputRecorder::flushTextBuffer()
{
    if (m_textBuffer.isEmpty())
        return;
    const QString text = m_textBuffer;
    m_textBuffer.clear();
    emitSimpleAction(this, SetupActionType::TypeText, QPoint(), QPoint(), text, QString());
}

void InputRecorder::flushWheelBuffer()
{
    if (m_wheelAccumDx == 0 && m_wheelAccumDy == 0)
        return;
    const int dx = m_wheelAccumDx;
    const int dy = m_wheelAccumDy;
    m_wheelAccumDx = 0;
    m_wheelAccumDy = 0;
    emit actionRecorded(SetupActionType::Scroll, m_wheelPos, QPoint(), QString(), QString(), dx, dy,
                         ContextMenuSelectionMode::ByName, QString(), 1);
}

void InputRecorder::flushPendingRightClick()
{
    if (!m_awaitingMenuSelection)
        return;
    m_awaitingMenuSelection = false;
    m_menuGraceTimer->stop();
    emitSimpleAction(this, SetupActionType::RightClick, m_pendingRightClickPos, QPoint(), QString(),
                      QString());
}

void InputRecorder::handlePossibleMenuSelectionClick(const QPoint &screenPos)
{
    m_menuGraceTimer->stop();
    m_awaitingMenuSelection = false;
    const QString itemName = PlatformAutomation::accessibleNameAtPoint(screenPos);
    if (!itemName.isEmpty()) {
        // The whole right-click-then-select gesture becomes one MenuSelect
        // action, anchored at the right-click's own position (matching a
        // manually-created MenuSelect's point() semantics -- see
        // TestConfig.h). This left click's press is swallowed entirely: its
        // matching release will find m_buttonDown still false below and
        // no-op on its own, so nothing further needs to happen here.
        emit actionRecorded(SetupActionType::MenuSelect, m_pendingRightClickPos, QPoint(), QString(),
                             QString(), 0, 0, ContextMenuSelectionMode::ByName, itemName, 1);
        return;
    }
    // Couldn't identify an item under the click (introspection unavailable/
    // failed -- a real possibility, see accessibleNameAtPoint's own
    // best-effort contract; confirmed by direct testing: this project's own
    // Xvfb/openbox test container has no D-Bus session bus at all, so
    // atspiUsable() always short-circuits and this branch is the only one
    // ever reachable there -- see SPEC.md 8章) -- record the right-click
    // that opened the menu on its own, then let this left press continue
    // through the normal click/drag tracking below so it's still captured
    // as *something* rather than silently lost.
    emitSimpleAction(this, SetupActionType::RightClick, m_pendingRightClickPos, QPoint(), QString(),
                      QString());
    flushTextBuffer();
    flushWheelBuffer();
    m_buttonDown = true;
    m_downButton = Qt::LeftButton;
    m_downPos = screenPos;
}

void InputRecorder::notifyWheelScroll(const QPoint &screenPos, int dx, int dy)
{
    if (!m_recording)
        return;
    flushPendingRightClick();  // a wheel notch ends any pending menu-selection window
    if (m_wheelAccumDx == 0 && m_wheelAccumDy == 0) {
        flushTextBuffer();  // starting a new wheel gesture ends any text-typing context
        m_wheelPos = screenPos;
    }
    m_wheelAccumDx += dx;
    m_wheelAccumDy += dy;
    m_wheelPos = screenPos;  // keep the most recent position for the eventual flush
}

void InputRecorder::notifyMouseButton(Qt::MouseButton button, bool pressed, const QPoint &screenPos)
{
    if (!m_recording)
        return;
    if (button != Qt::LeftButton && button != Qt::RightButton)
        return;  // middle/other buttons: no matching SetupActionType, ignore

    if (pressed && button == Qt::LeftButton && m_awaitingMenuSelection) {
        handlePossibleMenuSelectionClick(screenPos);
        return;
    }
    flushPendingRightClick();  // any other event ends the "waiting for a menu click" window

    if (pressed) {
        if (m_buttonDown)
            return;  // a different button is already down mid-gesture -- ignore the second press
        flushTextBuffer();  // starting a mouse gesture ends any text-typing context
        flushWheelBuffer();
        m_buttonDown = true;
        m_downButton = button;
        m_downPos = screenPos;
        return;
    }

    if (!m_buttonDown || button != m_downButton)
        return;
    m_buttonDown = false;
    m_downButton = Qt::NoButton;

    const QPoint delta = screenPos - m_downPos;
    const bool isDrag = (delta.x() * delta.x() + delta.y() * delta.y()) >
                         (kDragThresholdPx * kDragThresholdPx);
    if (isDrag) {
        emitSimpleAction(this, SetupActionType::Drag, m_downPos, screenPos, QString(), QString());
    } else if (button == Qt::LeftButton) {
        emitSimpleAction(this, SetupActionType::Click, m_downPos, QPoint(), QString(), QString());
    } else {
        // Don't emit yet -- see m_awaitingMenuSelection's comment: give a
        // brief grace period for a following left-click-on-a-menu-item to
        // arrive and convert this into a MenuSelect instead.
        m_awaitingMenuSelection = true;
        m_pendingRightClickPos = m_downPos;
        m_menuGraceTimer->start(kMenuSelectionGraceMs);
    }
}

void InputRecorder::notifyKeyEvent(int qtKey, const QString &printableText, bool pressed)
{
    if (!m_recording)
        return;
    flushPendingRightClick();  // a key event ends any pending menu-selection window

    // Track held-modifier state from the modifier keys' own press/release
    // (raw key events carry no per-event modifier bitmask the way ordinary
    // Qt/X11 key events do, so this class has to reconstruct it itself).
    Qt::KeyboardModifier trackedModifier = Qt::NoModifier;
    switch (qtKey) {
    case Qt::Key_Control: trackedModifier = Qt::ControlModifier; break;
    case Qt::Key_Alt: trackedModifier = Qt::AltModifier; break;
    case Qt::Key_Shift: trackedModifier = Qt::ShiftModifier; break;
    case Qt::Key_Meta: trackedModifier = Qt::MetaModifier; break;
    default: break;
    }
    if (trackedModifier != Qt::NoModifier) {
        if (pressed)
            m_heldModifiers |= trackedModifier;
        else
            m_heldModifiers &= ~trackedModifier;
        return;  // the modifier key itself is never a recordable action
    }

    if (!pressed)
        return;  // only act on the press half of a non-modifier key

    if (qtKey == Qt::Key_Escape) {
        // Stop is handled by the platform backend (it's the one that owns
        // teardownHook()); this just makes sure any text typed right up to
        // the Escape press is still captured rather than silently dropped
        // -- see InputRecorder.h's class comment on why Escape flushes
        // first. The backend calls finishRecording(true) itself right
        // after this, once teardownHook() has run.
        flushTextBuffer();
        flushWheelBuffer();
        return;
    }

    // Non-modifier, non-Escape key. Two cases:
    // - A "named" key (Tab/Return/Backspace/Delete/arrows/...) or any key
    //   held with Ctrl/Alt/Meta (a shortcut, e.g. Ctrl+A): flush any
    //   pending plain text first, then emit a KeyPress SetupAction whose
    //   keySequence is exactly the QKeySequence::toString() format this
    //   app already uses/parses everywhere else (ActionParams::
    //   shortcutSequences, SetupActionEditorDialog's own manual entry).
    // - A plain printable character with no such modifiers: append to the
    //   text buffer instead of emitting its own action (see m_textBuffer's
    //   comment).
    const bool isNamedKey = printableText.isEmpty();
    const bool hasShortcutModifier =
        m_heldModifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (isNamedKey || hasShortcutModifier) {
        flushTextBuffer();
        flushWheelBuffer();
        const QKeySequence sequence(qtKey | int(m_heldModifiers));
        const QString text = sequence.toString(QKeySequence::PortableText);
        if (!text.isEmpty())
            emitSimpleAction(this, SetupActionType::KeyPress, QPoint(), QPoint(), QString(), text);
        return;
    }

    m_textBuffer += printableText;
}
