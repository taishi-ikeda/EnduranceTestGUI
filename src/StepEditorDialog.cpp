#include "StepEditorDialog.h"
#include "ActionKindEditor.h"
#include "ActionParamsEditor.h"
#include "I18n.h"
#include "NamedRegionEditorDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace
{
// "操作領域1", "操作領域2", ... -- the first of these not already used by an
// existing named region, mirroring the naming scheme the old ①対象選択
// panel's "追加..." button used to generate (see MainWindow::
// generateDefaultRegionName()'s own identical rationale, now folded in here
// since region authoring moved to this dialog).
QString generateDefaultRegionName(const QList<NamedRegion> &existing)
{
    for (int n = 1;; ++n) {
        const QString candidate = I18n::t(QStringLiteral("操作領域%1")).arg(n);
        bool used = false;
        for (const NamedRegion &region : existing) {
            if (region.name == candidate) {
                used = true;
                break;
            }
        }
        if (!used)
            return candidate;
    }
}
}  // namespace

StepEditorDialog::StepEditorDialog(const RegionStep &initial, RegionEditContext *regionCtx, QWidget *parent,
                                    bool allowPopupDialogTarget, bool includeActionParams,
                                    const ActionParams &defaultActionParams)
    : QDialog(parent), m_defaultActionParams(defaultActionParams),
      m_initialCustomActionParams(initial.customActionParams), m_regionCtx(regionCtx),
      m_initialSweepRegionUseRandomPoint(initial.sweepRegionUseRandomPoint)
{
    setWindowTitle(includeActionParams ? I18n::t(QStringLiteral("ステップの設定"))
                                        : I18n::t(QStringLiteral("ステップの操作領域を選択")));

    auto *layout = new QVBoxLayout(this);

    auto *introLabel = new QLabel(
        includeActionParams
            ? I18n::t(QStringLiteral("このステップで操作する領域と、操作の種類・重み・回数、"
                                      "詳細パラメータを設定してください。"))
            : I18n::t(QStringLiteral("このステップで操作する領域を選択してください。\n"
                                      "操作の種類・重み・回数や詳細パラメータは、追加後にメンバー一覧で"
                                      "この項目を選択して設定します。")),
        this);
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);
    m_wholeWindowRadio = new QRadioButton(I18n::t(QStringLiteral("対象GUIの全領域（自動追従）")), this);
    m_namedRegionRadio = new QRadioButton(I18n::t(QStringLiteral("登録済みの操作領域から選択:")), this);
    layout->addWidget(m_wholeWindowRadio);
    auto *namedRegionRow = new QHBoxLayout;
    namedRegionRow->addWidget(m_namedRegionRadio);
    m_namedRegionCombo = new QComboBox(this);
    namedRegionRow->addWidget(m_namedRegionCombo, 1);
    layout->addLayout(namedRegionRow);

    // SPEC.md 追加実装依頼「操作領域の指定をステップ単位のダイアログへ
    // 統合」: region authoring moved here from the now-removed "①対象選択"
    // panel -- these mutate *m_regionCtx->namedRegions directly (the same
    // pool every other step's dialog shares), independently of whether this
    // whole dialog is later accepted or cancelled, exactly like the old
    // panel's own add/edit/delete buttons did.
    auto *namedRegionManageRow = new QHBoxLayout;
    m_createRegionButton = new QPushButton(I18n::t(QStringLiteral("新規作成...")), this);
    m_editRegionButton = new QPushButton(I18n::t(QStringLiteral("編集...")), this);
    m_deleteRegionButton = new QPushButton(I18n::t(QStringLiteral("削除")), this);
    namedRegionManageRow->addWidget(m_createRegionButton);
    namedRegionManageRow->addWidget(m_editRegionButton);
    namedRegionManageRow->addWidget(m_deleteRegionButton);
    namedRegionManageRow->addStretch();
    layout->addLayout(namedRegionManageRow);

    connect(m_wholeWindowRadio, &QRadioButton::toggled, this, &StepEditorDialog::onModeChanged);
    connect(m_namedRegionCombo, &QComboBox::currentTextChanged, this,
            &StepEditorDialog::onRegionSelectionChanged);
    connect(m_createRegionButton, &QPushButton::clicked, this, &StepEditorDialog::onCreateRegion);
    connect(m_editRegionButton, &QPushButton::clicked, this, &StepEditorDialog::onEditRegion);
    connect(m_deleteRegionButton, &QPushButton::clicked, this, &StepEditorDialog::onDeleteRegion);

    if (allowPopupDialogTarget) {
        m_popupDialogRadio = new QRadioButton(
            I18n::t(QStringLiteral("新しく出現したウィンドウ（ダイアログ等）を対象にする（自動検出）")), this);
        layout->addWidget(m_popupDialogRadio);
        auto *popupNoteLabel = new QLabel(
            I18n::t(QStringLiteral("※このタスク内で直前までに実行した操作が開いたダイアログ等、対象アプリの"
                            "メインウィンドウ以外に新しく出現したウィンドウ全体を操作領域にします。"
                            "実行時にそのようなウィンドウが見つからない場合は、見つかるまで待機します。")),
            this);
        popupNoteLabel->setWordWrap(true);
        layout->addWidget(popupNoteLabel);
    }

    if (includeActionParams) {
        auto *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *scrollContent = new QWidget;
        scroll->setWidget(scrollContent);
        auto *scrollLayout = new QVBoxLayout(scrollContent);

        // Only shown while the currently selected operation region is
        // itself a 点列(スイープ) NamedRegion (toggled by
        // onRegionSelectionChanged()) -- an ordinary rectangle region or
        // useWholeWindow has no sweep path to choose a mode for, so this
        // step always gets a plain random point within it, same as before
        // this choice existed.
        m_sweepModeGroup = new QGroupBox(I18n::t(QStringLiteral("この操作領域内での操作位置の選び方")),
                                          scrollContent);
        auto *sweepModeLayout = new QVBoxLayout(m_sweepModeGroup);
        auto *sweepModeRow = new QHBoxLayout;
        m_sweepRandomRadio = new QRadioButton(I18n::t(QStringLiteral("ランダム")), m_sweepModeGroup);
        m_sweepSequenceRadio = new QRadioButton(I18n::t(QStringLiteral("点列スイープ")), m_sweepModeGroup);
        sweepModeRow->addWidget(m_sweepRandomRadio);
        sweepModeRow->addWidget(m_sweepSequenceRadio);
        sweepModeRow->addStretch();
        sweepModeLayout->addLayout(sweepModeRow);
        auto *sweepModeHintLabel = new QLabel(
            I18n::t(QStringLiteral("「ランダム」はこの操作領域に登録された点列の経路上をランダムに操作し、"
                                    "「点列スイープ」は経路上を間隔・ランダム幅の設定に従って順番に操作します"
                                    "（間隔・ランダム幅は「編集...」の操作領域編集画面で設定します）。")),
            m_sweepModeGroup);
        sweepModeHintLabel->setWordWrap(true);
        sweepModeLayout->addWidget(sweepModeHintLabel);
        scrollLayout->addWidget(m_sweepModeGroup);

        auto *kindGroup =
            new QGroupBox(I18n::t(QStringLiteral("このステップの操作種別・重み・回数")), scrollContent);
        auto *kindLayout = new QVBoxLayout(kindGroup);
        m_kindEditor = new ActionKindEditor(kindGroup);
        kindLayout->addWidget(m_kindEditor);
        scrollLayout->addWidget(kindGroup);

        auto *paramsGroup = new QGroupBox(
            I18n::t(QStringLiteral("操作の詳細設定（ドラッグ距離・キー文字種・スクロール量など）")),
            scrollContent);
        auto *paramsLayout = new QVBoxLayout(paramsGroup);
        auto *modeRow = new QHBoxLayout;
        m_useDefaultParamsRadio =
            new QRadioButton(I18n::t(QStringLiteral("デフォルトを使う")), paramsGroup);
        m_useCustomParamsRadio =
            new QRadioButton(I18n::t(QStringLiteral("このステップ専用の設定を使う")), paramsGroup);
        modeRow->addWidget(m_useDefaultParamsRadio);
        modeRow->addWidget(m_useCustomParamsRadio);
        modeRow->addStretch();
        paramsLayout->addLayout(modeRow);
        auto *defaultsNoteLabel = new QLabel(
            I18n::t(QStringLiteral("※「デフォルトを使う」場合の値は参照のみです。変更するには②の"
                                    "「デフォルト」ボタンを使ってください。")),
            paramsGroup);
        defaultsNoteLabel->setWordWrap(true);
        paramsLayout->addWidget(defaultsNoteLabel);
        m_paramsEditor = new ActionParamsEditor(paramsGroup);
        paramsLayout->addWidget(m_paramsEditor, 1);
        scrollLayout->addWidget(paramsGroup, 1);

        layout->addWidget(scroll, 1);
        connect(m_useDefaultParamsRadio, &QRadioButton::toggled, this,
                &StepEditorDialog::onActionParamsModeChanged);
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    refreshRegionCombo();

    // Populate from `initial`.
    const bool hasRegions = m_regionCtx && m_regionCtx->namedRegions && !m_regionCtx->namedRegions->isEmpty();
    if (m_popupDialogRadio && initial.targetsPopupDialog) {
        m_popupDialogRadio->setChecked(true);
    } else if (initial.useWholeWindow || !hasRegions) {
        m_wholeWindowRadio->setChecked(true);
    } else {
        m_namedRegionRadio->setChecked(true);
        const int idx = m_namedRegionCombo->findText(initial.regionName);
        m_namedRegionCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    if (includeActionParams) {
        m_kindEditor->setKinds(initial);
        if (initial.useDefaultActionParams)
            m_useDefaultParamsRadio->setChecked(true);
        else
            m_useCustomParamsRadio->setChecked(true);
        m_paramsEditor->setParams(initial.useDefaultActionParams ? defaultActionParams
                                                                  : m_initialCustomActionParams);

        if (m_initialSweepRegionUseRandomPoint)
            m_sweepRandomRadio->setChecked(true);
        else
            m_sweepSequenceRadio->setChecked(true);
        onRegionSelectionChanged();
    }

    onModeChanged();
    if (includeActionParams)
        resize(560, 760);
    else
        resize(460, allowPopupDialogTarget ? 320 : 240);
}

void StepEditorDialog::onModeChanged()
{
    const bool named = m_namedRegionRadio->isChecked();
    m_namedRegionCombo->setEnabled(named && m_namedRegionCombo->count() > 0 && m_namedRegionRadio->isEnabled());
    onRegionSelectionChanged();
}

bool StepEditorDialog::useWholeWindow() const
{
    return m_wholeWindowRadio->isChecked();
}

QString StepEditorDialog::regionName() const
{
    return m_namedRegionRadio->isChecked() ? m_namedRegionCombo->currentText() : QString();
}

bool StepEditorDialog::targetsPopupDialog() const
{
    return m_popupDialogRadio && m_popupDialogRadio->isChecked();
}

void StepEditorDialog::onActionParamsModeChanged()
{
    const bool useDefault = m_useDefaultParamsRadio->isChecked();
    if (useDefault) {
        // Capture whatever is currently shown (the custom values just being
        // edited) before overwriting the editor with the read-only default
        // values, so switching back to "use custom" restores the edit
        // instead of reverting to the dialog-open-time snapshot.
        m_initialCustomActionParams = m_paramsEditor->params();
    }
    m_paramsEditor->setParams(useDefault ? m_defaultActionParams : m_initialCustomActionParams);
}

void StepEditorDialog::applyActionKindsTo(RegionStep &step) const
{
    if (m_kindEditor)
        m_kindEditor->applyKindsTo(step);
}

bool StepEditorDialog::useDefaultActionParams() const
{
    return !m_useCustomParamsRadio || m_useDefaultParamsRadio->isChecked();
}

ActionParams StepEditorDialog::customActionParams() const
{
    return m_paramsEditor ? m_paramsEditor->params() : ActionParams();
}

void StepEditorDialog::applySweepModeTo(RegionStep &step) const
{
    if (m_sweepModeGroup && m_sweepModeGroup->isVisible())
        step.sweepRegionUseRandomPoint = m_sweepRandomRadio->isChecked();
}

void StepEditorDialog::onRegionSelectionChanged()
{
    updateRegionButtonsEnabled();
    if (!m_sweepModeGroup)
        return;
    bool isSweepRegion = false;
    if (m_namedRegionRadio->isChecked() && m_regionCtx && m_regionCtx->namedRegions) {
        const QString name = m_namedRegionCombo->currentText();
        for (const NamedRegion &region : *m_regionCtx->namedRegions) {
            if (region.name == name && region.isSweepTarget) {
                isSweepRegion = true;
                break;
            }
        }
    }
    m_sweepModeGroup->setVisible(isSweepRegion);
}

void StepEditorDialog::refreshRegionCombo(int selectIndex)
{
    const QString previousText =
        (selectIndex < 0 && m_namedRegionCombo->count() > 0) ? m_namedRegionCombo->currentText() : QString();

    m_namedRegionCombo->blockSignals(true);
    m_namedRegionCombo->clear();
    const bool hasRegions = m_regionCtx && m_regionCtx->namedRegions && !m_regionCtx->namedRegions->isEmpty();
    if (hasRegions) {
        for (const NamedRegion &region : *m_regionCtx->namedRegions)
            m_namedRegionCombo->addItem(region.name);
    } else {
        m_namedRegionCombo->addItem(
            I18n::t(QStringLiteral("（操作領域がありません。「新規作成...」から追加してください）")));
        // The just-deleted (or never-existing) region can no longer be
        // selected -- fall back to the always-available whole-window
        // choice rather than leaving the radio checked with nothing valid
        // behind it.
        if (m_namedRegionRadio->isChecked())
            m_wholeWindowRadio->setChecked(true);
    }
    m_namedRegionRadio->setEnabled(hasRegions);

    if (selectIndex >= 0 && selectIndex < m_namedRegionCombo->count()) {
        m_namedRegionCombo->setCurrentIndex(selectIndex);
    } else if (!previousText.isEmpty()) {
        const int idx = m_namedRegionCombo->findText(previousText);
        m_namedRegionCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_namedRegionCombo->blockSignals(false);

    onModeChanged();  // recomputes combo's own enabled-state from the radios
    updateRegionButtonsEnabled();
}

void StepEditorDialog::updateRegionButtonsEnabled()
{
    const bool hasValidSelection = m_regionCtx && m_regionCtx->namedRegions &&
                                    m_namedRegionCombo->currentIndex() >= 0 &&
                                    m_namedRegionCombo->currentIndex() < m_regionCtx->namedRegions->size();
    m_editRegionButton->setEnabled(hasValidSelection);
    m_deleteRegionButton->setEnabled(hasValidSelection);
}

void StepEditorDialog::onCreateRegion()
{
    if (!m_regionCtx || !m_regionCtx->namedRegions)
        return;

    NamedRegion initial;
    initial.name = generateDefaultRegionName(*m_regionCtx->namedRegions);
    NamedRegionEditorDialog dialog(initial, m_regionCtx->targetTopLeft, m_regionCtx->hasTarget,
                                   m_regionCtx->targetPid, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const NamedRegion region = dialog.result();
    for (const NamedRegion &existing : *m_regionCtx->namedRegions) {
        if (existing.name == region.name) {
            QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                                  I18n::t(QStringLiteral("同じ名前の操作領域が既に存在します。")));
            return;
        }
    }
    m_regionCtx->namedRegions->append(region);
    refreshRegionCombo(m_regionCtx->namedRegions->size() - 1);
    m_namedRegionRadio->setChecked(true);
}

void StepEditorDialog::onEditRegion()
{
    if (!m_regionCtx || !m_regionCtx->namedRegions)
        return;
    const int idx = m_namedRegionCombo->currentIndex();
    if (idx < 0 || idx >= m_regionCtx->namedRegions->size())
        return;

    const QString oldName = (*m_regionCtx->namedRegions)[idx].name;
    NamedRegionEditorDialog dialog((*m_regionCtx->namedRegions)[idx], m_regionCtx->targetTopLeft,
                                   m_regionCtx->hasTarget, m_regionCtx->targetPid, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const NamedRegion region = dialog.result();
    for (int i = 0; i < m_regionCtx->namedRegions->size(); ++i) {
        if (i != idx && (*m_regionCtx->namedRegions)[i].name == region.name) {
            QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                                  I18n::t(QStringLiteral("同じ名前の操作領域が既に存在します。")));
            return;
        }
    }
    (*m_regionCtx->namedRegions)[idx] = region;
    if (oldName != region.name && m_regionCtx->renameReferences)
        m_regionCtx->renameReferences(oldName, region.name);
    refreshRegionCombo(idx);
}

void StepEditorDialog::onDeleteRegion()
{
    if (!m_regionCtx || !m_regionCtx->namedRegions)
        return;
    const int idx = m_namedRegionCombo->currentIndex();
    if (idx < 0 || idx >= m_regionCtx->namedRegions->size())
        return;

    const QString name = (*m_regionCtx->namedRegions)[idx].name;
    if (m_regionCtx->stepsReferencing) {
        const QStringList refs = m_regionCtx->stepsReferencing(name);
        if (!refs.isEmpty()) {
            QMessageBox::warning(
                this, I18n::t(QStringLiteral("削除できません")),
                I18n::t(QStringLiteral("この操作領域は次のステップで使われているため削除できません: %1\n"
                                        "先にそれらのステップの領域を変更するか、ステップを削除してください。"))
                    .arg(refs.join(QStringLiteral(", "))));
            return;
        }
    }
    m_regionCtx->namedRegions->removeAt(idx);
    refreshRegionCombo();
}
