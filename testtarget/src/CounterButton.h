#pragma once

#include <QPushButton>

// A QPushButton that also reports right-clicks and double-clicks (plain
// QPushButton only emits clicked() for a left single click), so the test
// target app can show separate left/right/double click counts per button
// -- used to verify EnduranceTestGUI's doubleClickPercent setting actually
// produces double-clicks and not just two rapid single clicks.
class CounterButton : public QPushButton
{
    Q_OBJECT

public:
    using QPushButton::QPushButton;

signals:
    void rightClicked();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};
