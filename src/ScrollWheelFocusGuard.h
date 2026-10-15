#pragma once

#include <QObject>

// SPEC.md 不具合報告E7「ホイールの死角」: a QSpinBox/QComboBox/list or table
// view inside a QScrollArea captures the mouse wheel for its own value-
// change/item-scroll behavior even when it doesn't have keyboard focus, so
// scrolling the mouse wheel down a long form (e.g. ①対象選択, packed with
// spin boxes and combo boxes with no room to spare at the default window
// size) silently stops scrolling the *page* the moment the cursor passes
// over one of these widgets -- misleadingly reading as "this is the bottom
// of the panel" when further content (タイミング・制限、負荷注入 etc.) is
// still below, just unreachable by wheel from that cursor position.
//
// Installed once, application-wide (see main.cpp), this guards any
// QAbstractSpinBox/QComboBox/QAbstractItemView that doesn't currently have
// keyboard focus from consuming the mouse wheel. Two things are needed,
// both handled in the .cpp:
//   1. QAbstractSpinBox/QComboBox set focusPolicy() to Qt::WheelFocus by
//      default, which makes Qt auto-focus the widget the instant a wheel
//      event arrives over it -- before any event filter runs. Left alone,
//      that defeats a simple "is this focused?" check (the widget always
//      reads as freshly focused "because of" the very event being
//      filtered). This strips that focus-policy bit so only a real click
//      or Tab grants focus.
//   2. With that fixed, an unfocused widget's wheel event is ignored and
//      manually re-sent up the parentWidget() chain (QCoreApplication::
//      sendEvent(), widget by widget) until some ancestor (typically a
//      QScrollArea) accepts it -- Qt's own "ignored wheel events bubble to
//      the parent" propagation is wired into QWidget's native event
//      dispatch, not into application-level QObject::eventFilter, so it
//      has to be replicated here rather than relied upon.
// Clicking (or Tab-ing) into the widget first still lets the wheel change
// its value/selection as usual.
class ScrollWheelFocusGuard : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};
