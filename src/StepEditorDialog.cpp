#include "StepEditorDialog.h"
#include "ActionKindEditor.h"
#include "ActionParamsEditor.h"
#include "I18n.h"
#include "PointHighlightOverlay.h"
#include "PointPickerOverlay.h"

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
#include <QSpinBox>
#include <QVBoxLayout>

StepEditorDialog::StepEditorDialog(const RegionStep &initial, const QList<NamedRegion> &availableRegions,
                                    QWidget *parent, bool allowPopupDialogTarget, bool includeActionParams,
                                    const ActionParams &defaultActionParams, const QPoint &targetTopLeft,
                                    bool hasTarget)
    : QDialog(parent), m_defaultActionParams(defaultActionParams),
      m_initialCustomActionParams(initial.customActionParams), m_sweepStart(initial.sweepStart),
      m_sweepEnd(initial.sweepEnd), m_sweepStartPicked(!initial.sweepStart.isNull()),
      m_sweepEndPicked(!initial.sweepEnd.isNull()), m_targetTopLeft(targetTopLeft), m_hasTarget(hasTarget)
{
    setWindowTitle(includeActionParams ? I18n::t(QStringLiteral("ステップの設定"))
                                        : I18n::t(QStringLiteral("ステップの操作領域を選択")));
    if (includeActionParams) {
        // See SetupActionEditorDialog's identical comment: the sweep
        // start/end highlight overlay (PointHighlightOverlay) uses
        // Qt::WindowStaysOnTopHint, so this dialog needs to be in that same
        // layer or several window managers keep re-asserting the overlay
        // above it, making the dialog unreachable.
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    }

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
    for (const NamedRegion &region : availableRegions)
        m_namedRegionCombo->addItem(region.name);
    namedRegionRow->addWidget(m_namedRegionCombo, 1);
    layout->addLayout(namedRegionRow);
    connect(m_wholeWindowRadio, &QRadioButton::toggled, this, &StepEditorDialog::onModeChanged);

    if (availableRegions.isEmpty()) {
        m_namedRegionRadio->setEnabled(false);
        m_namedRegionCombo->setEnabled(false);
        m_namedRegionCombo->addItem(I18n::t(QStringLiteral("（①対象選択パネルで操作領域を追加してください）")));
    }

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

        auto *pointModeGroup =
            new QGroupBox(I18n::t(QStringLiteral("操作位置の選び方")), scrollContent);
        auto *pointModeLayout = new QVBoxLayout(pointModeGroup);
        auto *pointModeRow = new QHBoxLayout;
        m_randomPointRadio = new QRadioButton(I18n::t(QStringLiteral("ランダム")), pointModeGroup);
        m_sweepPointRadio = new QRadioButton(I18n::t(QStringLiteral("点列（スイープ）")), pointModeGroup);
        pointModeRow->addWidget(m_randomPointRadio);
        pointModeRow->addWidget(m_sweepPointRadio);
        pointModeRow->addStretch();
        pointModeLayout->addLayout(pointModeRow);
        auto *sweepHintLabel = new QLabel(
            I18n::t(QStringLiteral("「点列（スイープ）」では、開始位置から終了位置まで指定した間隔で並んだ"
                                    "点を順番に操作します（最後まで行くと開始位置に戻って繰り返します）。")),
            pointModeGroup);
        sweepHintLabel->setWordWrap(true);
        pointModeLayout->addWidget(sweepHintLabel);

        auto *sweepStartRow = new QHBoxLayout;
        m_pickSweepStartButton = new QPushButton(I18n::t(QStringLiteral("開始位置を選択...")), pointModeGroup);
        m_sweepStartValueLabel = new QLabel(pointModeGroup);
        sweepStartRow->addWidget(m_pickSweepStartButton);
        sweepStartRow->addWidget(m_sweepStartValueLabel, 1);
        pointModeLayout->addLayout(sweepStartRow);

        auto *sweepEndRow = new QHBoxLayout;
        m_pickSweepEndButton = new QPushButton(I18n::t(QStringLiteral("終了位置を選択...")), pointModeGroup);
        m_sweepEndValueLabel = new QLabel(pointModeGroup);
        sweepEndRow->addWidget(m_pickSweepEndButton);
        sweepEndRow->addWidget(m_sweepEndValueLabel, 1);
        pointModeLayout->addLayout(sweepEndRow);

        auto *sweepIntervalRow = new QHBoxLayout;
        sweepIntervalRow->addWidget(new QLabel(I18n::t(QStringLiteral("間隔 (px):")), pointModeGroup));
        m_sweepIntervalSpin = new QSpinBox(pointModeGroup);
        m_sweepIntervalSpin->setRange(1, 5000);
        sweepIntervalRow->addWidget(m_sweepIntervalSpin, 1);
        pointModeLayout->addLayout(sweepIntervalRow);

        auto *sweepJitterRow = new QHBoxLayout;
        sweepJitterRow->addWidget(new QLabel(I18n::t(QStringLiteral("ランダム幅 (px):")), pointModeGroup));
        m_sweepJitterSpin = new QSpinBox(pointModeGroup);
        m_sweepJitterSpin->setRange(0, 1000);
        sweepJitterRow->addWidget(m_sweepJitterSpin, 1);
        pointModeLayout->addLayout(sweepJitterRow);
        auto *sweepJitterHintLabel = new QLabel(
            I18n::t(QStringLiteral("※各点を実際に操作する際、上下左右にこの範囲内でランダムにずらします"
                                    "（0なら常に同じ位置）。")),
            pointModeGroup);
        sweepJitterHintLabel->setWordWrap(true);
        pointModeLayout->addWidget(sweepJitterHintLabel);
        scrollLayout->addWidget(pointModeGroup);
        connect(m_randomPointRadio, &QRadioButton::toggled, this,
                &StepEditorDialog::onPointSelectionModeChanged);
        connect(m_pickSweepStartButton, &QPushButton::clicked, this, &StepEditorDialog::onPickSweepStart);
        connect(m_pickSweepEndButton, &QPushButton::clicked, this, &StepEditorDialog::onPickSweepEnd);

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

    // Populate from `initial`.
    if (m_popupDialogRadio && initial.targetsPopupDialog) {
        m_popupDialogRadio->setChecked(true);
    } else if (initial.useWholeWindow || availableRegions.isEmpty()) {
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

        if (initial.pointSelectionMode == PointSelectionMode::Sweep)
            m_sweepPointRadio->setChecked(true);
        else
            m_randomPointRadio->setChecked(true);
        m_sweepIntervalSpin->setValue(initial.sweepIntervalPx > 0 ? initial.sweepIntervalPx : 50);
        m_sweepJitterSpin->setValue(qMax(0, initial.sweepJitterPx));
        refreshSweepLabels();
        onPointSelectionModeChanged();

        // See SetupActionEditorDialog's identical pattern: visualize the
        // sweep start/end points on screen for as long as this dialog
        // stays open, hidden (not destroyed) on close regardless of how it
        // closed.
        connect(this, &QDialog::finished, this, [this](int) {
            if (m_highlightOverlay)
                m_highlightOverlay->hide();
        });
    }

    onModeChanged();
    if (includeActionParams)
        resize(560, 760);
    else
        resize(420, allowPopupDialogTarget ? 280 : 200);
}

void StepEditorDialog::onModeChanged()
{
    const bool named = m_namedRegionRadio->isChecked();
    m_namedRegionCombo->setEnabled(named && m_namedRegionCombo->count() > 0 && m_namedRegionRadio->isEnabled());
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

PointSelectionMode StepEditorDialog::pointSelectionMode() const
{
    return (m_sweepPointRadio && m_sweepPointRadio->isChecked()) ? PointSelectionMode::Sweep
                                                                   : PointSelectionMode::Random;
}

bool StepEditorDialog::sweepPointsValid() const
{
    if (pointSelectionMode() != PointSelectionMode::Sweep)
        return true;
    return m_sweepStartPicked && m_sweepEndPicked;
}

void StepEditorDialog::applySweepTo(RegionStep &step) const
{
    step.pointSelectionMode = pointSelectionMode();
    step.sweepStart = m_sweepStart;
    step.sweepEnd = m_sweepEnd;
    if (m_sweepIntervalSpin)
        step.sweepIntervalPx = m_sweepIntervalSpin->value();
    if (m_sweepJitterSpin)
        step.sweepJitterPx = m_sweepJitterSpin->value();
}

void StepEditorDialog::onPointSelectionModeChanged()
{
    const bool sweep = m_sweepPointRadio && m_sweepPointRadio->isChecked();
    if (m_pickSweepStartButton) {
        m_pickSweepStartButton->setEnabled(sweep);
        m_pickSweepEndButton->setEnabled(sweep);
        m_sweepIntervalSpin->setEnabled(sweep);
        m_sweepJitterSpin->setEnabled(sweep);
    }
    updateSweepHighlight();
}

void StepEditorDialog::onPickSweepStart()
{
    pickSweepPointInto(m_sweepStart, m_sweepStartPicked);
}

void StepEditorDialog::onPickSweepEnd()
{
    pickSweepPointInto(m_sweepEnd, m_sweepEndPicked);
}

void StepEditorDialog::pickSweepPointInto(QPoint &target, bool &pickedFlag)
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
        refreshSweepLabels();
        updateSweepHighlight();
    }
}

void StepEditorDialog::refreshSweepLabels()
{
    if (!m_sweepStartValueLabel)
        return;
    auto pointText = [](const QPoint &p) { return QStringLiteral("(%1, %2)").arg(p.x()).arg(p.y()); };
    m_sweepStartValueLabel->setText(pointText(m_sweepStart));
    m_sweepEndValueLabel->setText(pointText(m_sweepEnd));
}

void StepEditorDialog::updateSweepHighlight()
{
    if (!m_sweepPointRadio || !m_sweepPointRadio->isChecked()) {
        if (m_highlightOverlay)
            m_highlightOverlay->hide();
        return;
    }
    QList<QPoint> points;
    QList<QString> labels;
    if (m_sweepStartPicked) {
        points << (m_sweepStart + m_targetTopLeft);
        labels << I18n::t(QStringLiteral("開始"));
    }
    if (m_sweepEndPicked) {
        points << (m_sweepEnd + m_targetTopLeft);
        labels << I18n::t(QStringLiteral("終了"));
    }
    if (points.isEmpty()) {
        if (m_highlightOverlay)
            m_highlightOverlay->hide();
        return;
    }
    if (!m_highlightOverlay)
        m_highlightOverlay = new PointHighlightOverlay(this);
    m_highlightOverlay->showPoints(points, labels);
    // See the constructor's WindowStaysOnTopHint comment.
    raise();
    activateWindow();
}
