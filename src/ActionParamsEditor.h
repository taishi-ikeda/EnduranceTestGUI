#pragma once

#include <QWidget>

#include "TestConfig.h"

class QSpinBox;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QRadioButton;
class QListWidget;
class QListWidgetItem;
class QPushButton;

// Editor widget for one ActionParams value: drag distance/direction, key
// input character set, scroll amount, shortcut key list, and (experimental)
// context-menu item selection. Reused for both TestConfig::defaultActionParams
// (edited in the "対象選択" panel) and a single step's customActionParams
// (edited in the "操作パラメータ" panel when that step is set to use its own
// settings) -- see SPEC.md 6.4/6.9.
class ActionParamsEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ActionParamsEditor(QWidget *parent = nullptr);

    void setParams(const ActionParams &params);
    ActionParams params() const;

    // The app-wide fuzz-string corpus (MainWindow's QSettings-backed master
    // list -- see TestConfig::fuzzStringCorpus's own comment), used to
    // populate the "選択した文字列のみを使う" checklist below. Must be
    // called (by whichever dialog owns this editor) before setParams(), or
    // whenever the corpus may have changed (e.g. after the user edits it via
    // "表示" menu while this dialog happens to be open), so the checklist
    // reflects the current corpus; setParams()'s own fuzzStringSelectedEntries
    // are preserved across a later call even for entries no longer present.
    void setAvailableFuzzStrings(const QStringList &corpus);

private slots:
    void onAddShortcut();
    void onRemoveSelectedShortcut();
    void onAddContextMenuItem();
    void onRemoveSelectedContextMenuItem();
    void onAddContextMenuIndex();
    void onRemoveSelectedContextMenuIndex();
    void onContextMenuModeChanged();
    void onAddDialogButton();
    void onRemoveSelectedDialogButton();
    void onFuzzStringUseModeChanged();
    void onFuzzStringSelectionItemChanged(QListWidgetItem *item);

private:
    void refreshShortcutList();
    void refreshContextMenuList();
    void refreshContextMenuIndexList();
    void refreshDialogButtonList();
    void refreshFuzzStringSelectionList();

    QSpinBox *m_dragMinSpin = nullptr;
    QSpinBox *m_dragMaxSpin = nullptr;
    QComboBox *m_dragDirectionCombo = nullptr;

    QLineEdit *m_allowedKeysEdit = nullptr;

    QSpinBox *m_typingBurstDurationMinSpin = nullptr;
    QSpinBox *m_typingBurstDurationMaxSpin = nullptr;
    QSpinBox *m_typingBurstIntervalMinSpin = nullptr;
    QSpinBox *m_typingBurstIntervalMaxSpin = nullptr;

    QComboBox *m_fuzzStringDeliveryModeCombo = nullptr;
    QRadioButton *m_fuzzStringUseAllRadio = nullptr;
    QRadioButton *m_fuzzStringUseSelectedRadio = nullptr;
    QListWidget *m_fuzzStringSelectionListWidget = nullptr;
    QStringList m_fuzzStringCorpus;            // last corpus passed to setAvailableFuzzStrings()
    QStringList m_fuzzStringSelectedEntries;    // which of it (by exact value) are checked

    QSpinBox *m_scrollUpMinSpin = nullptr;
    QSpinBox *m_scrollUpMaxSpin = nullptr;
    QSpinBox *m_scrollDownMinSpin = nullptr;
    QSpinBox *m_scrollDownMaxSpin = nullptr;
    QSpinBox *m_scrollHorizontalMinSpin = nullptr;
    QSpinBox *m_scrollHorizontalMaxSpin = nullptr;

    QCheckBox *m_keyTabCheck = nullptr;
    QCheckBox *m_keyReturnCheck = nullptr;
    QCheckBox *m_keyEscapeCheck = nullptr;
    QCheckBox *m_keyBackspaceCheck = nullptr;
    QCheckBox *m_keyDeleteCheck = nullptr;
    QCheckBox *m_keyArrowsCheck = nullptr;

    QCheckBox *m_windowOpMoveCheck = nullptr;
    QCheckBox *m_windowOpResizeCheck = nullptr;
    QCheckBox *m_windowOpMinimizeCheck = nullptr;
    QCheckBox *m_windowOpMaximizeCheck = nullptr;

    QListWidget *m_shortcutListWidget = nullptr;
    QLineEdit *m_newShortcutEdit = nullptr;
    QPushButton *m_addShortcutButton = nullptr;
    QPushButton *m_removeShortcutButton = nullptr;
    QStringList m_shortcuts;

    QCheckBox *m_contextMenuCheck = nullptr;
    QRadioButton *m_contextMenuByNameRadio = nullptr;
    QRadioButton *m_contextMenuByIndexRadio = nullptr;
    QListWidget *m_contextMenuListWidget = nullptr;
    QLineEdit *m_newContextMenuItemEdit = nullptr;
    QPushButton *m_addContextMenuItemButton = nullptr;
    QPushButton *m_removeContextMenuItemButton = nullptr;
    QStringList m_contextMenuItems;
    QListWidget *m_contextMenuIndexListWidget = nullptr;
    QSpinBox *m_newContextMenuIndexSpin = nullptr;
    QPushButton *m_addContextMenuIndexButton = nullptr;
    QPushButton *m_removeContextMenuIndexButton = nullptr;
    QList<int> m_contextMenuIndices;

    QListWidget *m_dialogButtonListWidget = nullptr;
    QLineEdit *m_newDialogButtonEdit = nullptr;
    QPushButton *m_addDialogButtonButton = nullptr;
    QPushButton *m_removeDialogButtonButton = nullptr;
    QStringList m_dialogButtonNames;
};
