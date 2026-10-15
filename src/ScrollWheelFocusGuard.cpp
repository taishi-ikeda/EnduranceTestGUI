#include "ScrollWheelFocusGuard.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QWheelEvent>
#include <QWidget>

bool ScrollWheelFocusGuard::eventFilter(QObject *watched, QEvent *event)
{
    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget)
        return false;

    // QAbstractItemView covers QListWidget/QTreeWidget/QTableWidget (and
    // plain QListView etc.) in one check; QComboBox's own popup list is a
    // QAbstractItemView too but isn't reachable here since it isn't a
    // descendant of the scroll areas this guards (it's a top-level popup).
    if (!(qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QComboBox *>(widget) ||
          qobject_cast<QAbstractItemView *>(widget)))
        return false;

    // QAbstractSpinBox/QComboBox set focusPolicy() to exactly Qt::WheelFocus
    // by default, which makes Qt auto-focus the widget the instant a wheel
    // event arrives over it -- *before* this filter runs. That defeats the
    // hasFocus() check below (by the time we see the event, Qt has already
    // focused the widget "because of" this very scroll), which is why the
    // first version of this fix measurably did nothing: empirical testing
    // showed the dead zone persisted and the spin box gained a focus
    // rectangle it shouldn't have. Stripping just the wheel-focus bit here
    // (every event, not just Wheel, so it is stripped before the first real
    // wheel event in virtually all cases) keeps click/Tab focus
    // (Qt::StrongFocus) working normally while preventing that auto-grab.
    if (widget->focusPolicy() == Qt::WheelFocus)
        widget->setFocusPolicy(Qt::StrongFocus);

    if (event->type() != QEvent::Wheel)
        return false;

    if (widget->hasFocus())
        return false;

    // Ignoring the event here and returning false (so Qt delivers it to
    // `widget` as usual) does NOT reliably make Qt walk it up to an
    // ancestor QScrollArea itself -- that automatic "ignored wheel events
    // bubble to the parent" behavior is part of QWidget's own wheelEvent()
    // plumbing, and an application-wide event filter consuming the event
    // ahead of delivery sits outside that path. So walk the parent chain
    // ourselves instead, resending a fresh copy of the wheel event to each
    // ancestor in turn (a QScrollArea's own event handling scrolls its
    // scrollbars in response, same as if the cursor really had been over
    // empty space) until one actually accepts it.
    const auto *wheelEvent = static_cast<QWheelEvent *>(event);
    for (QWidget *ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QWheelEvent forwarded(wheelEvent->position(), wheelEvent->globalPosition(), wheelEvent->pixelDelta(),
                               wheelEvent->angleDelta(), wheelEvent->buttons(), wheelEvent->modifiers(),
                               wheelEvent->phase(), wheelEvent->inverted());
#else
        QWheelEvent forwarded(wheelEvent->posF(), wheelEvent->globalPosF(), wheelEvent->pixelDelta(),
                               wheelEvent->angleDelta(), wheelEvent->buttons(), wheelEvent->modifiers(),
                               wheelEvent->phase(), wheelEvent->inverted());
#endif
        QCoreApplication::sendEvent(ancestor, &forwarded);
        if (forwarded.isAccepted())
            break;
    }
    return true;  // swallow the original delivery to `widget` either way
}
