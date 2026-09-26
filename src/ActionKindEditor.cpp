#include "ActionKindEditor.h"
#include "I18n.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

ActionKindEditor::ActionKindEditor(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *weightHeaderRow = new QHBoxLayout;
    weightHeaderRow->addWidget(new QLabel(QString(), this), 1);
    weightHeaderRow->addWidget(new QLabel(I18n::t(QStringLiteral("重み")), this));
    layout->addLayout(weightHeaderRow);

    auto *clickRow = new QHBoxLayout;
    m_clickCheck = new QCheckBox(I18n::t(QStringLiteral("クリック")), this);
    m_leftClickCheck = new QCheckBox(I18n::t(QStringLiteral("左")), this);
    m_rightClickCheck = new QCheckBox(I18n::t(QStringLiteral("右")), this);
    clickRow->addWidget(m_clickCheck);
    clickRow->addWidget(m_leftClickCheck);
    clickRow->addWidget(m_rightClickCheck);
    clickRow->addStretch();
    m_clickWeightSpin = new QSpinBox(this);
    m_clickWeightSpin->setRange(1, 100);
    clickRow->addWidget(m_clickWeightSpin);
    layout->addLayout(clickRow);

    auto *doubleClickRow = new QHBoxLayout;
    m_doubleClickCheck = new QCheckBox(I18n::t(QStringLiteral("ダブルクリック（左のみ）")), this);
    doubleClickRow->addWidget(m_doubleClickCheck);
    doubleClickRow->addStretch();
    m_doubleClickWeightSpin = new QSpinBox(this);
    m_doubleClickWeightSpin->setRange(1, 100);
    doubleClickRow->addWidget(m_doubleClickWeightSpin);
    layout->addLayout(doubleClickRow);

    auto *dragRow = new QHBoxLayout;
    m_dragCheck = new QCheckBox(I18n::t(QStringLiteral("ドラッグ")), this);
    dragRow->addWidget(m_dragCheck);
    dragRow->addStretch();
    m_dragWeightSpin = new QSpinBox(this);
    m_dragWeightSpin->setRange(1, 100);
    dragRow->addWidget(m_dragWeightSpin);
    layout->addLayout(dragRow);

    auto *keyRow = new QHBoxLayout;
    m_keyCheck = new QCheckBox(I18n::t(QStringLiteral("キー入力")), this);
    keyRow->addWidget(m_keyCheck);
    keyRow->addStretch();
    m_keyWeightSpin = new QSpinBox(this);
    m_keyWeightSpin->setRange(1, 100);
    keyRow->addWidget(m_keyWeightSpin);
    layout->addLayout(keyRow);

    auto *scrollUpRow = new QHBoxLayout;
    m_scrollUpCheck = new QCheckBox(I18n::t(QStringLiteral("スクロール（上）")), this);
    scrollUpRow->addWidget(m_scrollUpCheck);
    scrollUpRow->addStretch();
    m_scrollUpWeightSpin = new QSpinBox(this);
    m_scrollUpWeightSpin->setRange(1, 100);
    scrollUpRow->addWidget(m_scrollUpWeightSpin);
    layout->addLayout(scrollUpRow);

    auto *scrollDownRow = new QHBoxLayout;
    m_scrollDownCheck = new QCheckBox(I18n::t(QStringLiteral("スクロール（下）")), this);
    scrollDownRow->addWidget(m_scrollDownCheck);
    scrollDownRow->addStretch();
    m_scrollDownWeightSpin = new QSpinBox(this);
    m_scrollDownWeightSpin->setRange(1, 100);
    scrollDownRow->addWidget(m_scrollDownWeightSpin);
    layout->addLayout(scrollDownRow);

    auto *scrollHorizontalRow = new QHBoxLayout;
    m_scrollHorizontalCheck = new QCheckBox(I18n::t(QStringLiteral("スクロール（横）")), this);
    scrollHorizontalRow->addWidget(m_scrollHorizontalCheck);
    scrollHorizontalRow->addStretch();
    m_scrollHorizontalWeightSpin = new QSpinBox(this);
    m_scrollHorizontalWeightSpin->setRange(1, 100);
    scrollHorizontalRow->addWidget(m_scrollHorizontalWeightSpin);
    layout->addLayout(scrollHorizontalRow);

    auto *shortcutRow = new QHBoxLayout;
    m_shortcutCheck = new QCheckBox(I18n::t(QStringLiteral("ショートカットキー")), this);
    shortcutRow->addWidget(m_shortcutCheck);
    shortcutRow->addStretch();
    m_shortcutWeightSpin = new QSpinBox(this);
    m_shortcutWeightSpin->setRange(1, 100);
    shortcutRow->addWidget(m_shortcutWeightSpin);
    layout->addLayout(shortcutRow);

    auto *windowOpRow = new QHBoxLayout;
    m_windowOpCheck = new QCheckBox(I18n::t(QStringLiteral("ウィンドウ操作\n（移動/リサイズ/最小化/最大化）")), this);
    windowOpRow->addWidget(m_windowOpCheck);
    windowOpRow->addStretch();
    m_windowOpWeightSpin = new QSpinBox(this);
    m_windowOpWeightSpin->setRange(1, 100);
    windowOpRow->addWidget(m_windowOpWeightSpin);
    layout->addLayout(windowOpRow);

    auto *dialogButtonPressRow = new QHBoxLayout;
    m_dialogButtonPressCheck = new QCheckBox(I18n::t(QStringLiteral("ダイアログのボタンを押す")), this);
    m_dialogButtonPressCheck->setToolTip(
        I18n::t(QStringLiteral("このステップ（タスク内のメンバー）が「新しく出現したダイアログ（自動検出）」"
                                "を操作対象にしている場合のみ有効です。操作の詳細設定の「ダイアログの"
                                "ボタン名」で指定した名前のボタンを探して押します。")));
    dialogButtonPressRow->addWidget(m_dialogButtonPressCheck);
    dialogButtonPressRow->addStretch();
    m_dialogButtonPressWeightSpin = new QSpinBox(this);
    m_dialogButtonPressWeightSpin->setRange(1, 100);
    dialogButtonPressRow->addWidget(m_dialogButtonPressWeightSpin);
    layout->addLayout(dialogButtonPressRow);

    m_countRowWidget = new QWidget(this);
    auto *countRow = new QHBoxLayout(m_countRowWidget);
    countRow->setContentsMargins(0, 0, 0, 0);
    countRow->addWidget(new QLabel(I18n::t(QStringLiteral("操作回数:")), m_countRowWidget));
    m_actionCountSpin = new QSpinBox(m_countRowWidget);
    m_actionCountSpin->setRange(1, 1000000);
    countRow->addWidget(m_actionCountSpin);
    countRow->addStretch();
    layout->addWidget(m_countRowWidget);

    const QList<QCheckBox *> checks = {m_clickCheck,       m_leftClickCheck,   m_rightClickCheck,
                                        m_doubleClickCheck, m_dragCheck,        m_keyCheck,
                                        m_scrollUpCheck,    m_scrollDownCheck,  m_scrollHorizontalCheck,
                                        m_shortcutCheck,    m_windowOpCheck,    m_dialogButtonPressCheck};
    for (QCheckBox *check : checks)
        connect(check, &QCheckBox::toggled, this, &ActionKindEditor::changed);

    const QList<QSpinBox *> spins = {m_clickWeightSpin,       m_doubleClickWeightSpin,
                                      m_dragWeightSpin,        m_keyWeightSpin,
                                      m_scrollUpWeightSpin,    m_scrollDownWeightSpin,
                                      m_scrollHorizontalWeightSpin, m_shortcutWeightSpin,
                                      m_windowOpWeightSpin,    m_dialogButtonPressWeightSpin,
                                      m_actionCountSpin};
    for (QSpinBox *spin : spins)
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionKindEditor::changed);
}

void ActionKindEditor::setKinds(const RegionStep &step)
{
    m_clickCheck->setChecked(step.enableClick);
    m_leftClickCheck->setChecked(step.enableLeftClick);
    m_rightClickCheck->setChecked(step.enableRightClick);
    m_clickWeightSpin->setValue(step.clickWeight);
    m_doubleClickCheck->setChecked(step.enableDoubleClick);
    m_doubleClickWeightSpin->setValue(step.doubleClickWeight);
    m_dragCheck->setChecked(step.enableDrag);
    m_dragWeightSpin->setValue(step.dragWeight);
    m_keyCheck->setChecked(step.enableKey);
    m_keyWeightSpin->setValue(step.keyWeight);
    m_scrollUpCheck->setChecked(step.enableScrollUp);
    m_scrollUpWeightSpin->setValue(step.scrollUpWeight);
    m_scrollDownCheck->setChecked(step.enableScrollDown);
    m_scrollDownWeightSpin->setValue(step.scrollDownWeight);
    m_scrollHorizontalCheck->setChecked(step.enableScrollHorizontal);
    m_scrollHorizontalWeightSpin->setValue(step.scrollHorizontalWeight);
    m_shortcutCheck->setChecked(step.enableShortcut);
    m_shortcutWeightSpin->setValue(step.shortcutWeight);
    m_windowOpCheck->setChecked(step.enableWindowOp);
    m_windowOpWeightSpin->setValue(step.windowOpWeight);
    m_dialogButtonPressCheck->setChecked(step.enableDialogButtonPress);
    m_dialogButtonPressWeightSpin->setValue(step.dialogButtonPressWeight);
    m_actionCountSpin->setValue(int(step.actionCount));
}

void ActionKindEditor::applyKindsTo(RegionStep &step) const
{
    step.enableClick = m_clickCheck->isChecked();
    step.enableLeftClick = m_leftClickCheck->isChecked();
    step.enableRightClick = m_rightClickCheck->isChecked();
    step.clickWeight = m_clickWeightSpin->value();
    step.enableDoubleClick = m_doubleClickCheck->isChecked();
    step.doubleClickWeight = m_doubleClickWeightSpin->value();
    step.enableDrag = m_dragCheck->isChecked();
    step.dragWeight = m_dragWeightSpin->value();
    step.enableKey = m_keyCheck->isChecked();
    step.keyWeight = m_keyWeightSpin->value();
    step.enableScrollUp = m_scrollUpCheck->isChecked();
    step.scrollUpWeight = m_scrollUpWeightSpin->value();
    step.enableScrollDown = m_scrollDownCheck->isChecked();
    step.scrollDownWeight = m_scrollDownWeightSpin->value();
    step.enableScrollHorizontal = m_scrollHorizontalCheck->isChecked();
    step.scrollHorizontalWeight = m_scrollHorizontalWeightSpin->value();
    step.enableShortcut = m_shortcutCheck->isChecked();
    step.shortcutWeight = m_shortcutWeightSpin->value();
    step.enableWindowOp = m_windowOpCheck->isChecked();
    step.windowOpWeight = m_windowOpWeightSpin->value();
    step.enableDialogButtonPress = m_dialogButtonPressCheck->isChecked();
    step.dialogButtonPressWeight = m_dialogButtonPressWeightSpin->value();
    step.actionCount = m_actionCountSpin->value();
}

void ActionKindEditor::setActionCountRowVisible(bool visible)
{
    m_countRowWidget->setVisible(visible);
}
