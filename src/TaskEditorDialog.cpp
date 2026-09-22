#include "TaskEditorDialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "ActionKindEditor.h"
#include "ActionParamsEditor.h"
#include "StepEditorDialog.h"
#include "I18n.h"

namespace
{
const ActionParams &effectiveParamsOf(const RegionStep &member, const ActionParams &defaults)
{
    return member.useDefaultActionParams ? defaults : member.customActionParams;
}
}  // namespace

TaskEditorDialog::TaskEditorDialog(const RegionStep &initialTask, const QList<NamedRegion> &namedRegions,
                                    const ActionParams &defaultActionParams,
                                    const RegionStep &defaultActionKindsTemplate, QWidget *parent)
    : QDialog(parent),
      m_namedRegions(namedRegions),
      m_defaultActionParams(defaultActionParams),
      m_defaultActionKindsTemplate(defaultActionKindsTemplate),
      m_members(initialTask.isTask ? initialTask.taskMembers : QList<RegionStep>())
{
    setWindowTitle(I18n::t(QStringLiteral("タスクの編集")));
    resize(720, 560);

    auto *layout = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel(
        I18n::t(QStringLiteral("タスクの操作（一覧の順番通りに、毎回すべて1回ずつ実行されます。\n"
                        "タスク全体で1回分の操作としてカウントされます）:")),
        this));
    topRow->addStretch();
    layout->addLayout(topRow);

    auto *mainRow = new QHBoxLayout;

    auto *leftCol = new QVBoxLayout;
    leftCol->addWidget(new QLabel(I18n::t(QStringLiteral("操作（この順番で実行されます）:")), this));
    m_memberListWidget = new QListWidget(this);
    leftCol->addWidget(m_memberListWidget, 1);
    auto *memberButtonsRow1 = new QHBoxLayout;
    m_addMemberButton = new QPushButton(I18n::t(QStringLiteral("追加...")), this);
    m_editMemberButton = new QPushButton(I18n::t(QStringLiteral("編集...")), this);
    m_removeMemberButton = new QPushButton(I18n::t(QStringLiteral("削除")), this);
    memberButtonsRow1->addWidget(m_addMemberButton);
    memberButtonsRow1->addWidget(m_editMemberButton);
    memberButtonsRow1->addWidget(m_removeMemberButton);
    leftCol->addLayout(memberButtonsRow1);
    auto *memberButtonsRow2 = new QHBoxLayout;
    m_moveMemberUpButton = new QPushButton(I18n::t(QStringLiteral("↑ 上へ")), this);
    m_moveMemberDownButton = new QPushButton(I18n::t(QStringLiteral("↓ 下へ")), this);
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
    m_detailContextLabel = new QLabel(I18n::t(QStringLiteral("操作未選択")), scrollContent);
    m_detailContextLabel->setWordWrap(true);
    rightCol->addWidget(m_detailContextLabel);

    m_kindEditor = new ActionKindEditor(scrollContent);
    // A task always runs each member exactly once per pass -- there's no
    // per-member "操作回数" concept (unlike a group, which has no count of
    // its own either, but for the different reason that ITS total call
    // count governs everything; a task simply has no repeat concept at
    // all), so hide that row entirely.
    m_kindEditor->setActionCountRowVisible(false);
    rightCol->addWidget(m_kindEditor);

    auto *modeRow = new QHBoxLayout;
    m_useDefaultParamsRadio = new QRadioButton(I18n::t(QStringLiteral("デフォルトを使う")), scrollContent);
    m_useCustomParamsRadio = new QRadioButton(I18n::t(QStringLiteral("このステップ専用の設定を使う")), scrollContent);
    modeRow->addWidget(m_useDefaultParamsRadio);
    modeRow->addWidget(m_useCustomParamsRadio);
    modeRow->addStretch();
    rightCol->addLayout(modeRow);
    auto *defaultsNoteLabel = new QLabel(
        I18n::t(QStringLiteral("※「デフォルトを使う」場合の値は参照のみです。変更するには①対象選択の"
                        "「デフォルト」ボタンを使ってください。")),
        scrollContent);
    defaultsNoteLabel->setWordWrap(true);
    rightCol->addWidget(defaultsNoteLabel);

    m_paramsEditor = new ActionParamsEditor(scrollContent);
    rightCol->addWidget(m_paramsEditor, 1);

    mainRow->addWidget(scroll, 2);
    layout->addLayout(mainRow, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &TaskEditorDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    connect(m_memberListWidget, &QListWidget::currentRowChanged, this,
            &TaskEditorDialog::onMemberSelectionChanged);
    connect(m_addMemberButton, &QPushButton::clicked, this, &TaskEditorDialog::onAddMember);
    connect(m_editMemberButton, &QPushButton::clicked, this, &TaskEditorDialog::onEditSelectedMember);
    connect(m_removeMemberButton, &QPushButton::clicked, this,
            &TaskEditorDialog::onRemoveSelectedMember);
    connect(m_moveMemberUpButton, &QPushButton::clicked, this, &TaskEditorDialog::onMoveMemberUp);
    connect(m_moveMemberDownButton, &QPushButton::clicked, this, &TaskEditorDialog::onMoveMemberDown);
    connect(m_kindEditor, &ActionKindEditor::changed, this, &TaskEditorDialog::flushMemberEditor);
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

QString TaskEditorDialog::describeMember(const RegionStep &member, int index) const
{
    QStringList actions;
    if (member.enableClick)
        actions << I18n::t(QStringLiteral("クリック"));
    if (member.enableDoubleClick)
        actions << I18n::t(QStringLiteral("ダブルクリック"));
    if (member.enableDrag)
        actions << I18n::t(QStringLiteral("ドラッグ"));
    if (member.enableKey)
        actions << I18n::t(QStringLiteral("キー入力"));
    if (member.enableScrollUp)
        actions << I18n::t(QStringLiteral("スクロール(上)"));
    if (member.enableScrollDown)
        actions << I18n::t(QStringLiteral("スクロール(下)"));
    if (member.enableScrollHorizontal)
        actions << I18n::t(QStringLiteral("スクロール(横)"));
    if (member.enableShortcut)
        actions << I18n::t(QStringLiteral("ショートカット"));
    if (member.enableWindowOp)
        actions << I18n::t(QStringLiteral("ウィンドウ操作"));

    const QString regionDesc = member.useWholeWindow
                                    ? I18n::t(QStringLiteral("対象GUIの全領域"))
                                    : (member.regionName.isEmpty()
                                           ? I18n::t(QStringLiteral("(未選択)"))
                                           : I18n::t(QStringLiteral("操作領域「%1」")).arg(member.regionName));

    return I18n::t(QStringLiteral("%1: %2 | 操作: %3"))
        .arg(index + 1)
        .arg(regionDesc)
        .arg(actions.isEmpty() ? I18n::t(QStringLiteral("(なし)")) : actions.join(QStringLiteral(", ")));
}

void TaskEditorDialog::refreshMemberList()
{
    m_memberListWidget->clear();
    for (int i = 0; i < m_members.size(); ++i)
        m_memberListWidget->addItem(describeMember(m_members[i], i));
}

void TaskEditorDialog::flushMemberEditor()
{
    if (m_lastEditedMemberRow < 0 || m_lastEditedMemberRow >= m_members.size())
        return;

    RegionStep &member = m_members[m_lastEditedMemberRow];
    m_kindEditor->applyKindsTo(member);  // also writes actionCount, which is simply unused for a member
    member.useDefaultActionParams = m_useDefaultParamsRadio->isChecked();
    if (!member.useDefaultActionParams)
        member.customActionParams = m_paramsEditor->params();

    if (auto *item = m_memberListWidget->item(m_lastEditedMemberRow))
        item->setText(describeMember(member, m_lastEditedMemberRow));
}

void TaskEditorDialog::loadMemberEditorForSelection()
{
    const int row = m_memberListWidget->currentRow();
    if (row < 0 || row >= m_members.size()) {
        m_detailContextLabel->setText(I18n::t(QStringLiteral("操作未選択")));
        m_kindEditor->setEnabled(false);
        m_useDefaultParamsRadio->setEnabled(false);
        m_useCustomParamsRadio->setEnabled(false);
        m_paramsEditor->setEnabled(false);
        m_lastEditedMemberRow = -1;
        return;
    }

    // A copy, not a reference -- same reentrancy concern as
    // StepGroupEditorDialog::loadMemberEditorForSelection() (ActionKindEditor::
    // setKinds() fires `changed` per widget as it programmatically updates
    // them, which is connected straight to flushMemberEditor()).
    const RegionStep member = m_members[row];

    m_detailContextLabel->setText(I18n::t(QStringLiteral("操作%1の操作種別・詳細設定を編集中")).arg(row + 1));

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

    // Must be set *before* setKinds() below -- see StepGroupEditorDialog's
    // identical comment on the exact same reentrancy hazard.
    m_lastEditedMemberRow = row;

    m_kindEditor->setEnabled(true);
    m_kindEditor->setKinds(member);

    m_paramsEditor->setEnabled(!member.useDefaultActionParams);
    m_paramsEditor->setParams(effectiveParamsOf(member, m_defaultActionParams));
}

void TaskEditorDialog::onMemberSelectionChanged()
{
    flushMemberEditor();
    loadMemberEditorForSelection();
}

void TaskEditorDialog::onAddMember()
{
    StepEditorDialog dialog(RegionStep(), m_namedRegions, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.useWholeWindow() && dialog.regionName().isEmpty()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("ステップの設定エラー")),
                              I18n::t(QStringLiteral("操作領域が選択されていません。")));
        return;
    }

    flushMemberEditor();
    RegionStep member = m_defaultActionKindsTemplate;  // seed kinds/weights from the default preset
    member.isGroup = false;
    member.groupMembers.clear();
    member.isTask = false;
    member.taskMembers.clear();
    member.useWholeWindow = dialog.useWholeWindow();
    member.regionName = dialog.regionName();
    m_members.append(member);
    refreshMemberList();
    m_memberListWidget->setCurrentRow(m_members.size() - 1);
}

void TaskEditorDialog::onEditSelectedMember()
{
    const int row = m_memberListWidget->currentRow();
    if (row < 0 || row >= m_members.size())
        return;

    StepEditorDialog dialog(m_members[row], m_namedRegions, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.useWholeWindow() && dialog.regionName().isEmpty()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("ステップの設定エラー")),
                              I18n::t(QStringLiteral("操作領域が選択されていません。")));
        return;
    }

    m_members[row].useWholeWindow = dialog.useWholeWindow();
    m_members[row].regionName = dialog.regionName();
    refreshMemberList();
    m_memberListWidget->setCurrentRow(row);
}

void TaskEditorDialog::onRemoveSelectedMember()
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

void TaskEditorDialog::onMoveMemberUp()
{
    const int row = m_memberListWidget->currentRow();
    if (row > 0 && row < m_members.size()) {
        flushMemberEditor();
        // See onRemoveSelectedMember()/StepGroupEditorDialog's identical
        // comment: invalidate before the list is rebuilt underneath the
        // reentrant selection-changed handler.
        m_lastEditedMemberRow = -1;
        m_members.move(row, row - 1);
        refreshMemberList();
        m_memberListWidget->setCurrentRow(row - 1);
    }
}

void TaskEditorDialog::onMoveMemberDown()
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

void TaskEditorDialog::onAccept()
{
    flushMemberEditor();
    if (m_members.isEmpty()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                              I18n::t(QStringLiteral("タスクに操作を最低1つ追加してください。")));
        return;
    }
    accept();
}

RegionStep TaskEditorDialog::result() const
{
    RegionStep task;
    task.isTask = true;
    task.taskMembers = m_members;
    return task;
}
