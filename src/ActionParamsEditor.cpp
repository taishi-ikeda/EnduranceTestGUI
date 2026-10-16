#include "ActionParamsEditor.h"
#include "I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

ActionParamsEditor::ActionParamsEditor(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Split across two rows (distance, then direction) rather than one long
    // row, so both stay usable when this panel is narrow (SPEC.md 6.9).
    auto *dragDistanceRow = new QHBoxLayout;
    dragDistanceRow->addWidget(new QLabel(I18n::t(QStringLiteral("ドラッグ距離:"))));
    m_dragMinSpin = new QSpinBox(this);
    m_dragMinSpin->setRange(1, 5000);
    m_dragMinSpin->setValue(20);
    m_dragMaxSpin = new QSpinBox(this);
    m_dragMaxSpin->setRange(1, 5000);
    m_dragMaxSpin->setValue(200);
    dragDistanceRow->addWidget(m_dragMinSpin);
    dragDistanceRow->addWidget(new QLabel(I18n::t(QStringLiteral("〜"))));
    dragDistanceRow->addWidget(m_dragMaxSpin);
    dragDistanceRow->addWidget(new QLabel(QStringLiteral("px")));
    dragDistanceRow->addStretch();
    layout->addLayout(dragDistanceRow);

    auto *dragDirectionRow = new QHBoxLayout;
    dragDirectionRow->addWidget(new QLabel(I18n::t(QStringLiteral("ドラッグ方向:"))));
    m_dragDirectionCombo = new QComboBox(this);
    m_dragDirectionCombo->addItem(I18n::t(QStringLiteral("ランダム")), int(DragDirectionMode::Random));
    m_dragDirectionCombo->addItem(I18n::t(QStringLiteral("上")), int(DragDirectionMode::Up));
    m_dragDirectionCombo->addItem(I18n::t(QStringLiteral("下")), int(DragDirectionMode::Down));
    m_dragDirectionCombo->addItem(I18n::t(QStringLiteral("左")), int(DragDirectionMode::Left));
    m_dragDirectionCombo->addItem(I18n::t(QStringLiteral("右")), int(DragDirectionMode::Right));
    dragDirectionRow->addWidget(m_dragDirectionCombo);
    dragDirectionRow->addStretch();
    layout->addLayout(dragDirectionRow);

    auto *keyRow = new QHBoxLayout;
    keyRow->addWidget(new QLabel(I18n::t(QStringLiteral("キー入力の使用文字:"))));
    m_allowedKeysEdit = new QLineEdit(QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789"), this);
    keyRow->addWidget(m_allowedKeysEdit, 1);
    layout->addLayout(keyRow);

    // Label on its own line, checkboxes split across two rows of three, so
    // this stays usable when the panel is narrow (SPEC.md 6.9).
    auto *namedKeyLabel = new QLabel(I18n::t(QStringLiteral("名前付きキーも候補に含める:")), this);
    namedKeyLabel->setWordWrap(true);
    layout->addWidget(namedKeyLabel);
    m_keyTabCheck = new QCheckBox(QStringLiteral("Tab"), this);
    m_keyReturnCheck = new QCheckBox(QStringLiteral("Return"), this);
    m_keyEscapeCheck = new QCheckBox(QStringLiteral("Escape"), this);
    m_keyBackspaceCheck = new QCheckBox(QStringLiteral("Backspace"), this);
    m_keyDeleteCheck = new QCheckBox(QStringLiteral("Delete"), this);
    m_keyArrowsCheck = new QCheckBox(I18n::t(QStringLiteral("矢印キー")), this);
    auto *namedKeyRow1 = new QHBoxLayout;
    namedKeyRow1->addWidget(m_keyTabCheck);
    namedKeyRow1->addWidget(m_keyReturnCheck);
    namedKeyRow1->addWidget(m_keyEscapeCheck);
    namedKeyRow1->addStretch();
    layout->addLayout(namedKeyRow1);
    auto *namedKeyRow2 = new QHBoxLayout;
    namedKeyRow2->addWidget(m_keyBackspaceCheck);
    namedKeyRow2->addWidget(m_keyDeleteCheck);
    namedKeyRow2->addWidget(m_keyArrowsCheck);
    namedKeyRow2->addStretch();
    layout->addLayout(namedKeyRow2);

    // 一定時間キーボード入力 (ActionKind::TypingBurst): shares the character/
    // named-key settings just above, so only its own time range (total
    // duration) and keystroke interval need their own row here.
    auto *typingBurstLabel =
        new QLabel(I18n::t(QStringLiteral("連続キー入力（一定時間）の時間・間隔（上記の使用文字・名前付き"
                                           "キーを使用）:")),
                    this);
    typingBurstLabel->setWordWrap(true);
    layout->addWidget(typingBurstLabel);
    auto *typingBurstDurationRow = new QHBoxLayout;
    typingBurstDurationRow->addWidget(new QLabel(I18n::t(QStringLiteral("時間:"))));
    m_typingBurstDurationMinSpin = new QSpinBox(this);
    m_typingBurstDurationMinSpin->setRange(1, 600000);
    m_typingBurstDurationMinSpin->setValue(2000);
    m_typingBurstDurationMaxSpin = new QSpinBox(this);
    m_typingBurstDurationMaxSpin->setRange(1, 600000);
    m_typingBurstDurationMaxSpin->setValue(5000);
    typingBurstDurationRow->addWidget(m_typingBurstDurationMinSpin);
    typingBurstDurationRow->addWidget(new QLabel(I18n::t(QStringLiteral("〜"))));
    typingBurstDurationRow->addWidget(m_typingBurstDurationMaxSpin);
    typingBurstDurationRow->addWidget(new QLabel(QStringLiteral("ms")));
    typingBurstDurationRow->addStretch();
    layout->addLayout(typingBurstDurationRow);
    auto *typingBurstIntervalRow = new QHBoxLayout;
    typingBurstIntervalRow->addWidget(new QLabel(I18n::t(QStringLiteral("キー間隔:"))));
    m_typingBurstIntervalMinSpin = new QSpinBox(this);
    m_typingBurstIntervalMinSpin->setRange(1, 60000);
    m_typingBurstIntervalMinSpin->setValue(50);
    m_typingBurstIntervalMaxSpin = new QSpinBox(this);
    m_typingBurstIntervalMaxSpin->setRange(1, 60000);
    m_typingBurstIntervalMaxSpin->setValue(200);
    typingBurstIntervalRow->addWidget(m_typingBurstIntervalMinSpin);
    typingBurstIntervalRow->addWidget(new QLabel(I18n::t(QStringLiteral("〜"))));
    typingBurstIntervalRow->addWidget(m_typingBurstIntervalMaxSpin);
    typingBurstIntervalRow->addWidget(new QLabel(QStringLiteral("ms"), this));
    typingBurstIntervalRow->addStretch();
    layout->addLayout(typingBurstIntervalRow);

    // 異常入力ファジング (ActionKind::FuzzString): how a chosen corpus entry
    // is delivered, then which entries this step may actually choose from.
    auto *fuzzStringDeliveryRow = new QHBoxLayout;
    fuzzStringDeliveryRow->addWidget(new QLabel(I18n::t(QStringLiteral("ファズ文字列の送信方法:"))));
    m_fuzzStringDeliveryModeCombo = new QComboBox(this);
    m_fuzzStringDeliveryModeCombo->addItem(I18n::t(QStringLiteral("ランダム（キー入力/貼り付けを毎回選択）")),
                                            int(FuzzStringDeliveryMode::Random));
    m_fuzzStringDeliveryModeCombo->addItem(I18n::t(QStringLiteral("キー入力（1文字ずつ送信）")),
                                            int(FuzzStringDeliveryMode::KeyByKey));
    m_fuzzStringDeliveryModeCombo->addItem(I18n::t(QStringLiteral("クリップボード貼り付け（Ctrl+V）")),
                                            int(FuzzStringDeliveryMode::Paste));
    fuzzStringDeliveryRow->addWidget(m_fuzzStringDeliveryModeCombo);
    fuzzStringDeliveryRow->addStretch();
    layout->addLayout(fuzzStringDeliveryRow);

    auto *fuzzStringUseModeRow = new QHBoxLayout;
    m_fuzzStringUseAllRadio = new QRadioButton(I18n::t(QStringLiteral("すべてのファズ文字列を使う")), this);
    m_fuzzStringUseSelectedRadio = new QRadioButton(I18n::t(QStringLiteral("選択した文字列のみを使う")), this);
    m_fuzzStringUseAllRadio->setChecked(true);
    fuzzStringUseModeRow->addWidget(m_fuzzStringUseAllRadio);
    fuzzStringUseModeRow->addWidget(m_fuzzStringUseSelectedRadio);
    fuzzStringUseModeRow->addStretch();
    layout->addLayout(fuzzStringUseModeRow);
    connect(m_fuzzStringUseAllRadio, &QRadioButton::toggled, this,
            &ActionParamsEditor::onFuzzStringUseModeChanged);

    auto *fuzzStringListLabel = new QLabel(
        I18n::t(QStringLiteral("候補（チェックした文字列だけが「選択した文字列のみを使う」の対象になります。"
                                "一覧自体は「表示」メニューの「ファズ文字列リストを編集...」で追加・変更できます）:")),
        this);
    fuzzStringListLabel->setWordWrap(true);
    layout->addWidget(fuzzStringListLabel);
    m_fuzzStringSelectionListWidget = new QListWidget(this);
    m_fuzzStringSelectionListWidget->setMaximumHeight(100);
    layout->addWidget(m_fuzzStringSelectionListWidget);
    connect(m_fuzzStringSelectionListWidget, &QListWidget::itemChanged, this,
            &ActionParamsEditor::onFuzzStringSelectionItemChanged);

    auto *scrollUpRow = new QHBoxLayout;
    scrollUpRow->addWidget(new QLabel(I18n::t(QStringLiteral("スクロール量(上):"))));
    m_scrollUpMinSpin = new QSpinBox(this);
    m_scrollUpMinSpin->setRange(1, 200);
    m_scrollUpMinSpin->setValue(1);
    m_scrollUpMaxSpin = new QSpinBox(this);
    m_scrollUpMaxSpin->setRange(1, 200);
    m_scrollUpMaxSpin->setValue(10);
    scrollUpRow->addWidget(m_scrollUpMinSpin);
    scrollUpRow->addWidget(new QLabel(I18n::t(QStringLiteral("〜"))));
    scrollUpRow->addWidget(m_scrollUpMaxSpin);
    scrollUpRow->addStretch();
    layout->addLayout(scrollUpRow);

    auto *scrollDownRow = new QHBoxLayout;
    scrollDownRow->addWidget(new QLabel(I18n::t(QStringLiteral("スクロール量(下):"))));
    m_scrollDownMinSpin = new QSpinBox(this);
    m_scrollDownMinSpin->setRange(1, 200);
    m_scrollDownMinSpin->setValue(1);
    m_scrollDownMaxSpin = new QSpinBox(this);
    m_scrollDownMaxSpin->setRange(1, 200);
    m_scrollDownMaxSpin->setValue(10);
    scrollDownRow->addWidget(m_scrollDownMinSpin);
    scrollDownRow->addWidget(new QLabel(I18n::t(QStringLiteral("〜"))));
    scrollDownRow->addWidget(m_scrollDownMaxSpin);
    scrollDownRow->addStretch();
    layout->addLayout(scrollDownRow);

    auto *scrollHorizontalRow = new QHBoxLayout;
    scrollHorizontalRow->addWidget(new QLabel(I18n::t(QStringLiteral("スクロール量(横。左右はランダム):"))));
    m_scrollHorizontalMinSpin = new QSpinBox(this);
    m_scrollHorizontalMinSpin->setRange(1, 200);
    m_scrollHorizontalMinSpin->setValue(1);
    m_scrollHorizontalMaxSpin = new QSpinBox(this);
    m_scrollHorizontalMaxSpin->setRange(1, 200);
    m_scrollHorizontalMaxSpin->setValue(10);
    scrollHorizontalRow->addWidget(m_scrollHorizontalMinSpin);
    scrollHorizontalRow->addWidget(new QLabel(I18n::t(QStringLiteral("〜"))));
    scrollHorizontalRow->addWidget(m_scrollHorizontalMaxSpin);
    scrollHorizontalRow->addStretch();
    layout->addLayout(scrollHorizontalRow);

    auto *shortcutLabel = new QLabel(
        I18n::t(QStringLiteral("ショートカットキー一覧（例: Ctrl+C, Ctrl+Shift+Z — macOSではCtrlはCmdとして送信されます）:")),
        this);
    shortcutLabel->setWordWrap(true);
    layout->addWidget(shortcutLabel);
    m_shortcutListWidget = new QListWidget(this);
    m_shortcutListWidget->setMaximumHeight(80);
    layout->addWidget(m_shortcutListWidget);
    auto *shortcutRow = new QHBoxLayout;
    m_newShortcutEdit = new QLineEdit(this);
    m_newShortcutEdit->setPlaceholderText(QStringLiteral("Ctrl+C"));
    m_addShortcutButton = new QPushButton(I18n::t(QStringLiteral("追加")), this);
    m_removeShortcutButton = new QPushButton(I18n::t(QStringLiteral("選択を削除")), this);
    shortcutRow->addWidget(m_newShortcutEdit, 1);
    shortcutRow->addWidget(m_addShortcutButton);
    shortcutRow->addWidget(m_removeShortcutButton);
    layout->addLayout(shortcutRow);
    connect(m_addShortcutButton, &QPushButton::clicked, this, &ActionParamsEditor::onAddShortcut);
    connect(m_removeShortcutButton, &QPushButton::clicked, this,
            &ActionParamsEditor::onRemoveSelectedShortcut);

    // Label on its own line (rather than sharing a row with the
    // checkboxes) so it isn't squeezed out when the panel is narrow.
    layout->addWidget(new QLabel(I18n::t(QStringLiteral("ウィンドウ操作で許可する種類:")), this));
    auto *windowOpRow = new QHBoxLayout;
    m_windowOpMoveCheck = new QCheckBox(I18n::t(QStringLiteral("移動")), this);
    m_windowOpResizeCheck = new QCheckBox(I18n::t(QStringLiteral("リサイズ")), this);
    m_windowOpMinimizeCheck = new QCheckBox(I18n::t(QStringLiteral("最小化")), this);
    m_windowOpMaximizeCheck = new QCheckBox(I18n::t(QStringLiteral("最大化")), this);
    windowOpRow->addWidget(m_windowOpMoveCheck);
    windowOpRow->addWidget(m_windowOpResizeCheck);
    windowOpRow->addWidget(m_windowOpMinimizeCheck);
    windowOpRow->addWidget(m_windowOpMaximizeCheck);
    windowOpRow->addStretch();
    layout->addLayout(windowOpRow);

    m_contextMenuCheck = new QCheckBox(
        I18n::t(QStringLiteral("右クリック後にメニュー項目を選択する\n（実験的機能。macOS/Linuxのアクセシビリティ機能に依存）")),
        this);
    layout->addWidget(m_contextMenuCheck);

    auto *contextMenuModeRow = new QHBoxLayout;
    m_contextMenuByNameRadio = new QRadioButton(I18n::t(QStringLiteral("項目名で指定")), this);
    m_contextMenuByIndexRadio = new QRadioButton(I18n::t(QStringLiteral("上から何番目かで指定")), this);
    m_contextMenuByNameRadio->setChecked(true);
    contextMenuModeRow->addWidget(m_contextMenuByNameRadio);
    contextMenuModeRow->addWidget(m_contextMenuByIndexRadio);
    contextMenuModeRow->addStretch();
    layout->addLayout(contextMenuModeRow);
    connect(m_contextMenuByNameRadio, &QRadioButton::toggled, this,
            &ActionParamsEditor::onContextMenuModeChanged);
    connect(m_contextMenuCheck, &QCheckBox::toggled, this, &ActionParamsEditor::onContextMenuModeChanged);

    auto *contextMenuNameLabel = new QLabel(
        I18n::t(QStringLiteral("候補項目名（開いたメニューにあるものの中からランダムに1つ選択。無ければメニューを閉じる):")),
        this);
    contextMenuNameLabel->setWordWrap(true);
    layout->addWidget(contextMenuNameLabel);
    m_contextMenuListWidget = new QListWidget(this);
    m_contextMenuListWidget->setMaximumHeight(80);
    layout->addWidget(m_contextMenuListWidget);
    auto *contextMenuRow = new QHBoxLayout;
    m_newContextMenuItemEdit = new QLineEdit(this);
    m_newContextMenuItemEdit->setPlaceholderText(I18n::t(QStringLiteral("コピー")));
    m_addContextMenuItemButton = new QPushButton(I18n::t(QStringLiteral("追加")), this);
    m_removeContextMenuItemButton = new QPushButton(I18n::t(QStringLiteral("選択を削除")), this);
    contextMenuRow->addWidget(m_newContextMenuItemEdit, 1);
    contextMenuRow->addWidget(m_addContextMenuItemButton);
    contextMenuRow->addWidget(m_removeContextMenuItemButton);
    layout->addLayout(contextMenuRow);

    auto *contextMenuIndexLabel = new QLabel(
        I18n::t(QStringLiteral("候補の番号（上から何番目か、1始まり）。開いたメニューの項目数に収まるものの中からランダムに1つ選択。"
            "無ければメニューを閉じる。※メニューの項目数や並びが状況によって変わる場合、意図しない項目を"
            "選んでしまう可能性があるため注意（項目名指定の方が安全）:")),
        this);
    contextMenuIndexLabel->setWordWrap(true);
    layout->addWidget(contextMenuIndexLabel);
    m_contextMenuIndexListWidget = new QListWidget(this);
    m_contextMenuIndexListWidget->setMaximumHeight(80);
    layout->addWidget(m_contextMenuIndexListWidget);
    auto *contextMenuIndexRow = new QHBoxLayout;
    m_newContextMenuIndexSpin = new QSpinBox(this);
    m_newContextMenuIndexSpin->setRange(1, 999);
    m_newContextMenuIndexSpin->setValue(1);
    m_addContextMenuIndexButton = new QPushButton(I18n::t(QStringLiteral("追加")), this);
    m_removeContextMenuIndexButton = new QPushButton(I18n::t(QStringLiteral("選択を削除")), this);
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

    // SPEC.md 6.2追加実装及び修正依頼: candidates for ②の「ダイアログの
    // ボタンを押す」action kind (only meaningful for a task member that
    // targets a newly-appeared dialog) -- same "list several candidates,
    // pick whichever are actually present" idiom as the context-menu
    // item-name list above.
    auto *dialogButtonLabel = new QLabel(
        I18n::t(QStringLiteral("ダイアログのボタン名（この中で実際に見つかったものからランダムに1つ選んで"
                                "押す。1つも見つからなければこの操作をスキップ):")),
        this);
    dialogButtonLabel->setWordWrap(true);
    layout->addWidget(dialogButtonLabel);
    m_dialogButtonListWidget = new QListWidget(this);
    m_dialogButtonListWidget->setMaximumHeight(80);
    layout->addWidget(m_dialogButtonListWidget);
    auto *dialogButtonRow = new QHBoxLayout;
    m_newDialogButtonEdit = new QLineEdit(this);
    m_newDialogButtonEdit->setPlaceholderText(I18n::t(QStringLiteral("OK")));
    m_addDialogButtonButton = new QPushButton(I18n::t(QStringLiteral("追加")), this);
    m_removeDialogButtonButton = new QPushButton(I18n::t(QStringLiteral("選択を削除")), this);
    dialogButtonRow->addWidget(m_newDialogButtonEdit, 1);
    dialogButtonRow->addWidget(m_addDialogButtonButton);
    dialogButtonRow->addWidget(m_removeDialogButtonButton);
    layout->addLayout(dialogButtonRow);

    connect(m_addDialogButtonButton, &QPushButton::clicked, this,
            &ActionParamsEditor::onAddDialogButton);
    connect(m_removeDialogButtonButton, &QPushButton::clicked, this,
            &ActionParamsEditor::onRemoveSelectedDialogButton);

    layout->addStretch();

    onContextMenuModeChanged();
    onFuzzStringUseModeChanged();
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

    m_typingBurstDurationMinSpin->setValue(p.typingBurstMinDurationMs);
    m_typingBurstDurationMaxSpin->setValue(p.typingBurstMaxDurationMs);
    m_typingBurstIntervalMinSpin->setValue(p.typingBurstKeyIntervalMinMs);
    m_typingBurstIntervalMaxSpin->setValue(p.typingBurstKeyIntervalMaxMs);

    const int deliveryIndex = m_fuzzStringDeliveryModeCombo->findData(int(p.fuzzStringDeliveryMode));
    m_fuzzStringDeliveryModeCombo->setCurrentIndex(deliveryIndex >= 0 ? deliveryIndex : 0);
    if (p.fuzzStringUseAllEntries)
        m_fuzzStringUseAllRadio->setChecked(true);
    else
        m_fuzzStringUseSelectedRadio->setChecked(true);
    m_fuzzStringSelectedEntries = p.fuzzStringSelectedEntries;
    refreshFuzzStringSelectionList();
    onFuzzStringUseModeChanged();

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

    m_dialogButtonNames = p.dialogButtonNames;
    refreshDialogButtonList();
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
    p.typingBurstMinDurationMs = m_typingBurstDurationMinSpin->value();
    p.typingBurstMaxDurationMs = m_typingBurstDurationMaxSpin->value();
    p.typingBurstKeyIntervalMinMs = m_typingBurstIntervalMinSpin->value();
    p.typingBurstKeyIntervalMaxMs = m_typingBurstIntervalMaxSpin->value();
    p.fuzzStringDeliveryMode = FuzzStringDeliveryMode(m_fuzzStringDeliveryModeCombo->currentData().toInt());
    p.fuzzStringUseAllEntries = m_fuzzStringUseAllRadio->isChecked();
    p.fuzzStringSelectedEntries = m_fuzzStringSelectedEntries;
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
    p.dialogButtonNames = m_dialogButtonNames;
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
        m_contextMenuIndexListWidget->addItem(I18n::t(QStringLiteral("上から %1 番目")).arg(idx));
}

void ActionParamsEditor::onContextMenuModeChanged()
{
    // Both the by-name/by-index choice itself and whichever candidate list
    // goes with it are meaningless while the master checkbox is off, so
    // gate everything on it too (not just on which radio is selected).
    const bool enabled = m_contextMenuCheck->isChecked();
    const bool byName = m_contextMenuByNameRadio->isChecked();
    m_contextMenuByNameRadio->setEnabled(enabled);
    m_contextMenuByIndexRadio->setEnabled(enabled);
    m_contextMenuListWidget->setEnabled(enabled && byName);
    m_newContextMenuItemEdit->setEnabled(enabled && byName);
    m_addContextMenuItemButton->setEnabled(enabled && byName);
    m_removeContextMenuItemButton->setEnabled(enabled && byName);
    m_contextMenuIndexListWidget->setEnabled(enabled && !byName);
    m_newContextMenuIndexSpin->setEnabled(enabled && !byName);
    m_addContextMenuIndexButton->setEnabled(enabled && !byName);
    m_removeContextMenuIndexButton->setEnabled(enabled && !byName);
}

void ActionParamsEditor::onAddDialogButton()
{
    const QString text = m_newDialogButtonEdit->text().trimmed();
    if (text.isEmpty())
        return;
    m_dialogButtonNames.append(text);
    m_newDialogButtonEdit->clear();
    refreshDialogButtonList();
}

void ActionParamsEditor::onRemoveSelectedDialogButton()
{
    const int row = m_dialogButtonListWidget->currentRow();
    if (row >= 0 && row < m_dialogButtonNames.size()) {
        m_dialogButtonNames.removeAt(row);
        refreshDialogButtonList();
    }
}

void ActionParamsEditor::refreshDialogButtonList()
{
    m_dialogButtonListWidget->clear();
    for (const QString &s : m_dialogButtonNames)
        m_dialogButtonListWidget->addItem(s);
}

void ActionParamsEditor::setAvailableFuzzStrings(const QStringList &corpus)
{
    m_fuzzStringCorpus = corpus;
    refreshFuzzStringSelectionList();
}

void ActionParamsEditor::refreshFuzzStringSelectionList()
{
    // Rebuilding from scratch fires itemChanged for every row as its check
    // state is (re)established -- onFuzzStringSelectionItemChanged() would
    // otherwise read back a half-built m_fuzzStringSelectedEntries while
    // this runs, so block signals for the rebuild and only reconnect once
    // every row reflects the real selection.
    const QSignalBlocker blocker(m_fuzzStringSelectionListWidget);
    m_fuzzStringSelectionListWidget->clear();
    for (const QString &entry : m_fuzzStringCorpus) {
        // Elide the same way FuzzStringCorpusDialog's own list does, so a
        // very long corpus entry (e.g. the built-in 64KB stress string)
        // doesn't make this row unreasonably wide -- the full text is still
        // what's actually stored in m_fuzzStringSelectedEntries/compared
        // against below, only the displayed label is shortened.
        constexpr int kMaxDisplayChars = 80;
        const QString label = entry.size() <= kMaxDisplayChars
                                   ? entry
                                   : I18n::t(QStringLiteral("%1...(全%2文字)"))
                                         .arg(entry.left(kMaxDisplayChars))
                                         .arg(entry.size());
        auto *item = new QListWidgetItem(label, m_fuzzStringSelectionListWidget);
        item->setData(Qt::UserRole, entry);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_fuzzStringSelectedEntries.contains(entry) ? Qt::Checked : Qt::Unchecked);
    }
}

void ActionParamsEditor::onFuzzStringSelectionItemChanged(QListWidgetItem *item)
{
    const QString entry = item->data(Qt::UserRole).toString();
    const bool checked = item->checkState() == Qt::Checked;
    if (checked && !m_fuzzStringSelectedEntries.contains(entry))
        m_fuzzStringSelectedEntries.append(entry);
    else if (!checked)
        m_fuzzStringSelectedEntries.removeAll(entry);
}

void ActionParamsEditor::onFuzzStringUseModeChanged()
{
    // The checklist only matters once "選択した文字列のみを使う" is picked
    // (SPEC.md 追加実装依頼「異常入力ファジング」) -- greyed out, not
    // hidden, while "すべてのファズ文字列を使う" is selected, so the
    // checked state a user already set up is still visible (just inert)
    // if they switch back and forth.
    m_fuzzStringSelectionListWidget->setEnabled(m_fuzzStringUseSelectedRadio->isChecked());
}
