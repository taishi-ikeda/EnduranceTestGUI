#include "InputRecorder.h"

#include <QKeySequence>

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
}  // namespace

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
    emit actionRecorded(SetupActionType::TypeText, QPoint(), QPoint(), text, QString());
}

void InputRecorder::notifyMouseButton(Qt::MouseButton button, bool pressed, const QPoint &screenPos)
{
    if (!m_recording)
        return;
    if (button != Qt::LeftButton && button != Qt::RightButton)
        return;  // middle/other buttons: no matching SetupActionType, ignore

    if (pressed) {
        if (m_buttonDown)
            return;  // a different button is already down mid-gesture -- ignore the second press
        flushTextBuffer();  // starting a mouse gesture ends any text-typing context
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
        emit actionRecorded(SetupActionType::Drag, m_downPos, screenPos, QString(), QString());
    } else if (button == Qt::LeftButton) {
        emit actionRecorded(SetupActionType::Click, m_downPos, QPoint(), QString(), QString());
    } else {
        emit actionRecorded(SetupActionType::RightClick, m_downPos, QPoint(), QString(), QString());
    }
}

void InputRecorder::notifyKeyEvent(int qtKey, const QString &printableText, bool pressed)
{
    if (!m_recording)
        return;

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
        const QKeySequence sequence(qtKey | int(m_heldModifiers));
        const QString text = sequence.toString(QKeySequence::PortableText);
        if (!text.isEmpty())
            emit actionRecorded(SetupActionType::KeyPress, QPoint(), QPoint(), QString(), text);
        return;
    }

    m_textBuffer += printableText;
}
