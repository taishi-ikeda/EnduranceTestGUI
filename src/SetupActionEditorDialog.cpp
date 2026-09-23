#include "SetupActionEditorDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "I18n.h"
#include "PointHighlightOverlay.h"
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
constexpr int kPageScroll = 5;
constexpr int kPageMenuSelect = 6;

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
        case SetupActionType::Scroll:
            return kPageScroll;
        case SetupActionType::MenuSelect:
            return kPageMenuSelect;
    }
    return kPagePoint;
}

// Scroll direction combo indices/values. PlatformAutomation::scroll()'s own
// sign convention (see Automation_linux.cpp/_mac.mm): positive dy scrolls
// up, negative scrolls down; positive dx maps to X11 wheel button 6 ("left"
// in the standard 6/7 = left/right pairing), negative to button 7 ("right").
enum class ScrollDirection { Up, Down, Left, Right };

void directionAndAmountToDelta(ScrollDirection dir, int amount, int &outDx, int &outDy)
{
    outDx = 0;
    outDy = 0;
    switch (dir) {
    case ScrollDirection::Up: outDy = amount; break;
    case ScrollDirection::Down: outDy = -amount; break;
    case ScrollDirection::Left: outDx = amount; break;
    case ScrollDirection::Right: outDx = -amount; break;
    }
}

ScrollDirection directionFromDelta(int dx, int dy)
{
    if (dx != 0)
        return dx > 0 ? ScrollDirection::Left : ScrollDirection::Right;
    return dy >= 0 ? ScrollDirection::Up : ScrollDirection::Down;
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
      // An action being edited that already has a non-origin point was
      // necessarily picked at some point in the past (a fresh SetupAction
      // from "追加..." always starts at QPoint(0, 0), see TestConfig.h) --
      // treat that as already valid so re-opening an existing entry doesn't
      // spuriously demand re-picking it. A genuinely-(0,0) point (new, or an
      // old entry saved before this validation existed) still requires an
      // explicit pick before OK is accepted (see onAccept()).
      m_pointPicked(!initial.point.isNull()),
      m_dragToPicked(!initial.dragToPoint.isNull()),
      m_targetTopLeft(targetTopLeft),
      m_hasTarget(hasTarget)
{
    setWindowTitle(I18n::t(QStringLiteral("起動時セットアップ操作の設定")));
    // m_highlightOverlay's per-screen windows (shown continuously while
    // this dialog is open, see updateHighlight() below) use
    // Qt::WindowStaysOnTopHint so the marker stays visible above the
    // target app being tested. Without this dialog being in that same
    // "always on top" layer, several window managers (e.g. a bare openbox
    // session) keep re-asserting the highlight windows above this dialog,
    // making the dialog itself impossible to see or interact with --
    // exactly the bug NamedRegionEditorDialog had for the same reason
    // (SPEC.md 6.3/6.13/8).
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

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
    m_typeCombo->addItem(I18n::t(QStringLiteral("ホイールスクロール")), QVariant(int(SetupActionType::Scroll)));
    m_typeCombo->addItem(I18n::t(QStringLiteral("メニュー項目を選択")), QVariant(int(SetupActionType::MenuSelect)));
    typeRow->addWidget(m_typeCombo, 1);
    layout->addLayout(typeRow);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildPointPage());   // kPagePoint
    m_stack->addWidget(buildDragPage());    // kPageDrag
    m_stack->addWidget(buildTypeTextPage()); // kPageTypeText
    m_stack->addWidget(buildKeyPressPage()); // kPageKeyPress
    m_stack->addWidget(buildWaitPage());    // kPageWait
    m_stack->addWidget(buildScrollPage());  // kPageScroll
    m_stack->addWidget(buildMenuSelectPage()); // kPageMenuSelect
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

    const ScrollDirection initialDir = directionFromDelta(initial.scrollDx, initial.scrollDy);
    m_scrollDirectionCombo->setCurrentIndex(int(initialDir));
    const int initialAmount = qMax(qAbs(initial.scrollDx), qAbs(initial.scrollDy));
    m_scrollAmountSpin->setValue(initialAmount > 0 ? initialAmount : 3);  // 3: a sensible default for a fresh action

    if (initial.menuSelectionMode == ContextMenuSelectionMode::ByIndex)
        m_menuByIndexRadio->setChecked(true);
    else
        m_menuByNameRadio->setChecked(true);
    m_menuItemNameEdit->setText(initial.menuItemName);
    m_menuItemIndexSpin->setValue(qMax(1, initial.menuItemIndex));

    const int initialComboIndex = m_typeCombo->findData(QVariant(int(initial.type)));
    m_typeCombo->setCurrentIndex(initialComboIndex >= 0 ? initialComboIndex : 0);
    m_stack->setCurrentIndex(pageForType(initial.type));
    refreshPointLabels();

    // Visualize the currently-relevant picked point(s) on screen for as
    // long as this dialog stays open (SPEC.md 6.13) -- hidden (not
    // destroyed) when the dialog closes, whether accepted, cancelled, or
    // closed any other way. Mirrors NamedRegionEditorDialog's identical
    // pattern for its own RegionHighlightOverlay.
    connect(this, &QDialog::finished, this, [this](int) {
        if (m_highlightOverlay)
            m_highlightOverlay->hide();
    });
    updateHighlight();

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
    auto *hint = new QLabel(I18n::t(QStringLiteral("パスワード等も入力できます。実行ログにはこの内容自体は記録されません。")), page);
    hint->setWordWrap(true);
    layout->addWidget(hint);
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

QWidget *SetupActionEditorDialog::buildScrollPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *hint = new QLabel(I18n::t(QStringLiteral("対象ウィンドウを基準にした位置をクリックで指定してください。")), page);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *pointRow = new QHBoxLayout;
    m_pickScrollPointButton = new QPushButton(I18n::t(QStringLiteral("位置を選択...")), page);
    m_scrollPointValueLabel = new QLabel(page);
    pointRow->addWidget(m_pickScrollPointButton);
    pointRow->addWidget(m_scrollPointValueLabel, 1);
    layout->addLayout(pointRow);

    auto *dirRow = new QHBoxLayout;
    dirRow->addWidget(new QLabel(I18n::t(QStringLiteral("方向:")), page));
    m_scrollDirectionCombo = new QComboBox(page);
    m_scrollDirectionCombo->addItem(I18n::t(QStringLiteral("上")), int(ScrollDirection::Up));
    m_scrollDirectionCombo->addItem(I18n::t(QStringLiteral("下")), int(ScrollDirection::Down));
    m_scrollDirectionCombo->addItem(I18n::t(QStringLiteral("左")), int(ScrollDirection::Left));
    m_scrollDirectionCombo->addItem(I18n::t(QStringLiteral("右")), int(ScrollDirection::Right));
    dirRow->addWidget(m_scrollDirectionCombo, 1);
    layout->addLayout(dirRow);

    auto *amountRow = new QHBoxLayout;
    amountRow->addWidget(new QLabel(I18n::t(QStringLiteral("量（ホイールの「目盛り」数）:")), page));
    m_scrollAmountSpin = new QSpinBox(page);
    m_scrollAmountSpin->setRange(1, 1000);
    amountRow->addWidget(m_scrollAmountSpin, 1);
    layout->addLayout(amountRow);

    layout->addStretch(1);
    connect(m_pickScrollPointButton, &QPushButton::clicked, this, &SetupActionEditorDialog::onPickPoint);
    return page;
}

QWidget *SetupActionEditorDialog::buildMenuSelectPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *hint = new QLabel(
        I18n::t(QStringLiteral("対象ウィンドウを基準にした右クリックの位置を指定してください。開いた"
                        "メニューから、常に同じ1項目を選択します。")),
        page);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *pointRow = new QHBoxLayout;
    m_pickMenuPointButton = new QPushButton(I18n::t(QStringLiteral("位置を選択...")), page);
    m_menuPointValueLabel = new QLabel(page);
    pointRow->addWidget(m_pickMenuPointButton);
    pointRow->addWidget(m_menuPointValueLabel, 1);
    layout->addLayout(pointRow);

    auto *modeRow = new QHBoxLayout;
    m_menuByNameRadio = new QRadioButton(I18n::t(QStringLiteral("項目名で指定:")), page);
    m_menuByIndexRadio = new QRadioButton(I18n::t(QStringLiteral("上から何番目かで指定:")), page);
    modeRow->addWidget(m_menuByNameRadio);
    modeRow->addWidget(m_menuByIndexRadio);
    layout->addLayout(modeRow);

    auto *nameRow = new QHBoxLayout;
    m_menuItemNameEdit = new QLineEdit(page);
    m_menuItemNameEdit->setPlaceholderText(I18n::t(QStringLiteral("例: 削除")));
    nameRow->addWidget(m_menuItemNameEdit);
    layout->addLayout(nameRow);

    auto *indexRow = new QHBoxLayout;
    m_menuItemIndexSpin = new QSpinBox(page);
    m_menuItemIndexSpin->setRange(1, 999);
    indexRow->addWidget(m_menuItemIndexSpin);
    indexRow->addStretch(1);
    layout->addLayout(indexRow);

    layout->addStretch(1);
    connect(m_pickMenuPointButton, &QPushButton::clicked, this, &SetupActionEditorDialog::onPickPoint);
    connect(m_menuByNameRadio, &QRadioButton::toggled, m_menuItemNameEdit, &QWidget::setEnabled);
    connect(m_menuByIndexRadio, &QRadioButton::toggled, m_menuItemIndexSpin, &QWidget::setEnabled);
    return page;
}

void SetupActionEditorDialog::onTypeChanged(int index)
{
    const SetupActionType type = static_cast<SetupActionType>(m_typeCombo->itemData(index).toInt());
    m_stack->setCurrentIndex(pageForType(type));
    updateHighlight();
}

void SetupActionEditorDialog::updateHighlight()
{
    const SetupActionType type =
        static_cast<SetupActionType>(m_typeCombo->itemData(m_typeCombo->currentIndex()).toInt());

    QList<QPoint> points;
    QList<QString> labels;
    if (type == SetupActionType::Drag) {
        if (m_pointPicked) {
            points << (m_point + m_targetTopLeft);
            labels << I18n::t(QStringLiteral("開始"));
        }
        if (m_dragToPicked) {
            points << (m_dragToPoint + m_targetTopLeft);
            labels << I18n::t(QStringLiteral("終了"));
        }
    } else if ((type == SetupActionType::Click || type == SetupActionType::DoubleClick ||
                type == SetupActionType::RightClick || type == SetupActionType::Scroll ||
                type == SetupActionType::MenuSelect) &&
               m_pointPicked) {
        points << (m_point + m_targetTopLeft);
        labels << I18n::t(QStringLiteral("位置"));
    }

    if (points.isEmpty()) {
        if (m_highlightOverlay)
            m_highlightOverlay->hide();
        return;
    }
    if (!m_highlightOverlay)
        m_highlightOverlay = new PointHighlightOverlay(this);
    m_highlightOverlay->showPoints(points, labels);
    // See the constructor's WindowStaysOnTopHint comment: reassert this
    // dialog above the highlight windows every time they're (re)shown, on
    // top of the flag alone, mirroring NamedRegionEditorDialog::
    // updateHighlight().
    raise();
    activateWindow();
}

void SetupActionEditorDialog::pickPointInto(QPoint &target, bool &pickedFlag)
{
    if (!m_hasTarget) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("対象ウィンドウ未選択")),
                              I18n::t(QStringLiteral("対象ウィンドウを選択してから位置を指定してください。")));
        return;
    }
    QPoint picked;
    if (PointPickerOverlay::run(picked)) {
        target = picked - m_targetTopLeft;
        pickedFlag = true;
        refreshPointLabels();
        updateHighlight();
    }
}

void SetupActionEditorDialog::onPickPoint()
{
    pickPointInto(m_point, m_pointPicked);
}

void SetupActionEditorDialog::onPickDragFromPoint()
{
    pickPointInto(m_point, m_pointPicked);
}

void SetupActionEditorDialog::onPickDragToPoint()
{
    pickPointInto(m_dragToPoint, m_dragToPicked);
}

void SetupActionEditorDialog::refreshPointLabels()
{
    m_pointValueLabel->setText(pointText(m_point));
    m_dragFromValueLabel->setText(pointText(m_point));
    m_dragToValueLabel->setText(pointText(m_dragToPoint));
    m_scrollPointValueLabel->setText(pointText(m_point));
    m_menuPointValueLabel->setText(pointText(m_point));
}

void SetupActionEditorDialog::onAccept()
{
    const SetupActionType type =
        static_cast<SetupActionType>(m_typeCombo->itemData(m_typeCombo->currentIndex()).toInt());
    const bool needsPoint = type == SetupActionType::Click || type == SetupActionType::DoubleClick ||
                             type == SetupActionType::RightClick || type == SetupActionType::Drag ||
                             type == SetupActionType::Scroll || type == SetupActionType::MenuSelect;
    if (needsPoint && !m_pointPicked) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                              I18n::t(QStringLiteral("「位置を選択...」から位置を指定してください。")));
        return;
    }
    if (type == SetupActionType::Drag && !m_dragToPicked) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                              I18n::t(QStringLiteral("「終了位置を選択...」から位置を指定してください。")));
        return;
    }
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
    if (type == SetupActionType::MenuSelect && m_menuByNameRadio->isChecked() &&
        m_menuItemNameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                              I18n::t(QStringLiteral("選択する項目名を入力してください。")));
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
    directionAndAmountToDelta(
        static_cast<ScrollDirection>(m_scrollDirectionCombo->currentData().toInt()),
        m_scrollAmountSpin->value(), action.scrollDx, action.scrollDy);
    action.menuSelectionMode =
        m_menuByIndexRadio->isChecked() ? ContextMenuSelectionMode::ByIndex : ContextMenuSelectionMode::ByName;
    action.menuItemName = m_menuItemNameEdit->text().trimmed();
    action.menuItemIndex = m_menuItemIndexSpin->value();
    action.label = m_labelEdit->text().trimmed();
    return action;
}
