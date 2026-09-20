#include "StepGroupEditorDialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ActionKindEditor.h"
#include "ActionParamsEditor.h"
#include "StepEditorDialog.h"

namespace
{
const ActionParams &effectiveParamsOf(const RegionStep &member, const ActionParams &defaults)
{
    return member.useDefaultActionParams ? defaults : member.customActionParams;
}
}  // namespace

StepGroupEditorDialog::StepGroupEditorDialog(const RegionStep &initialGroup,
                                              const QList<NamedRegion> &namedRegions,
                                              const ActionParams &defaultActionParams,
                                              const RegionStep &defaultActionKindsTemplate,
                                              QWidget *parent)
    : QDialog(parent),
      m_namedRegions(namedRegions),
      m_defaultActionParams(defaultActionParams),
      m_defaultActionKindsTemplate(defaultActionKindsTemplate),
      m_members(initialGroup.isGroup ? initialGroup.groupMembers : QList<RegionStep>())
{
    setWindowTitle(QStringLiteral("グループの編集"));
    resize(720, 560);

    auto *layout = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel(
        QStringLiteral("グループ全体の合計呼び出し回数（毎回ランダムに選ばれたメンバーが\n"
                        "1操作ずつ実行され、この回数に達すると次のステップへ進みます）:"),
        this));
    m_totalCallCountSpin = new QSpinBox(this);
    m_totalCallCountSpin->setRange(1, 100000000);
    m_totalCallCountSpin->setValue(
        initialGroup.isGroup ? int(qMin<qint64>(initialGroup.groupTotalCallCount, 100000000)) : 50);
    topRow->addWidget(m_totalCallCountSpin);
    topRow->addStretch();
    layout->addLayout(topRow);

    auto *mainRow = new QHBoxLayout;

    auto *leftCol = new QVBoxLayout;
    leftCol->addWidget(new QLabel(QStringLiteral("メンバー（各ステップの重みに応じてランダムに選ばれます）:"), this));
    m_memberListWidget = new QListWidget(this);
    leftCol->addWidget(m_memberListWidget, 1);
    auto *memberButtonsRow1 = new QHBoxLayout;
    m_addMemberButton = new QPushButton(QStringLiteral("追加..."), this);
    m_editMemberButton = new QPushButton(QStringLiteral("編集..."), this);
    m_removeMemberButton = new QPushButton(QStringLiteral("削除"), this);
    memberButtonsRow1->addWidget(m_addMemberButton);
    memberButtonsRow1->addWidget(m_editMemberButton);
    memberButtonsRow1->addWidget(m_removeMemberButton);
    leftCol->addLayout(memberButtonsRow1);
    auto *memberButtonsRow2 = new QHBoxLayout;
    m_moveMemberUpButton = new QPushButton(QStringLiteral("↑ 上へ"), this);
    m_moveMemberDownButton = new QPushButton(QStringLiteral("↓ 下へ"), this);
    memberButtonsRow2->addWidget(m_moveMemberUpButton);
    memberButtonsRow2->addWidget(m_moveMemberDownButton);
    leftCol->addLayout(memberButtonsRow2);
    mainRow->addLayout(leftCol, 1);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *scrollContent = new QWidget;
    scroll->setWidget(scrollContent);
    auto *rightCol = new QVBoxLayout(scrollContent);
    m_detailContextLabel = new QLabel(QStringLiteral("メンバー未選択"), scrollContent);
    m_detailContextLabel->setWordWrap(true);
    rightCol->addWidget(m_detailContextLabel);

    auto *weightRow = new QHBoxLayout;
    weightRow->addWidget(
        new QLabel(QStringLiteral("グループ内での重み（大きいほど選ばれやすい）:"), scrollContent));
    m_memberWeightSpin = new QSpinBox(scrollContent);
    m_memberWeightSpin->setRange(1, 100);
    weightRow->addWidget(m_memberWeightSpin);
    weightRow->addStretch();
    rightCol->addLayout(weightRow);

    m_kindEditor = new ActionKindEditor(scrollContent);
    // The group's own total call count (above) governs how many actions
    // happen overall -- a member's individual "操作回数" would be
    // meaningless/unused, so hide that row entirely (see
    // ActionKindEditor::setActionCountRowVisible()).
    m_kindEditor->setActionCountRowVisible(false);
    rightCol->addWidget(m_kindEditor);

    auto *modeRow = new QHBoxLayout;
    m_useDefaultParamsRadio = new QRadioButton(QStringLiteral("デフォルトを使う"), scrollContent);
    m_useCustomParamsRadio = new QRadioButton(QStringLiteral("このステップ専用の設定を使う"), scrollContent);
    modeRow->addWidget(m_useDefaultParamsRadio);
    modeRow->addWidget(m_useCustomParamsRadio);
    modeRow->addStretch();
    rightCol->addLayout(modeRow);
    auto *defaultsNoteLabel = new QLabel(
        QStringLiteral("※「デフォルトを使う」場合の値は参照のみです。変更するには①対象選択の"
                        "「デフォルト」ボタンを使ってください。"),
        scrollContent);
    defaultsNoteLabel->setWordWrap(true);
    rightCol->addWidget(defaultsNoteLabel);

    m_paramsEditor = new ActionParamsEditor(scrollContent);
    rightCol->addWidget(m_paramsEditor, 1);

    mainRow->addWidget(scroll, 2);
    layout->addLayout(mainRow, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &StepGroupEditorDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    connect(m_memberListWidget, &QListWidget::currentRowChanged, this,
            &StepGroupEditorDialog::onMemberSelectionChanged);
    connect(m_addMemberButton, &QPushButton::clicked, this, &StepGroupEditorDialog::onAddMember);
    connect(m_editMemberButton, &QPushButton::clicked, this,
            &StepGroupEditorDialog::onEditSelectedMember);
    connect(m_removeMemberButton, &QPushButton::clicked, this,
            &StepGroupEditorDialog::onRemoveSelectedMember);
    connect(m_moveMemberUpButton, &QPushButton::clicked, this, &StepGroupEditorDialog::onMoveMemberUp);
    connect(m_moveMemberDownButton, &QPushButton::clicked, this,
            &StepGroupEditorDialog::onMoveMemberDown);
    connect(m_kindEditor, &ActionKindEditor::changed, this, &StepGroupEditorDialog::flushMemberEditor);
    connect(m_useDefaultParamsRadio, &QRadioButton::toggled, this, [this](bool useDefault) {
        if (m_lastEditedMemberRow < 0)
            return;
        m_paramsEditor->setEnabled(!useDefault);
        m_paramsEditor->setParams(useDefault ? m_defaultActionParams
                                              : m_members[m_lastEditedMemberRow].customActionParams);
    });

    refreshMemberList();
    loadMemberEditorForSelection();
}

QString StepGroupEditorDialog::describeMember(const RegionStep &member, int index) const
{
    QStringList actions;
    if (member.enableClick)
        actions << QStringLiteral("クリック");
    if (member.enableDoubleClick)
        actions << QStringLiteral("ダブルクリック");
    if (member.enableDrag)
        actions << QStringLiteral("ドラッグ");
    if (member.enableKey)
        actions << QStringLiteral("キー入力");
    if (member.enableScrollUp)
        actions << QStringLiteral("スクロール(上)");
    if (member.enableScrollDown)
        actions << QStringLiteral("スクロール(下)");
    if (member.enableScrollHorizontal)
        actions << QStringLiteral("スクロール(横)");
    if (member.enableShortcut)
        actions << QStringLiteral("ショートカット");
    if (member.enableWindowOp)
        actions << QStringLiteral("ウィンドウ操作");

    const QString regionDesc = member.useWholeWindow
                                    ? QStringLiteral("対象GUIの全領域")
                                    : (member.regionName.isEmpty()
                                           ? QStringLiteral("(未選択)")
                                           : QStringLiteral("操作領域「%1」").arg(member.regionName));

    return QStringLiteral("メンバー%1: %2 | 操作: %3 | 重み: %4")
        .arg(index + 1)
        .arg(regionDesc)
        .arg(actions.isEmpty() ? QStringLiteral("(なし)") : actions.join(QStringLiteral(", ")))
        .arg(member.groupWeight);
}

void StepGroupEditorDialog::refreshMemberList()
{
    m_memberListWidget->clear();
    for (int i = 0; i < m_members.size(); ++i)
        m_memberListWidget->addItem(describeMember(m_members[i], i));
}

void StepGroupEditorDialog::flushMemberEditor()
{
    if (m_lastEditedMemberRow < 0 || m_lastEditedMemberRow >= m_members.size())
        return;

    RegionStep &member = m_members[m_lastEditedMemberRow];
    m_kindEditor->applyKindsTo(member);  // also writes actionCount, which is simply unused for a member
    member.groupWeight = m_memberWeightSpin->value();
    member.useDefaultActionParams = m_useDefaultParamsRadio->isChecked();
    if (!member.useDefaultActionParams)
        member.customActionParams = m_paramsEditor->params();

    if (auto *item = m_memberListWidget->item(m_lastEditedMemberRow))
        item->setText(describeMember(member, m_lastEditedMemberRow));
}

void StepGroupEditorDialog::loadMemberEditorForSelection()
{
    const int row = m_memberListWidget->currentRow();
    if (row < 0 || row >= m_members.size()) {
        m_detailContextLabel->setText(QStringLiteral("メンバー未選択"));
        m_memberWeightSpin->setEnabled(false);
        m_kindEditor->setEnabled(false);
        m_useDefaultParamsRadio->setEnabled(false);
        m_useCustomParamsRadio->setEnabled(false);
        m_paramsEditor->setEnabled(false);
        m_lastEditedMemberRow = -1;
        return;
    }

    // A copy, not a reference -- same reentrancy concern as MainWindow::
    // loadActionParamsEditorForSelection() (ActionKindEditor::setKinds()
    // fires `changed` per widget as it programmatically updates them,
    // which is connected straight to flushMemberEditor()).
    const RegionStep member = m_members[row];

    m_detailContextLabel->setText(QStringLiteral("メンバー%1の操作種別・詳細設定を編集中").arg(row + 1));
    m_memberWeightSpin->setEnabled(true);
    m_memberWeightSpin->blockSignals(true);
    m_memberWeightSpin->setValue(member.groupWeight);
    m_memberWeightSpin->blockSignals(false);

    m_kindEditor->setEnabled(true);
    m_kindEditor->setKinds(member);

    m_useDefaultParamsRadio->setEnabled(true);
    m_useCustomParamsRadio->setEnabled(true);
    m_useDefaultParamsRadio->blockSignals(true);
    m_useCustomParamsRadio->blockSignals(true);
    if (member.useDefaultActionParams)
        m_useDefaultParamsRadio->setChecked(true);
    else
        m_useCustomParamsRadio->setChecked(true);
    m_useDefaultParamsRadio->blockSignals(false);
    m_useCustomParamsRadio->blockSignals(false);

    m_paramsEditor->setEnabled(!member.useDefaultActionParams);
    m_paramsEditor->setParams(effectiveParamsOf(member, m_defaultActionParams));

    m_lastEditedMemberRow = row;
}

void StepGroupEditorDialog::onMemberSelectionChanged()
{
    flushMemberEditor();
    loadMemberEditorForSelection();
}

void StepGroupEditorDialog::onAddMember()
{
    StepEditorDialog dialog(RegionStep(), m_namedRegions, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.useWholeWindow() && dialog.regionName().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("ステップの設定エラー"),
                              QStringLiteral("操作領域が選択されていません。"));
        return;
    }

    flushMemberEditor();
    RegionStep member = m_defaultActionKindsTemplate;  // seed kinds/weights from the default preset
    member.isGroup = false;
    member.groupMembers.clear();
    member.groupWeight = 1;
    member.useWholeWindow = dialog.useWholeWindow();
    member.regionName = dialog.regionName();
    m_members.append(member);
    refreshMemberList();
    m_memberListWidget->setCurrentRow(m_members.size() - 1);
}

void StepGroupEditorDialog::onEditSelectedMember()
{
    const int row = m_memberListWidget->currentRow();
    if (row < 0 || row >= m_members.size())
        return;

    StepEditorDialog dialog(m_members[row], m_namedRegions, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.useWholeWindow() && dialog.regionName().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("ステップの設定エラー"),
                              QStringLiteral("操作領域が選択されていません。"));
        return;
    }

    m_members[row].useWholeWindow = dialog.useWholeWindow();
    m_members[row].regionName = dialog.regionName();
    refreshMemberList();
    m_memberListWidget->setCurrentRow(row);
}

void StepGroupEditorDialog::onRemoveSelectedMember()
{
    const int row = m_memberListWidget->currentRow();
    if (row < 0 || row >= m_members.size())
        return;
    m_lastEditedMemberRow = -1;  // the row about to be removed is stale; nothing to flush into it
    m_members.removeAt(row);
    refreshMemberList();
    const int newRow = qMin(row, m_members.size() - 1);
    if (newRow >= 0)
        m_memberListWidget->setCurrentRow(newRow);
    else
        loadMemberEditorForSelection();
}

void StepGroupEditorDialog::onMoveMemberUp()
{
    const int row = m_memberListWidget->currentRow();
    if (row > 0 && row < m_members.size()) {
        flushMemberEditor();
        // See onRemoveSelectedMember(): m_members.move() shifts which
        // member lives at `row`, and refreshMemberList()'s clear() re-enters
        // onMemberSelectionChanged() synchronously -- without invalidating
        // this first, its flushMemberEditor() would use the now-stale index
        // and overwrite a *different* member's data with this one's.
        m_lastEditedMemberRow = -1;
        m_members.move(row, row - 1);
        refreshMemberList();
        m_memberListWidget->setCurrentRow(row - 1);
    }
}

void StepGroupEditorDialog::onMoveMemberDown()
{
    const int row = m_memberListWidget->currentRow();
    if (row >= 0 && row < m_members.size() - 1) {
        flushMemberEditor();
        m_lastEditedMemberRow = -1;  // see onMoveMemberUp()
        m_members.move(row, row + 1);
        refreshMemberList();
        m_memberListWidget->setCurrentRow(row + 1);
    }
}

void StepGroupEditorDialog::onAccept()
{
    flushMemberEditor();
    if (m_members.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("入力エラー"),
                              QStringLiteral("グループにステップを最低1つ追加してください。"));
        return;
    }
    accept();
}

RegionStep StepGroupEditorDialog::result() const
{
    RegionStep group;
    group.isGroup = true;
    group.groupMembers = m_members;
    group.groupTotalCallCount = m_totalCallCountSpin->value();
    return group;
}
