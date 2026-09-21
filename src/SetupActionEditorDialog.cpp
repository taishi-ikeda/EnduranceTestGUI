#include "SetupActionEditorDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "I18n.h"
#include "PointPickerOverlay.h"

namespace
{
// Stack page indices -- Click/DoubleClick/RightClick share a page (all
// three just need a single point).
constexpr int kPagePoint = 0;
constexpr int kPageDrag = 1;
constexpr int kPageTypeText = 2;
constexpr int kPageKeyPress = 3;
constexpr int kPageWait = 4;

int pageForType(SetupActionType type)
{
    switch (type) {
        case SetupActionType::Click:
        case SetupActionType::DoubleClick:
        case SetupActionType::RightClick:
            return kPagePoint;
        case SetupActionType::Drag:
            return kPageDrag;
        case SetupActionType::TypeText:
            return kPageTypeText;
        case SetupActionType::KeyPress:
            return kPageKeyPress;
        case SetupActionType::Wait:
            return kPageWait;
    }
    return kPagePoint;
}

QString pointText(const QPoint &p)
{
    return QStringLiteral("(%1, %2)").arg(p.x()).arg(p.y());
}
}  // namespace

SetupActionEditorDialog::SetupActionEditorDialog(const SetupAction &initial, const QPoint &targetTopLeft,
                                                   bool hasTarget, QWidget *parent)
    : QDialog(parent),
      m_point(initial.point),
      m_dragToPoint(initial.dragToPoint),
      m_targetTopLeft(targetTopLeft),
      m_hasTarget(hasTarget)
{
    setWindowTitle(I18n::t(QStringLiteral("起動時セットアップ操作の設定")));

    auto *layout = new QVBoxLayout(this);

    auto *typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel(I18n::t(QStringLiteral("種類:")), this));
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(I18n::t(QStringLiteral("クリック")), QVariant(int(SetupActionType::Click)));
    m_typeCombo->addItem(I18n::t(QStringLiteral("ダブルクリック")), QVariant(int(SetupActionType::DoubleClick)));
    m_typeCombo->addItem(I18n::t(QStringLiteral("右クリック")), QVariant(int(SetupActionType::RightClick)));
    m_typeCombo->addItem(I18n::t(QStringLiteral("ドラッグ")), QVariant(int(SetupActionType::Drag)));
    m_typeCombo->addItem(I18n::t(QStringLiteral("文字入力")), QVariant(int(SetupActionType::TypeText)));
    m_typeCombo->addItem(I18n::t(QStringLiteral("キー入力")), QVariant(int(SetupActionType::KeyPress)));
    m_typeCombo->addItem(I18n::t(QStringLiteral("待機")), QVariant(int(SetupActionType::Wait)));
    typeRow->addWidget(m_typeCombo, 1);
    layout->addLayout(typeRow);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildPointPage());   // kPagePoint
    m_stack->addWidget(buildDragPage());    // kPageDrag
    m_stack->addWidget(buildTypeTextPage()); // kPageTypeText
    m_stack->addWidget(buildKeyPressPage()); // kPageKeyPress
    m_stack->addWidget(buildWaitPage());    // kPageWait
    layout->addWidget(m_stack);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SetupActionEditorDialog::onTypeChanged);

    auto *labelRow = new QHBoxLayout;
    labelRow->addWidget(new QLabel(I18n::t(QStringLiteral("説明（任意）:")), this));
    m_labelEdit = new QLineEdit(initial.label, this);
    m_labelEdit->setPlaceholderText(I18n::t(QStringLiteral("例: ユーザー名欄")));
    labelRow->addWidget(m_labelEdit, 1);
    layout->addLayout(labelRow);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SetupActionEditorDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    m_typeTextEdit->setText(initial.text);
    m_keySequenceEdit->setText(initial.keySequence);
    m_waitMsSpin->setValue(initial.waitMs);

    const int initialComboIndex = m_typeCombo->findData(QVariant(int(initial.type)));
    m_typeCombo->setCurrentIndex(initialComboIndex >= 0 ? initialComboIndex : 0);
    m_stack->setCurrentIndex(pageForType(initial.type));
    refreshPointLabels();

    resize(420, 260);
}

QWidget *SetupActionEditorDialog::buildPointPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *hint = new QLabel(I18n::t(QStringLiteral("対象ウィンドウを基準にした位置をクリックで指定してください。")), page);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *row = new QHBoxLayout;
    m_pickPointButton = new QPushButton(I18n::t(QStringLiteral("位置を選択...")), page);
    m_pointValueLabel = new QLabel(page);
    row->addWidget(m_pickPointButton);
    row->addWidget(m_pointValueLabel, 1);
    layout->addLayout(row);
    layout->addStretch(1);
    connect(m_pickPointButton, &QPushButton::clicked, this, &SetupActionEditorDialog::onPickPoint);
    return page;
}

QWidget *SetupActionEditorDialog::buildDragPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *hint =
        new QLabel(I18n::t(QStringLiteral("対象ウィンドウを基準にしたドラッグの開始位置と終了位置をそれぞれ指定してください。")), page);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *fromRow = new QHBoxLayout;
    m_pickDragFromButton = new QPushButton(I18n::t(QStringLiteral("開始位置を選択...")), page);
    m_dragFromValueLabel = new QLabel(page);
    fromRow->addWidget(m_pickDragFromButton);
    fromRow->addWidget(m_dragFromValueLabel, 1);
    layout->addLayout(fromRow);

    auto *toRow = new QHBoxLayout;
    m_pickDragToButton = new QPushButton(I18n::t(QStringLiteral("終了位置を選択...")), page);
    m_dragToValueLabel = new QLabel(page);
    toRow->addWidget(m_pickDragToButton);
    toRow->addWidget(m_dragToValueLabel, 1);
    layout->addLayout(toRow);

    layout->addStretch(1);
    connect(m_pickDragFromButton, &QPushButton::clicked, this, &SetupActionEditorDialog::onPickDragFromPoint);
    connect(m_pickDragToButton, &QPushButton::clicked, this, &SetupActionEditorDialog::onPickDragToPoint);
    return page;
}

QWidget *SetupActionEditorDialog::buildTypeTextPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(I18n::t(QStringLiteral("入力するテキスト:")), page));
    m_typeTextEdit = new QLineEdit(page);
    layout->addWidget(m_typeTextEdit);
    layout->addStretch(1);
    return page;
}

QWidget *SetupActionEditorDialog::buildKeyPressPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(I18n::t(QStringLiteral("キー（例: Return, Tab, Ctrl+A）:")), page));
    m_keySequenceEdit = new QLineEdit(page);
    m_keySequenceEdit->setPlaceholderText(I18n::t(QStringLiteral("例: Return")));
    layout->addWidget(m_keySequenceEdit);
    layout->addStretch(1);
    return page;
}

QWidget *SetupActionEditorDialog::buildWaitPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(I18n::t(QStringLiteral("待機時間（ミリ秒）:")), page));
    m_waitMsSpin = new QSpinBox(page);
    m_waitMsSpin->setRange(0, 600000);
    m_waitMsSpin->setSingleStep(100);
    layout->addWidget(m_waitMsSpin);
    layout->addStretch(1);
    return page;
}

void SetupActionEditorDialog::onTypeChanged(int index)
{
    const SetupActionType type = static_cast<SetupActionType>(m_typeCombo->itemData(index).toInt());
    m_stack->setCurrentIndex(pageForType(type));
}

void SetupActionEditorDialog::onPickPoint()
{
    if (!m_hasTarget) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("対象ウィンドウ未選択")),
                              I18n::t(QStringLiteral("対象ウィンドウを選択してから位置を指定してください。")));
        return;
    }
    QPoint picked;
    if (PointPickerOverlay::run(picked)) {
        m_point = picked - m_targetTopLeft;
        refreshPointLabels();
    }
}

void SetupActionEditorDialog::onPickDragFromPoint()
{
    if (!m_hasTarget) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("対象ウィンドウ未選択")),
                              I18n::t(QStringLiteral("対象ウィンドウを選択してから位置を指定してください。")));
        return;
    }
    QPoint picked;
    if (PointPickerOverlay::run(picked)) {
        m_point = picked - m_targetTopLeft;
        refreshPointLabels();
    }
}

void SetupActionEditorDialog::onPickDragToPoint()
{
    if (!m_hasTarget) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("対象ウィンドウ未選択")),
                              I18n::t(QStringLiteral("対象ウィンドウを選択してから位置を指定してください。")));
        return;
    }
    QPoint picked;
    if (PointPickerOverlay::run(picked)) {
        m_dragToPoint = picked - m_targetTopLeft;
        refreshPointLabels();
    }
}

void SetupActionEditorDialog::refreshPointLabels()
{
    m_pointValueLabel->setText(pointText(m_point));
    m_dragFromValueLabel->setText(pointText(m_point));
    m_dragToValueLabel->setText(pointText(m_dragToPoint));
}

void SetupActionEditorDialog::onAccept()
{
    const SetupActionType type =
        static_cast<SetupActionType>(m_typeCombo->itemData(m_typeCombo->currentIndex()).toInt());
    if (type == SetupActionType::TypeText && m_typeTextEdit->text().isEmpty()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                              I18n::t(QStringLiteral("入力するテキストを入力してください。")));
        return;
    }
    if (type == SetupActionType::KeyPress && m_keySequenceEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                              I18n::t(QStringLiteral("キーを入力してください。")));
        return;
    }
    accept();
}

SetupAction SetupActionEditorDialog::result() const
{
    SetupAction action;
    action.type = static_cast<SetupActionType>(m_typeCombo->itemData(m_typeCombo->currentIndex()).toInt());
    action.point = m_point;
    action.dragToPoint = m_dragToPoint;
    action.text = m_typeTextEdit->text();
    action.keySequence = m_keySequenceEdit->text().trimmed();
    action.waitMs = m_waitMsSpin->value();
    action.label = m_labelEdit->text().trimmed();
    return action;
}
