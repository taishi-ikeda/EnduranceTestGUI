#include "ActionParamsEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

ActionParamsEditor::ActionParamsEditor(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *dragRow = new QHBoxLayout;
    dragRow->addWidget(new QLabel(QStringLiteral("ドラッグ距離:")));
    m_dragMinSpin = new QSpinBox(this);
    m_dragMinSpin->setRange(1, 5000);
    m_dragMinSpin->setValue(20);
    m_dragMaxSpin = new QSpinBox(this);
    m_dragMaxSpin->setRange(1, 5000);
    m_dragMaxSpin->setValue(200);
    dragRow->addWidget(m_dragMinSpin);
    dragRow->addWidget(new QLabel(QStringLiteral("〜")));
    dragRow->addWidget(m_dragMaxSpin);
    dragRow->addWidget(new QLabel(QStringLiteral("px")));
    dragRow->addWidget(new QLabel(QStringLiteral("方向:")));
    m_dragDirectionCombo = new QComboBox(this);
    m_dragDirectionCombo->addItem(QStringLiteral("ランダム"), int(DragDirectionMode::Random));
    m_dragDirectionCombo->addItem(QStringLiteral("上"), int(DragDirectionMode::Up));
    m_dragDirectionCombo->addItem(QStringLiteral("下"), int(DragDirectionMode::Down));
    m_dragDirectionCombo->addItem(QStringLiteral("左"), int(DragDirectionMode::Left));
    m_dragDirectionCombo->addItem(QStringLiteral("右"), int(DragDirectionMode::Right));
    dragRow->addWidget(m_dragDirectionCombo);
    dragRow->addStretch();
    layout->addLayout(dragRow);

    auto *keyRow = new QHBoxLayout;
    keyRow->addWidget(new QLabel(QStringLiteral("キー入力の使用文字:")));
    m_allowedKeysEdit = new QLineEdit(QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789"), this);
    keyRow->addWidget(m_allowedKeysEdit, 1);
    layout->addLayout(keyRow);

    auto *namedKeyRow = new QHBoxLayout;
    namedKeyRow->addWidget(new QLabel(QStringLiteral("名前付きキーも候補に含める:")));
    m_keyTabCheck = new QCheckBox(QStringLiteral("Tab"), this);
    m_keyReturnCheck = new QCheckBox(QStringLiteral("Return"), this);
    m_keyEscapeCheck = new QCheckBox(QStringLiteral("Escape"), this);
    m_keyBackspaceCheck = new QCheckBox(QStringLiteral("Backspace"), this);
    m_keyDeleteCheck = new QCheckBox(QStringLiteral("Delete"), this);
    m_keyArrowsCheck = new QCheckBox(QStringLiteral("矢印キー"), this);
    namedKeyRow->addWidget(m_keyTabCheck);
    namedKeyRow->addWidget(m_keyReturnCheck);
    namedKeyRow->addWidget(m_keyEscapeCheck);
    namedKeyRow->addWidget(m_keyBackspaceCheck);
    namedKeyRow->addWidget(m_keyDeleteCheck);
    namedKeyRow->addWidget(m_keyArrowsCheck);
    namedKeyRow->addStretch();
    layout->addLayout(namedKeyRow);

    auto *scrollUpRow = new QHBoxLayout;
    scrollUpRow->addWidget(new QLabel(QStringLiteral("スクロール量(上):")));
    m_scrollUpMinSpin = new QSpinBox(this);
    m_scrollUpMinSpin->setRange(1, 200);
    m_scrollUpMinSpin->setValue(1);
    m_scrollUpMaxSpin = new QSpinBox(this);
    m_scrollUpMaxSpin->setRange(1, 200);
    m_scrollUpMaxSpin->setValue(10);
    scrollUpRow->addWidget(m_scrollUpMinSpin);
    scrollUpRow->addWidget(new QLabel(QStringLiteral("〜")));
    scrollUpRow->addWidget(m_scrollUpMaxSpin);
    scrollUpRow->addStretch();
    layout->addLayout(scrollUpRow);

    auto *scrollDownRow = new QHBoxLayout;
    scrollDownRow->addWidget(new QLabel(QStringLiteral("スクロール量(下):")));
    m_scrollDownMinSpin = new QSpinBox(this);
    m_scrollDownMinSpin->setRange(1, 200);
    m_scrollDownMinSpin->setValue(1);
    m_scrollDownMaxSpin = new QSpinBox(this);
    m_scrollDownMaxSpin->setRange(1, 200);
    m_scrollDownMaxSpin->setValue(10);
    scrollDownRow->addWidget(m_scrollDownMinSpin);
    scrollDownRow->addWidget(new QLabel(QStringLiteral("〜")));
    scrollDownRow->addWidget(m_scrollDownMaxSpin);
    scrollDownRow->addStretch();
    layout->addLayout(scrollDownRow);

    auto *scrollHorizontalRow = new QHBoxLayout;
    scrollHorizontalRow->addWidget(new QLabel(QStringLiteral("スクロール量(横。左右はランダム):")));
    m_scrollHorizontalMinSpin = new QSpinBox(this);
    m_scrollHorizontalMinSpin->setRange(1, 200);
    m_scrollHorizontalMinSpin->setValue(1);
    m_scrollHorizontalMaxSpin = new QSpinBox(this);
    m_scrollHorizontalMaxSpin->setRange(1, 200);
    m_scrollHorizontalMaxSpin->setValue(10);
    scrollHorizontalRow->addWidget(m_scrollHorizontalMinSpin);
    scrollHorizontalRow->addWidget(new QLabel(QStringLiteral("〜")));
    scrollHorizontalRow->addWidget(m_scrollHorizontalMaxSpin);
    scrollHorizontalRow->addStretch();
    layout->addLayout(scrollHorizontalRow);

    layout->addWidget(new QLabel(
        QStringLiteral("ショートカットキー一覧（例: Ctrl+C, Ctrl+Shift+Z — macOSではCtrlはCmdとして送信されます）:"),
        this));
    m_shortcutListWidget = new QListWidget(this);
    m_shortcutListWidget->setMaximumHeight(80);
    layout->addWidget(m_shortcutListWidget);
    auto *shortcutRow = new QHBoxLayout;
    m_newShortcutEdit = new QLineEdit(this);
    m_newShortcutEdit->setPlaceholderText(QStringLiteral("Ctrl+C"));
    m_addShortcutButton = new QPushButton(QStringLiteral("追加"), this);
    m_removeShortcutButton = new QPushButton(QStringLiteral("選択を削除"), this);
    shortcutRow->addWidget(m_newShortcutEdit, 1);
    shortcutRow->addWidget(m_addShortcutButton);
    shortcutRow->addWidget(m_removeShortcutButton);
    layout->addLayout(shortcutRow);
    connect(m_addShortcutButton, &QPushButton::clicked, this, &ActionParamsEditor::onAddShortcut);
    connect(m_removeShortcutButton, &QPushButton::clicked, this,
            &ActionParamsEditor::onRemoveSelectedShortcut);

    auto *windowOpRow = new QHBoxLayout;
    windowOpRow->addWidget(new QLabel(QStringLiteral("ウィンドウ操作で許可する種類:")));
    m_windowOpMoveCheck = new QCheckBox(QStringLiteral("移動"), this);
    m_windowOpResizeCheck = new QCheckBox(QStringLiteral("リサイズ"), this);
    m_windowOpMinimizeCheck = new QCheckBox(QStringLiteral("最小化"), this);
    m_windowOpMaximizeCheck = new QCheckBox(QStringLiteral("最大化"), this);
    windowOpRow->addWidget(m_windowOpMoveCheck);
    windowOpRow->addWidget(m_windowOpResizeCheck);
    windowOpRow->addWidget(m_windowOpMinimizeCheck);
    windowOpRow->addWidget(m_windowOpMaximizeCheck);
    windowOpRow->addStretch();
    layout->addLayout(windowOpRow);

    m_contextMenuCheck = new QCheckBox(
        QStringLiteral("右クリック後にメニュー項目を選択する（実験的機能。macOS/Linuxのアクセシビリティ機能に依存）"),
        this);
    layout->addWidget(m_contextMenuCheck);

    auto *contextMenuModeRow = new QHBoxLayout;
    m_contextMenuByNameRadio = new QRadioButton(QStringLiteral("項目名で指定"), this);
    m_contextMenuByIndexRadio = new QRadioButton(QStringLiteral("上から何番目かで指定"), this);
    m_contextMenuByNameRadio->setChecked(true);
    contextMenuModeRow->addWidget(m_contextMenuByNameRadio);
    contextMenuModeRow->addWidget(m_contextMenuByIndexRadio);
    contextMenuModeRow->addStretch();
    layout->addLayout(contextMenuModeRow);
    connect(m_contextMenuByNameRadio, &QRadioButton::toggled, this,
            &ActionParamsEditor::onContextMenuModeChanged);

    layout->addWidget(new QLabel(
        QStringLiteral("候補項目名（開いたメニューにあるものの中からランダムに1つ選択。無ければメニューを閉じる):"),
        this));
    m_contextMenuListWidget = new QListWidget(this);
    m_contextMenuListWidget->setMaximumHeight(80);
    layout->addWidget(m_contextMenuListWidget);
    auto *contextMenuRow = new QHBoxLayout;
    m_newContextMenuItemEdit = new QLineEdit(this);
    m_newContextMenuItemEdit->setPlaceholderText(QStringLiteral("コピー"));
    m_addContextMenuItemButton = new QPushButton(QStringLiteral("追加"), this);
    m_removeContextMenuItemButton = new QPushButton(QStringLiteral("選択を削除"), this);
    contextMenuRow->addWidget(m_newContextMenuItemEdit, 1);
    contextMenuRow->addWidget(m_addContextMenuItemButton);
    contextMenuRow->addWidget(m_removeContextMenuItemButton);
    layout->addLayout(contextMenuRow);

    layout->addWidget(new QLabel(
        QStringLiteral(
            "候補の番号（上から何番目か、1始まり）。開いたメニューの項目数に収まるものの中からランダムに1つ選択。"
            "無ければメニューを閉じる。※メニューの項目数や並びが状況によって変わる場合、意図しない項目を"
            "選んでしまう可能性があるため注意（項目名指定の方が安全）:"),
        this));
    m_contextMenuIndexListWidget = new QListWidget(this);
    m_contextMenuIndexListWidget->setMaximumHeight(80);
    layout->addWidget(m_contextMenuIndexListWidget);
    auto *contextMenuIndexRow = new QHBoxLayout;
    m_newContextMenuIndexSpin = new QSpinBox(this);
    m_newContextMenuIndexSpin->setRange(1, 999);
    m_newContextMenuIndexSpin->setValue(1);
    m_addContextMenuIndexButton = new QPushButton(QStringLiteral("追加"), this);
    m_removeContextMenuIndexButton = new QPushButton(QStringLiteral("選択を削除"), this);
    contextMenuIndexRow->addWidget(m_newContextMenuIndexSpin);
    contextMenuIndexRow->addWidget(m_addContextMenuIndexButton);
    contextMenuIndexRow->addWidget(m_removeContextMenuIndexButton);
    contextMenuIndexRow->addStretch();
    layout->addLayout(contextMenuIndexRow);

    connect(m_addContextMenuItemButton, &QPushButton::clicked, this,
            &ActionParamsEditor::onAddContextMenuItem);
    connect(m_removeContextMenuItemButton, &QPushButton::clicked, this,
            &ActionParamsEditor::onRemoveSelectedContextMenuItem);
    connect(m_addContextMenuIndexButton, &QPushButton::clicked, this,
            &ActionParamsEditor::onAddContextMenuIndex);
    connect(m_removeContextMenuIndexButton, &QPushButton::clicked, this,
            &ActionParamsEditor::onRemoveSelectedContextMenuIndex);

    layout->addStretch();

    onContextMenuModeChanged();
}

void ActionParamsEditor::setParams(const ActionParams &p)
{
    m_dragMinSpin->setValue(p.dragMinDistance);
    m_dragMaxSpin->setValue(p.dragMaxDistance);
    const int dirIndex = m_dragDirectionCombo->findData(int(p.dragDirection));
    m_dragDirectionCombo->setCurrentIndex(dirIndex >= 0 ? dirIndex : 0);

    m_allowedKeysEdit->setText(p.allowedKeyChars);
    m_keyTabCheck->setChecked(p.keyIncludeTab);
    m_keyReturnCheck->setChecked(p.keyIncludeReturn);
    m_keyEscapeCheck->setChecked(p.keyIncludeEscape);
    m_keyBackspaceCheck->setChecked(p.keyIncludeBackspace);
    m_keyDeleteCheck->setChecked(p.keyIncludeDelete);
    m_keyArrowsCheck->setChecked(p.keyIncludeArrowKeys);

    m_scrollUpMinSpin->setValue(p.scrollUpMinAmount);
    m_scrollUpMaxSpin->setValue(p.scrollUpMaxAmount);
    m_scrollDownMinSpin->setValue(p.scrollDownMinAmount);
    m_scrollDownMaxSpin->setValue(p.scrollDownMaxAmount);
    m_scrollHorizontalMinSpin->setValue(p.scrollHorizontalMinAmount);
    m_scrollHorizontalMaxSpin->setValue(p.scrollHorizontalMaxAmount);

    m_windowOpMoveCheck->setChecked(p.windowOpMove);
    m_windowOpResizeCheck->setChecked(p.windowOpResize);
    m_windowOpMinimizeCheck->setChecked(p.windowOpMinimize);
    m_windowOpMaximizeCheck->setChecked(p.windowOpMaximize);

    m_shortcuts = p.shortcutSequences;
    refreshShortcutList();

    m_contextMenuCheck->setChecked(p.enableContextMenuSelection);
    if (p.contextMenuSelectionMode == ContextMenuSelectionMode::ByName)
        m_contextMenuByNameRadio->setChecked(true);
    else
        m_contextMenuByIndexRadio->setChecked(true);
    m_contextMenuItems = p.contextMenuItemNames;
    refreshContextMenuList();
    m_contextMenuIndices = p.contextMenuIndices;
    refreshContextMenuIndexList();
    onContextMenuModeChanged();
}

ActionParams ActionParamsEditor::params() const
{
    ActionParams p;
    p.dragMinDistance = m_dragMinSpin->value();
    p.dragMaxDistance = m_dragMaxSpin->value();
    p.dragDirection = DragDirectionMode(m_dragDirectionCombo->currentData().toInt());
    p.allowedKeyChars = m_allowedKeysEdit->text();
    p.keyIncludeTab = m_keyTabCheck->isChecked();
    p.keyIncludeReturn = m_keyReturnCheck->isChecked();
    p.keyIncludeEscape = m_keyEscapeCheck->isChecked();
    p.keyIncludeBackspace = m_keyBackspaceCheck->isChecked();
    p.keyIncludeDelete = m_keyDeleteCheck->isChecked();
    p.keyIncludeArrowKeys = m_keyArrowsCheck->isChecked();
    p.scrollUpMinAmount = m_scrollUpMinSpin->value();
    p.scrollUpMaxAmount = m_scrollUpMaxSpin->value();
    p.scrollDownMinAmount = m_scrollDownMinSpin->value();
    p.scrollDownMaxAmount = m_scrollDownMaxSpin->value();
    p.scrollHorizontalMinAmount = m_scrollHorizontalMinSpin->value();
    p.scrollHorizontalMaxAmount = m_scrollHorizontalMaxSpin->value();
    p.windowOpMove = m_windowOpMoveCheck->isChecked();
    p.windowOpResize = m_windowOpResizeCheck->isChecked();
    p.windowOpMinimize = m_windowOpMinimizeCheck->isChecked();
    p.windowOpMaximize = m_windowOpMaximizeCheck->isChecked();
    p.shortcutSequences = m_shortcuts;
    p.enableContextMenuSelection = m_contextMenuCheck->isChecked();
    p.contextMenuSelectionMode = m_contextMenuByNameRadio->isChecked()
                                      ? ContextMenuSelectionMode::ByName
                                      : ContextMenuSelectionMode::ByIndex;
    p.contextMenuItemNames = m_contextMenuItems;
    p.contextMenuIndices = m_contextMenuIndices;
    return p;
}

void ActionParamsEditor::onAddShortcut()
{
    const QString text = m_newShortcutEdit->text().trimmed();
    if (text.isEmpty())
        return;
    m_shortcuts.append(text);
    m_newShortcutEdit->clear();
    refreshShortcutList();
}

void ActionParamsEditor::onRemoveSelectedShortcut()
{
    const int row = m_shortcutListWidget->currentRow();
    if (row >= 0 && row < m_shortcuts.size()) {
        m_shortcuts.removeAt(row);
        refreshShortcutList();
    }
}

void ActionParamsEditor::refreshShortcutList()
{
    m_shortcutListWidget->clear();
    for (const QString &s : m_shortcuts)
        m_shortcutListWidget->addItem(s);
}

void ActionParamsEditor::onAddContextMenuItem()
{
    const QString text = m_newContextMenuItemEdit->text().trimmed();
    if (text.isEmpty())
        return;
    m_contextMenuItems.append(text);
    m_newContextMenuItemEdit->clear();
    refreshContextMenuList();
}

void ActionParamsEditor::onRemoveSelectedContextMenuItem()
{
    const int row = m_contextMenuListWidget->currentRow();
    if (row >= 0 && row < m_contextMenuItems.size()) {
        m_contextMenuItems.removeAt(row);
        refreshContextMenuList();
    }
}

void ActionParamsEditor::refreshContextMenuList()
{
    m_contextMenuListWidget->clear();
    for (const QString &s : m_contextMenuItems)
        m_contextMenuListWidget->addItem(s);
}

void ActionParamsEditor::onAddContextMenuIndex()
{
    const int value = m_newContextMenuIndexSpin->value();
    if (!m_contextMenuIndices.contains(value))
        m_contextMenuIndices.append(value);
    refreshContextMenuIndexList();
}

void ActionParamsEditor::onRemoveSelectedContextMenuIndex()
{
    const int row = m_contextMenuIndexListWidget->currentRow();
    if (row >= 0 && row < m_contextMenuIndices.size()) {
        m_contextMenuIndices.removeAt(row);
        refreshContextMenuIndexList();
    }
}

void ActionParamsEditor::refreshContextMenuIndexList()
{
    m_contextMenuIndexListWidget->clear();
    for (int idx : m_contextMenuIndices)
        m_contextMenuIndexListWidget->addItem(QStringLiteral("上から %1 番目").arg(idx));
}

void ActionParamsEditor::onContextMenuModeChanged()
{
    const bool byName = m_contextMenuByNameRadio->isChecked();
    m_contextMenuListWidget->setEnabled(byName);
    m_newContextMenuItemEdit->setEnabled(byName);
    m_addContextMenuItemButton->setEnabled(byName);
    m_removeContextMenuItemButton->setEnabled(byName);
    m_contextMenuIndexListWidget->setEnabled(!byName);
    m_newContextMenuIndexSpin->setEnabled(!byName);
    m_addContextMenuIndexButton->setEnabled(!byName);
    m_removeContextMenuIndexButton->setEnabled(!byName);
}
