#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QVector>

class QLabel;
class QCheckBox;
class QSlider;
class QLineEdit;
class QPlainTextEdit;
class QScrollArea;
class QMenu;
class QPushButton;
class QSpinBox;
class CounterButton;
class SimpleViewerWidget;

// Minimal target application for exercising EnduranceTestGUI against:
// a grid of buttons, a checkbox, a slider, a text field, vertical and
// horizontal scroll areas, a real context menu, a custom multi-button
// dialog, and window-state tracking, each with visible counters so you can
// confirm random clicks, double clicks, drags, key/shortcut input, scroll
// events, context-menu selection, dialog-button presses, and window
// move/resize/minimize/maximize are actually arriving. Includes one
// deliberately hazardous "Quit" button (with a confirmation dialog) meant
// to be carved out with an exclude/mask region while testing, and several
// visually distinct group boxes meant to be drawn as separate named
// operation regions (SPEC.md 6.2/6.3).
class TestTargetWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TestTargetWindow(QWidget *parent = nullptr);

private slots:
    void onButtonLeftClicked(int index);
    void onButtonRightClicked(int index);
    void onButtonDoubleClicked(int index);
    void onCheckBoxToggled(bool checked);
    void onSliderValueChanged(int value);
    void onScrollValueChanged(int value);
    void onHScrollValueChanged(int value);
    void onContextMenuRequested(const QPoint &pos);
    void onContextMenuItemTriggered(const QString &itemName);
    void onOpenCustomDialog();
    void onCustomDialogButtonClicked(const QString &label);
    void onResetCounters();
    void onQuitButtonClicked();
    void updateStatsLabel();
    void onMultiFieldLineEditChanged(int index, const QString &text);
    void onMultiFieldSpinChanged(int value);
    void onMultiFieldPlainTextChanged();

private:
    void appendLog(const QString &message);
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeEvent(QEvent *event) override;
    void updateGeometryLabel();
    void updateMultiFieldStatsLabel();

    QVector<CounterButton *> m_buttons;
    QVector<int> m_leftCounts;
    QVector<int> m_rightCounts;
    QVector<int> m_doubleCounts;

    QCheckBox *m_checkBox = nullptr;
    int m_checkToggleCount = 0;

    QSlider *m_slider = nullptr;
    QLabel *m_sliderLabel = nullptr;
    int m_sliderChangeCount = 0;

    QLineEdit *m_keyInputEdit = nullptr;
    int m_keyPressCount = 0;
    int m_shortcutCount = 0;

    QScrollArea *m_scrollArea = nullptr;
    int m_scrollEventCount = 0;

    QScrollArea *m_hScrollArea = nullptr;
    int m_hScrollEventCount = 0;

    // Real native context menu (right-click) target -- used to verify
    // EnduranceTestGUI's enableContextMenuSelection / contextMenuItemNames
    // / contextMenuIndices actually pick the right item, not just dismiss
    // the menu.
    QPushButton *m_contextMenuTarget = nullptr;
    QLabel *m_contextMenuLabel = nullptr;
    int m_contextMenuOpenCount = 0;
    QVector<QString> m_contextMenuItemLabels;
    QVector<int> m_contextMenuItemCounts;

    // Custom-named dialog buttons (SPEC.md 6.2 v0.70) -- see the
    // constructor's comment on why this exists alongside the quit button's
    // plain Yes/No confirmation dialog.
    QPushButton *m_openCustomDialogButton = nullptr;
    QLabel *m_customDialogLabel = nullptr;
    int m_customDialogOpenCount = 0;
    QVector<QString> m_customDialogButtonLabels;
    QVector<int> m_customDialogButtonCounts;

    QLabel *m_geometryLabel = nullptr;
    int m_minimizeCount = 0;
    int m_maximizeCount = 0;
    bool m_wasMinimized = false;
    bool m_wasMaximized = false;

    // SPEC.md 追加実装依頼「ファジングテスト: 領域内の全入力項目」:
    // several differently-typed editable widgets grouped into one
    // QGroupBox, so a single drawn operation region can cover all of them
    // at once -- for verifying ActionKind::FuzzAllFieldsInRegion actually
    // reaches every kind of field (plain text fields, a spin box's
    // embedded entry, a multi-line text area), not just whichever single
    // widget happened to have focus already. Indices 0-2 are
    // m_multiFieldEdits, 3 is the spin box, 4 is the plain text edit.
    QVector<QLineEdit *> m_multiFieldEdits;
    QSpinBox *m_multiFieldSpin = nullptr;
    QPlainTextEdit *m_multiFieldTextEdit = nullptr;
    QVector<bool> m_multiFieldTouched;  // size 5, see the indices above
    int m_multiFieldChangeCount = 0;
    QLabel *m_multiFieldStatsLabel = nullptr;

    // SPEC.md 追加実装依頼「ビューワー耐久テスト強化」: a minimal 3D-viewer
    // stand-in (wireframe cube responding to VTK-style camera bindings),
    // for verifying middle-drag/modifier-drag/HoldDrag/viewer-mode
    // (viewerRightButtonNeverOpensMenu) features end-to-end.
    SimpleViewerWidget *m_viewerWidget = nullptr;

    QLabel *m_statsLabel = nullptr;
    QPlainTextEdit *m_logView = nullptr;

    int m_totalLeftClicks = 0;
    int m_totalRightClicks = 0;
    int m_totalDoubleClicks = 0;
    QElapsedTimer m_uptime;
};
