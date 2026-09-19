#include "CounterButton.h"

#include <QMouseEvent>

void CounterButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        emit rightClicked();
        return;
    }
    QPushButton::mousePressEvent(event);
}

void CounterButton::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked();
        return;
    }
    QPushButton::mouseDoubleClickEvent(event);
}
