#include "MainWindow.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "ActionKindEditor.h"
#include "ActionParamsEditor.h"
#include "DefaultActionParamsDialog.h"
#include "NamedRegionEditorDialog.h"
#include "RegionSelectorOverlay.h"
#include "StepEditorDialog.h"
#include "StopPanel.h"

namespace
{
QLabel *columnHeader(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont f = label->font();
    f.setPointSize(f.pointSize() + 2);
    f.setBold(true);
    label->setFont(f);
    return label;
}

const ActionParams &effectiveParamsOf(const RegionStep &step, const ActionParams &defaults)
{
    return step.useDefaultActionParams ? defaults : step.customActionParams;
}
}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_engine = new RandomActionEngine(this);
    connect(m_engine, &RandomActionEngine::finished, this, &MainWindow::onEngineFinished);
    connect(m_engine, &RandomActionEngine::logMessage, this, &MainWindow::onActionLog);
    connect(m_engine, &RandomActionEngine::iterationCountChanged, this,
            &MainWindow::onIterationCountChanged);
    connect(m_engine, &RandomActionEngine::pausedChanged, this, &MainWindow::onEnginePausedChanged);
    connect(m_engine, &RandomActionEngine::resourceUsageUpdated, this,
            &MainWindow::onResourceUsageUpdated);
    connect(m_engine, &RandomActionEngine::currentStepChanged, this,
            &MainWindow::onCurrentStepChanged);

    m_uiTimer = new QTimer(this);
    m_uiTimer->setInterval(500);
    connect(m_uiTimer, &QTimer::timeout, this, &MainWindow::updateElapsedLabel);

    // The Accessibility permission is typically granted/toggled in System
    // Settings while this app is running, then the user alt-tabs back.
    // Re-check it (and refresh the target list, in case new windows
    // appeared) whenever the app regains focus, instead of only at
    // startup / on manual "更新" clicks.
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive)
                    onRefreshTargets();
            });

    buildUi();
    onRefreshTargets();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("EnduranceTestGUI - GUI耐久テストツール"));

    buildMenuBar();

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *rootLayout = new QVBoxLayout(central);

    // SPEC.md 6.9: 3 vertically-stacked rows -- ①対象選択 on top (full
    // width), ②ステップ構成/③操作パラメータ split horizontally in the
    // middle row, and execution controls + ログ on the bottom row. Nesting
    // a horizontal QSplitter inside a vertical one lets the user drag to
    // resize every one of these (row boundaries and the ②/③ divide alike).
    auto *outerSplitter = new QSplitter(Qt::Vertical, central);

    outerSplitter->addWidget(buildTargetColumn(outerSplitter));

    auto *middleSplitter = new QSplitter(Qt::Horizontal, outerSplitter);
    middleSplitter->addWidget(buildStepsColumn(middleSplitter));
    middleSplitter->addWidget(buildActionParamsColumn(middleSplitter));
    middleSplitter->setStretchFactor(0, 1);
    middleSplitter->setStretchFactor(1, 2);
    outerSplitter->addWidget(middleSplitter);

    // Execution controls and ログ don't belong to any one of ①②③ -- they're
    // cross-cutting (control/monitor the run as a whole), so they share the
    // bottom row rather than living inside one of the three sections.
    auto *bottomWidget = new QWidget(outerSplitter);
    auto *bottomLayout = new QVBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    // --- Controls ---
    // Split across two rows (buttons+status, then the run-progress labels)
    // rather than one long row, so both stay usable when the window is
    // narrow (SPEC.md 6.9).
    auto *controlsRow = new QHBoxLayout;
    m_startButton = new QPushButton(QStringLiteral("▶ 開始"), central);
    m_stopButton = new QPushButton(QStringLiteral("■ 停止"), central);
    m_stopButton->setEnabled(false);
    m_pauseResumeButton = new QPushButton(QStringLiteral("‖ 一時停止"), central);
    m_pauseResumeButton->setEnabled(false);
    m_statusLabel = new QLabel(QStringLiteral("待機中"), central);
    controlsRow->addWidget(m_startButton);
    controlsRow->addWidget(m_stopButton);
    controlsRow->addWidget(m_pauseResumeButton);
    controlsRow->addWidget(m_statusLabel);
    controlsRow->addStretch();
    bottomLayout->addLayout(controlsRow);

    auto *progressRow = new QHBoxLayout;
    m_resourceUsageLabel = new QLabel(QString(), central);
    m_elapsedLabel = new QLabel(QStringLiteral("経過: 0秒"), central);
    m_iterationLabel = new QLabel(QStringLiteral("実行回数: 0"), central);
    progressRow->addWidget(m_resourceUsageLabel);
    progressRow->addStretch();
    progressRow->addWidget(m_elapsedLabel);
    progressRow->addWidget(m_iterationLabel);
    bottomLayout->addLayout(progressRow);

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_pauseResumeButton, &QPushButton::clicked, this, &MainWindow::onPauseResume);

    // --- Log ---
    auto *logGroup = new QGroupBox(QStringLiteral("ログ"), central);
    auto *logLayout = new QVBoxLayout(logGroup);
    m_logView = new QPlainTextEdit(logGroup);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(5000);
    logLayout->addWidget(m_logView);
    auto *logButtonsRow = new QHBoxLayout;
    auto *clearLogButton = new QPushButton(QStringLiteral("ログをクリア"), logGroup);
    auto *saveLogButton = new QPushButton(QStringLiteral("ログを保存..."), logGroup);
    logButtonsRow->addWidget(clearLogButton);
    logButtonsRow->addWidget(saveLogButton);
    logButtonsRow->addStretch();
    logLayout->addLayout(logButtonsRow);
    connect(clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLog);
    connect(saveLogButton, &QPushButton::clicked, this, &MainWindow::onSaveLog);

    bottomLayout->addWidget(logGroup, 1);
    outerSplitter->addWidget(bottomWidget);

    // ①対象選択 row stays compact by default; ②/③ gets the most vertical
    // space; the controls+ログ row gets a modest share. All are still
    // freely draggable by the user afterward.
    outerSplitter->setStretchFactor(0, 0);
    outerSplitter->setStretchFactor(1, 3);
    outerSplitter->setStretchFactor(2, 1);

    rootLayout->addWidget(outerSplitter, 1);

    refreshNamedRegionList();
    refreshStepList();
    m_actionParamsEditor->setParams(m_defaultActionParams);

    resize(1200, 900);
}

void MainWindow::buildMenuBar()
{
    auto *helpMenu = menuBar()->addMenu(QStringLiteral("ヘルプ"));

    auto *aboutAppAction = helpMenu->addAction(QStringLiteral("EnduranceTestGUIについて..."));
    connect(aboutAppAction, &QAction::triggered, this, &MainWindow::onAboutApp);

    // Required by the Qt LGPLv3/GPL license terms this app is built
    // against: end users must be able to see which license Qt itself is
    // distributed under and read its full text. QMessageBox::aboutQt()
    // is Qt's own standard dialog for this -- it names the exact license
    // and has a "Show License" button with the complete text, so no
    // license text needs to be bundled/reproduced separately here.
    auto *aboutQtAction = helpMenu->addAction(QStringLiteral("Qtについて..."));
    connect(aboutQtAction, &QAction::triggered, this, &MainWindow::onAboutQt);
}

void MainWindow::onAboutApp()
{
    QMessageBox::about(
        this, QStringLiteral("EnduranceTestGUIについて"),
        QStringLiteral(
            "EnduranceTestGUI\n\n"
            "GUIアプリケーションの耐久テスト（ランダムクリック・ランダム操作）を行うための"
            "デスクトップツールです。\n\n"
            "Qt %1 を使用して構築されています。Qtはフリーソフトウェア版の場合"
            "GNU LGPL バージョン3（一部モジュールはGPL）の下で配布されています。Qt自体の"
            "ライセンス条文は、このメニューの「Qtについて...」から確認できます。")
            .arg(QStringLiteral(QT_VERSION_STR)));
}

void MainWindow::onAboutQt()
{
    QMessageBox::aboutQt(this);
}

QWidget *MainWindow::buildTargetColumn(QWidget *parent)
{
    auto *wrapper = new QWidget(parent);
    auto *wrapperLayout = new QVBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addWidget(columnHeader(QStringLiteral("① 対象選択"), wrapper));

    auto *scroll = new QScrollArea(wrapper);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    wrapperLayout->addWidget(scroll, 1);

    auto *container = new QWidget;
    scroll->setWidget(container);
    auto *layout = new QVBoxLayout(container);

    // --- Target group ---
    m_targetGroup = new QGroupBox(QStringLiteral("対象 (Target)"), container);
    auto *targetLayout = new QVBoxLayout(m_targetGroup);
    auto *targetRow = new QHBoxLayout;
    m_targetCombo = new QComboBox(m_targetGroup);
    m_refreshButton = new QPushButton(QStringLiteral("更新"), m_targetGroup);
    targetRow->addWidget(m_targetCombo, 1);
    targetRow->addWidget(m_refreshButton);
    targetLayout->addLayout(targetRow);

    // Label above, button below (not side-by-side) so the label has room
    // to wrap instead of squeezing the button when this column is narrow.
    m_permissionLabel = new QLabel(m_targetGroup);
    m_permissionLabel->setWordWrap(true);
    targetLayout->addWidget(m_permissionLabel);
    m_openSettingsButton = new QPushButton(QStringLiteral("権限設定を開く"), m_targetGroup);
    targetLayout->addWidget(m_openSettingsButton);

    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshTargets);
    connect(m_openSettingsButton, &QPushButton::clicked, this,
            &MainWindow::onOpenAccessibilitySettings);

    layout->addWidget(m_targetGroup);

    // --- Named operation regions ---
    m_namedRegionGroup = new QGroupBox(
        QStringLiteral("操作領域（名前付き。②でステップ作成時に選択して使う）"), container);
    auto *namedRegionLayout = new QVBoxLayout(m_namedRegionGroup);
    m_namedRegionListWidget = new QListWidget(m_namedRegionGroup);
    m_namedRegionListWidget->setMaximumHeight(110);
    namedRegionLayout->addWidget(m_namedRegionListWidget);
    auto *namedRegionButtonsRow = new QHBoxLayout;
    m_addNamedRegionButton = new QPushButton(QStringLiteral("追加..."), m_namedRegionGroup);
    m_editNamedRegionButton = new QPushButton(QStringLiteral("編集..."), m_namedRegionGroup);
    m_removeNamedRegionButton = new QPushButton(QStringLiteral("削除"), m_namedRegionGroup);
    namedRegionButtonsRow->addWidget(m_addNamedRegionButton);
    namedRegionButtonsRow->addWidget(m_editNamedRegionButton);
    namedRegionButtonsRow->addWidget(m_removeNamedRegionButton);
    namedRegionLayout->addLayout(namedRegionButtonsRow);

    connect(m_addNamedRegionButton, &QPushButton::clicked, this, &MainWindow::onAddNamedRegion);
    connect(m_editNamedRegionButton, &QPushButton::clicked, this,
            &MainWindow::onEditSelectedNamedRegion);
    connect(m_removeNamedRegionButton, &QPushButton::clicked, this,
            &MainWindow::onRemoveSelectedNamedRegion);

    layout->addWidget(m_namedRegionGroup);

    // --- Timing group ---
    m_timingGroup = new QGroupBox(QStringLiteral("タイミング・制限"), container);
    auto *timingForm = new QFormLayout(m_timingGroup);
    // Some of these row labels are long (e.g. "ステップ全体（シーケンス）の繰り返し回数
    // （必須）:"); let Qt move the field below the label instead of shrinking it when this
    // column is narrow (SPEC.md 6.9), rather than a fixed side-by-side layout.
    timingForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    // Operation interval: either an ms range (direct) or a rate range
    // (operations/sec, converted to an equivalent ms range in
    // buildConfigFromUi) -- the mode radios switch which page of
    // m_intervalStack is shown, but both pages keep their own values so
    // switching back and forth doesn't lose anything (SPEC.md 6.6).
    auto *intervalContainer = new QWidget(m_timingGroup);
    auto *intervalContainerLayout = new QVBoxLayout(intervalContainer);
    intervalContainerLayout->setContentsMargins(0, 0, 0, 0);

    auto *intervalModeRow = new QHBoxLayout;
    m_intervalModeMsRadio = new QRadioButton(QStringLiteral("ms指定"), intervalContainer);
    m_intervalModeRateRadio = new QRadioButton(QStringLiteral("回数/秒指定"), intervalContainer);
    m_intervalModeMsRadio->setChecked(true);
    intervalModeRow->addWidget(m_intervalModeMsRadio);
    intervalModeRow->addWidget(m_intervalModeRateRadio);
    intervalModeRow->addStretch();
    intervalContainerLayout->addLayout(intervalModeRow);

    m_intervalStack = new QStackedWidget(intervalContainer);

    auto *msPage = new QWidget(m_intervalStack);
    auto *msPageLayout = new QHBoxLayout(msPage);
    msPageLayout->setContentsMargins(0, 0, 0, 0);
    m_minIntervalSpin = new QSpinBox(msPage);
    m_minIntervalSpin->setRange(10, 600000);
    m_minIntervalSpin->setValue(20);
    m_maxIntervalSpin = new QSpinBox(msPage);
    m_maxIntervalSpin->setRange(10, 600000);
    m_maxIntervalSpin->setValue(100);
    msPageLayout->addWidget(m_minIntervalSpin);
    msPageLayout->addWidget(new QLabel(QStringLiteral("〜"), msPage));
    msPageLayout->addWidget(m_maxIntervalSpin);
    msPageLayout->addWidget(new QLabel(QStringLiteral("ms"), msPage));
    msPageLayout->addStretch();
    m_intervalStack->addWidget(msPage);

    // Rate range is the reciprocal of the ms range, so the *min* rate
    // (slowest pace) corresponds to the *max* ms interval and vice versa --
    // matching the ms page's defaults (20ms〜100ms == 50〜10回/秒).
    auto *ratePage = new QWidget(m_intervalStack);
    auto *ratePageLayout = new QHBoxLayout(ratePage);
    ratePageLayout->setContentsMargins(0, 0, 0, 0);
    m_minRateSpin = new QDoubleSpinBox(ratePage);
    m_minRateSpin->setRange(0.01, 100.0);
    m_minRateSpin->setDecimals(2);
    m_minRateSpin->setValue(10.0);
    m_maxRateSpin = new QDoubleSpinBox(ratePage);
    m_maxRateSpin->setRange(0.01, 100.0);
    m_maxRateSpin->setDecimals(2);
    m_maxRateSpin->setValue(50.0);
    ratePageLayout->addWidget(m_minRateSpin);
    ratePageLayout->addWidget(new QLabel(QStringLiteral("〜"), ratePage));
    ratePageLayout->addWidget(m_maxRateSpin);
    ratePageLayout->addWidget(new QLabel(QStringLiteral("回/秒"), ratePage));
    ratePageLayout->addStretch();
    m_intervalStack->addWidget(ratePage);

    intervalContainerLayout->addWidget(m_intervalStack);
    connect(m_intervalModeRateRadio, &QRadioButton::toggled, this, [this](bool checked) {
        m_intervalStack->setCurrentIndex(checked ? 1 : 0);
    });

    timingForm->addRow(QStringLiteral("操作間隔:"), intervalContainer);

    m_maxIterationsSpin = new QSpinBox(m_timingGroup);
    m_maxIterationsSpin->setRange(1, 100000000);
    m_maxIterationsSpin->setValue(1000);
    timingForm->addRow(QStringLiteral("最大実行回数（全ステップ合計、必須）:"), m_maxIterationsSpin);

    m_maxDurationSecSpin = new QSpinBox(m_timingGroup);
    m_maxDurationSecSpin->setRange(1, 6000000);
    m_maxDurationSecSpin->setValue(120);
    m_maxDurationSecSpin->setSuffix(QStringLiteral(" 秒"));
    timingForm->addRow(QStringLiteral("最大実行時間（必須）:"), m_maxDurationSecSpin);

    m_maxSequenceLoopsSpin = new QSpinBox(m_timingGroup);
    m_maxSequenceLoopsSpin->setRange(1, 100000000);
    m_maxSequenceLoopsSpin->setValue(10);
    timingForm->addRow(QStringLiteral("ステップ全体（シーケンス）の繰り返し回数（必須）:"), m_maxSequenceLoopsSpin);

    m_keepActiveCheck = new QCheckBox(QStringLiteral("対象アプリを常に最前面に保つ"), m_timingGroup);
    m_keepActiveCheck->setChecked(true);
    timingForm->addRow(QString(), m_keepActiveCheck);

    m_rngSeedSpin = new QSpinBox(m_timingGroup);
    m_rngSeedSpin->setRange(0, 2000000000);
    m_rngSeedSpin->setValue(0);
    m_rngSeedSpin->setSpecialValueText(QStringLiteral("ランダム"));
    timingForm->addRow(QStringLiteral("乱数シード（クラッシュ再現用。開始時にログに記録される）:"), m_rngSeedSpin);

    layout->addWidget(m_timingGroup);
    layout->addStretch();

    return wrapper;
}

QWidget *MainWindow::buildStepsColumn(QWidget *parent)
{
    auto *wrapper = new QWidget(parent);
    auto *wrapperLayout = new QVBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addWidget(columnHeader(QStringLiteral("② ステップ構成"), wrapper));

    m_stepsGroup = new QGroupBox(
        QStringLiteral("領域ごとに操作種別・回数を指定し、順番に繰り返し実行"), wrapper);
    auto *stepsLayout = new QVBoxLayout(m_stepsGroup);
    m_stepListWidget = new QListWidget(m_stepsGroup);
    stepsLayout->addWidget(m_stepListWidget, 1);

    auto *stepButtonsRow = new QHBoxLayout;
    m_addStepButton = new QPushButton(QStringLiteral("追加..."), m_stepsGroup);
    m_addWaitStepButton = new QPushButton(QStringLiteral("待機を追加..."), m_stepsGroup);
    m_editStepButton = new QPushButton(QStringLiteral("編集..."), m_stepsGroup);
    m_removeStepButton = new QPushButton(QStringLiteral("削除"), m_stepsGroup);
    stepButtonsRow->addWidget(m_addStepButton);
    stepButtonsRow->addWidget(m_addWaitStepButton);
    stepButtonsRow->addWidget(m_editStepButton);
    stepButtonsRow->addWidget(m_removeStepButton);
    stepsLayout->addLayout(stepButtonsRow);

    auto *stepOrderRow = new QHBoxLayout;
    m_moveStepUpButton = new QPushButton(QStringLiteral("↑ 上へ"), m_stepsGroup);
    m_moveStepDownButton = new QPushButton(QStringLiteral("↓ 下へ"), m_stepsGroup);
    m_clearStepsButton = new QPushButton(QStringLiteral("すべて削除"), m_stepsGroup);
    stepOrderRow->addWidget(m_moveStepUpButton);
    stepOrderRow->addWidget(m_moveStepDownButton);
    stepOrderRow->addWidget(m_clearStepsButton);
    stepsLayout->addLayout(stepOrderRow);

    connect(m_stepListWidget, &QListWidget::currentRowChanged, this,
            &MainWindow::onStepSelectionChanged);
    connect(m_addStepButton, &QPushButton::clicked, this, &MainWindow::onAddStep);
    connect(m_addWaitStepButton, &QPushButton::clicked, this, &MainWindow::onAddWaitStep);
    connect(m_editStepButton, &QPushButton::clicked, this, &MainWindow::onEditSelectedStep);
    connect(m_removeStepButton, &QPushButton::clicked, this, &MainWindow::onRemoveSelectedStep);
    connect(m_moveStepUpButton, &QPushButton::clicked, this, &MainWindow::onMoveStepUp);
    connect(m_moveStepDownButton, &QPushButton::clicked, this, &MainWindow::onMoveStepDown);
    connect(m_clearStepsButton, &QPushButton::clicked, this, &MainWindow::onClearSteps);

    wrapperLayout->addWidget(m_stepsGroup, 1);
    return wrapper;
}

QWidget *MainWindow::buildActionParamsColumn(QWidget *parent)
{
    auto *wrapper = new QWidget(parent);
    auto *wrapperLayout = new QVBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addWidget(columnHeader(QStringLiteral("③ 操作パラメータ"), wrapper));

    auto *contextRow = new QHBoxLayout;
    m_actionParamsContextLabel = new QLabel(QStringLiteral("デフォルト値を編集中（ステップ未選択）"), wrapper);
    contextRow->addWidget(m_actionParamsContextLabel, 1);
    m_editDefaultParamsButton = new QPushButton(QStringLiteral("デフォルト"), wrapper);
    m_editDefaultParamsButton->setToolTip(
        QStringLiteral("共通のデフォルト操作パラメータをダイアログで編集します（②の選択は変わりません）"));
    contextRow->addWidget(m_editDefaultParamsButton);
    wrapperLayout->addLayout(contextRow);
    connect(m_editDefaultParamsButton, &QPushButton::clicked, this, &MainWindow::onEditDefaultParams);

    auto *scroll = new QScrollArea(wrapper);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *scrollContent = new QWidget;
    scroll->setWidget(scrollContent);
    auto *scrollLayout = new QVBoxLayout(scrollContent);

    // --- Step-level: which action kinds this step performs, their
    // relative weight, and how many actions to run (SPEC.md 6.2/6.3).
    m_stepKindGroup = new QGroupBox(
        QStringLiteral("このステップの操作種別・重み・回数（②でステップを選択すると編集できます）"),
        scrollContent);
    auto *kindLayout = new QVBoxLayout(m_stepKindGroup);
    m_stepKindEditor = new ActionKindEditor(m_stepKindGroup);
    kindLayout->addWidget(m_stepKindEditor);
    m_stepKindGroup->setEnabled(false);
    scrollLayout->addWidget(m_stepKindGroup);
    // Keep ②'s step label (action list, count, badge) in sync as the user
    // edits, rather than only on the next selection change/add/remove
    // (SPEC.md 6.2/6.4): flushActionParamsEditor() already re-writes the
    // list item's text after applying the current widget values.
    connect(m_stepKindEditor, &ActionKindEditor::changed, this, &MainWindow::flushActionParamsEditor);

    // --- ActionParams: shared defaults, or a per-step override toggled as
    // a whole (unchanged from before -- SPEC.md 6.4/6.9).
    m_actionParamsGroup = new QGroupBox(
        QStringLiteral("操作の詳細設定（ドラッグ距離・キー文字種・スクロール量など）"), scrollContent);
    auto *layout = new QVBoxLayout(m_actionParamsGroup);

    auto *modeRow = new QHBoxLayout;
    m_stepUseDefaultParamsRadio = new QRadioButton(QStringLiteral("デフォルトを使う"), m_actionParamsGroup);
    m_stepUseCustomParamsRadio =
        new QRadioButton(QStringLiteral("このステップ専用の設定を使う"), m_actionParamsGroup);
    m_stepUseDefaultParamsRadio->setChecked(true);
    m_stepUseDefaultParamsRadio->setEnabled(false);
    m_stepUseCustomParamsRadio->setEnabled(false);
    modeRow->addWidget(m_stepUseDefaultParamsRadio);
    modeRow->addWidget(m_stepUseCustomParamsRadio);
    modeRow->addStretch();
    layout->addLayout(modeRow);
    connect(m_stepUseDefaultParamsRadio, &QRadioButton::toggled, this,
            &MainWindow::onStepParamsModeChanged);

    m_actionParamsEditor = new ActionParamsEditor(m_actionParamsGroup);
    layout->addWidget(m_actionParamsEditor, 1);

    scrollLayout->addWidget(m_actionParamsGroup, 1);

    wrapperLayout->addWidget(scroll, 1);
    return wrapper;
}

void MainWindow::onRefreshTargets()
{
    m_targetCombo->clear();
    m_windows = PlatformAutomation::listWindows();
    for (const WindowInfo &w : m_windows) {
        const QString label = QStringLiteral("%1 (pid %2)%3")
                                   .arg(w.appName.isEmpty() ? QStringLiteral("不明なアプリ") : w.appName)
                                   .arg(w.pid)
                                   .arg(w.title.isEmpty() ? QString() : QStringLiteral(" - ") + w.title);
        m_targetCombo->addItem(label);
    }
    if (m_targetCombo->count() == 0)
        m_targetCombo->addItem(QStringLiteral("(ウィンドウが見つかりません)"));

    refreshPermissionLabel();
}

void MainWindow::refreshPermissionLabel()
{
    const bool trusted = PlatformAutomation::isAccessibilityTrusted(false);
    m_permissionLabel->setText(trusted
                                    ? QStringLiteral("✓ 入力送信の権限は許可されています")
                                    : QStringLiteral("✗ 権限が必要です（下のボタンから設定を開いてください）"));
    m_permissionLabel->setStyleSheet(trusted ? "color: green;" : "color: red;");
}

QString MainWindow::describeStep(const RegionStep &step, int index) const
{
    if (step.isWaitStep) {
        const QString runningPrefix =
            index == m_currentRunningStepIndex ? QStringLiteral("▶ 実行中 ") : QString();
        return QStringLiteral("%1ステップ%2: 待機（%3 ms）")
            .arg(runningPrefix)
            .arg(index + 1)
            .arg(step.waitDurationMs);
    }

    QStringList actions;
    if (step.enableClick)
        actions << QStringLiteral("クリック");
    if (step.enableDoubleClick)
        actions << QStringLiteral("ダブルクリック");
    if (step.enableDrag)
        actions << QStringLiteral("ドラッグ");
    if (step.enableKey)
        actions << QStringLiteral("キー入力");
    if (step.enableScrollUp)
        actions << QStringLiteral("スクロール(上)");
    if (step.enableScrollDown)
        actions << QStringLiteral("スクロール(下)");
    if (step.enableScrollHorizontal)
        actions << QStringLiteral("スクロール(横)");
    if (step.enableShortcut)
        actions << QStringLiteral("ショートカット");
    if (step.enableWindowOp)
        actions << QStringLiteral("ウィンドウ操作");

    const QString regionDesc = step.useWholeWindow
                                    ? QStringLiteral("対象GUIの全領域")
                                    : (step.regionName.isEmpty()
                                           ? QStringLiteral("(未選択)")
                                           : QStringLiteral("操作領域「%1」").arg(step.regionName));
    const QString paramsBadge =
        step.useDefaultActionParams ? QString() : QStringLiteral(" | [カスタム設定]");
    const QString runningPrefix =
        index == m_currentRunningStepIndex ? QStringLiteral("▶ 実行中 ") : QString();

    return QStringLiteral("%1ステップ%2: %3 | 操作: %4 | 回数: %5%6")
        .arg(runningPrefix)
        .arg(index + 1)
        .arg(regionDesc)
        .arg(actions.isEmpty() ? QStringLiteral("(なし)") : actions.join(QStringLiteral(", ")))
        .arg(step.actionCount)
        .arg(paramsBadge);
}

void MainWindow::refreshStepList()
{
    m_stepListWidget->clear();
    for (int i = 0; i < m_steps.size(); ++i)
        m_stepListWidget->addItem(describeStep(m_steps[i], i));
}

QString MainWindow::describeNamedRegion(const NamedRegion &region) const
{
    return QStringLiteral("%1（矩形%2個・除外%3個）")
        .arg(region.name)
        .arg(region.regions.size())
        .arg(region.excludeRegions.size());
}

void MainWindow::refreshNamedRegionList()
{
    m_namedRegionListWidget->clear();
    for (const NamedRegion &region : m_namedRegions)
        m_namedRegionListWidget->addItem(describeNamedRegion(region));
}

QStringList MainWindow::stepsReferencing(const QString &regionName) const
{
    QStringList result;
    for (int i = 0; i < m_steps.size(); ++i) {
        if (!m_steps[i].useWholeWindow && m_steps[i].regionName == regionName)
            result << QStringLiteral("ステップ%1").arg(i + 1);
    }
    return result;
}

QString MainWindow::generateDefaultRegionName() const
{
    for (int n = 1;; ++n) {
        // "操作領域N" (not just "領域N") so this doesn't read the same as
        // the "領域N"/"除外N" labels NamedRegionEditorDialog gives the
        // individual rectangles drawn inside one operation region -- those
        // are a different, unrelated numbering scope and having both say
        // "領域1" was confusing (SPEC.md 6.3).
        const QString candidate = QStringLiteral("操作領域%1").arg(n);
        bool used = false;
        for (const NamedRegion &existing : m_namedRegions) {
            if (existing.name == candidate) {
                used = true;
                break;
            }
        }
        if (!used)
            return candidate;
    }
}

void MainWindow::onAddNamedRegion()
{
    NamedRegion initial;
    initial.name = generateDefaultRegionName();
    // NamedRegionEditorDialog visualizes the region being built on screen
    // itself for the duration it's open (SPEC.md 6.3) -- MainWindow no
    // longer shows any on-screen highlight from the list selection.
    NamedRegionEditorDialog dialog(initial, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const NamedRegion region = dialog.result();
    for (const NamedRegion &existing : m_namedRegions) {
        if (existing.name == region.name) {
            QMessageBox::warning(this, QStringLiteral("入力エラー"),
                                  QStringLiteral("同じ名前の操作領域が既に存在します。"));
            return;
        }
    }
    m_namedRegions.append(region);
    refreshNamedRegionList();
    m_namedRegionListWidget->setCurrentRow(m_namedRegions.size() - 1);
}

void MainWindow::onEditSelectedNamedRegion()
{
    const int row = m_namedRegionListWidget->currentRow();
    if (row < 0 || row >= m_namedRegions.size())
        return;
    const QString oldName = m_namedRegions[row].name;

    NamedRegionEditorDialog dialog(m_namedRegions[row], this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const NamedRegion region = dialog.result();

    for (int i = 0; i < m_namedRegions.size(); ++i) {
        if (i != row && m_namedRegions[i].name == region.name) {
            QMessageBox::warning(this, QStringLiteral("入力エラー"),
                                  QStringLiteral("同じ名前の操作領域が既に存在します。"));
            return;
        }
    }

    m_namedRegions[row] = region;
    if (oldName != region.name) {
        // Keep steps that referenced the old name pointing at the same
        // region rather than silently breaking them.
        for (RegionStep &step : m_steps) {
            if (!step.useWholeWindow && step.regionName == oldName)
                step.regionName = region.name;
        }
        // refreshStepList() clears and re-adds all items, which drops the
        // list's current selection -- restore it so column ③'s kind/
        // ActionParams editors (keyed on that selection) aren't reset.
        const int selectedStepRow = m_stepListWidget->currentRow();
        refreshStepList();
        if (selectedStepRow >= 0 && selectedStepRow < m_steps.size())
            m_stepListWidget->setCurrentRow(selectedStepRow);
    }
    refreshNamedRegionList();
    // refreshNamedRegionList() clears and re-adds all items, dropping the
    // selection -- restore it so the just-edited region stays selected.
    m_namedRegionListWidget->setCurrentRow(row);
}

void MainWindow::onRemoveSelectedNamedRegion()
{
    const int row = m_namedRegionListWidget->currentRow();
    if (row < 0 || row >= m_namedRegions.size())
        return;

    const QStringList referencingSteps = stepsReferencing(m_namedRegions[row].name);
    if (!referencingSteps.isEmpty()) {
        QMessageBox::warning(
            this, QStringLiteral("削除できません"),
            QStringLiteral("この操作領域は次のステップで使われているため削除できません: %1\n"
                            "先にそれらのステップの領域を変更するか、ステップを削除してください。")
                .arg(referencingSteps.join(QStringLiteral(", "))));
        return;
    }

    m_namedRegions.removeAt(row);
    refreshNamedRegionList();
}

void MainWindow::flushActionParamsEditor()
{
    if (!m_actionParamsEditor)
        return;
    const ActionParams p = m_actionParamsEditor->params();
    if (m_lastEditedStepRow < 0 || m_lastEditedStepRow >= m_steps.size()) {
        m_defaultActionParams = p;
        return;
    }

    RegionStep &step = m_steps[m_lastEditedStepRow];
    if (!step.useDefaultActionParams)
        step.customActionParams = p;
    else
        m_defaultActionParams = p;

    // Step-level enabled kinds / weights / action count (SPEC.md 6.2/6.3):
    // only meaningful for a selected step, which is guaranteed here by the
    // early return above.
    m_stepKindEditor->applyKindsTo(step);

    if (auto *item = m_stepListWidget->item(m_lastEditedStepRow))
        item->setText(describeStep(step, m_lastEditedStepRow));
}

void MainWindow::loadActionParamsEditorForSelection()
{
    const int row = m_stepListWidget->currentRow();
    if (row < 0 || row >= m_steps.size()) {
        m_actionParamsContextLabel->setText(QStringLiteral("デフォルト値を編集中（ステップ未選択）"));
        m_stepUseDefaultParamsRadio->blockSignals(true);
        m_stepUseDefaultParamsRadio->setChecked(true);
        m_stepUseDefaultParamsRadio->blockSignals(false);
        m_stepUseDefaultParamsRadio->setEnabled(false);
        m_stepUseCustomParamsRadio->setEnabled(false);
        m_actionParamsEditor->setParams(m_defaultActionParams);
        m_stepKindGroup->setEnabled(false);
        m_lastEditedStepRow = -1;
        return;
    }

    if (m_steps[row].isWaitStep) {
        // A wait step has no region/action-kind/ActionParams fields to
        // edit here at all (see RegionStep::isWaitStep) -- disable both
        // groups entirely rather than showing controls that don't apply.
        m_actionParamsContextLabel->setText(
            QStringLiteral("ステップ %1 は待機ステップです（操作パラメータはありません）").arg(row + 1));
        m_stepUseDefaultParamsRadio->blockSignals(true);
        m_stepUseDefaultParamsRadio->setChecked(true);
        m_stepUseDefaultParamsRadio->blockSignals(false);
        m_stepUseDefaultParamsRadio->setEnabled(false);
        m_stepUseCustomParamsRadio->setEnabled(false);
        m_actionParamsEditor->setParams(m_defaultActionParams);
        m_stepKindGroup->setEnabled(false);
        m_lastEditedStepRow = -1;
        return;
    }

    const RegionStep &step = m_steps[row];
    m_actionParamsContextLabel->setText(QStringLiteral("ステップ %1 の操作種別・詳細設定を編集中").arg(row + 1));
    m_stepUseDefaultParamsRadio->setEnabled(true);
    m_stepUseCustomParamsRadio->setEnabled(true);
    m_stepUseDefaultParamsRadio->blockSignals(true);
    m_stepUseCustomParamsRadio->blockSignals(true);
    if (step.useDefaultActionParams)
        m_stepUseDefaultParamsRadio->setChecked(true);
    else
        m_stepUseCustomParamsRadio->setChecked(true);
    m_stepUseDefaultParamsRadio->blockSignals(false);
    m_stepUseCustomParamsRadio->blockSignals(false);

    m_lastEditedStepRow = row;
    m_actionParamsEditor->setParams(effectiveParamsOf(step, m_defaultActionParams));

    m_stepKindGroup->setEnabled(true);
    m_stepKindEditor->setKinds(step);
}

void MainWindow::onStepSelectionChanged()
{
    flushActionParamsEditor();
    loadActionParamsEditorForSelection();
}

void MainWindow::onStepParamsModeChanged()
{
    const int row = m_stepListWidget->currentRow();
    if (row < 0 || row >= m_steps.size())
        return;

    const bool useDefault = m_stepUseDefaultParamsRadio->isChecked();
    if (m_steps[row].useDefaultActionParams == useDefault)
        return;

    flushActionParamsEditor();
    m_steps[row].useDefaultActionParams = useDefault;
    if (auto *item = m_stepListWidget->item(row))
        item->setText(describeStep(m_steps[row], row));

    m_lastEditedStepRow = row;
    m_actionParamsEditor->setParams(effectiveParamsOf(m_steps[row], m_defaultActionParams));
}

void MainWindow::onEditDefaultParams()
{
    // Flush any pending edits first so the dialog opens with the latest
    // values (relevant if the panel is currently showing defaults in-place,
    // i.e. no step selected).
    flushActionParamsEditor();

    DefaultActionParamsDialog dialog(m_defaultActionKinds, m_defaultActionParams, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_defaultActionKinds = dialog.resultKinds();
    m_defaultActionParams = dialog.resultParams();

    // If the panel is currently displaying the defaults (no step selected,
    // or the selected step uses them), refresh it so it doesn't keep
    // showing stale pre-dialog values until the selection next changes.
    const bool showingDefaults = m_lastEditedStepRow < 0 || m_lastEditedStepRow >= m_steps.size() ||
                                  m_steps[m_lastEditedStepRow].useDefaultActionParams;
    if (showingDefaults)
        m_actionParamsEditor->setParams(m_defaultActionParams);
}

void MainWindow::onAddStep()
{
    StepEditorDialog dialog(RegionStep(), m_namedRegions, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.useWholeWindow() && dialog.regionName().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("ステップの設定エラー"),
                              QStringLiteral("操作領域が選択されていません。"));
        return;
    }

    flushActionParamsEditor();
    RegionStep step = m_defaultActionKinds;  // seed kinds/weights/count from the default preset
    step.useWholeWindow = dialog.useWholeWindow();
    step.regionName = dialog.regionName();
    m_steps.append(step);
    refreshStepList();
    // Select the new step so its kinds/weights/count and ActionParams can
    // be configured right away in ③操作パラメータ.
    m_stepListWidget->setCurrentRow(m_steps.size() - 1);
}

void MainWindow::onAddWaitStep()
{
    bool ok = false;
    const int ms = QInputDialog::getInt(this, QStringLiteral("待機ステップを追加"),
                                         QStringLiteral("待機時間 (ms):"), 1000, 1, 600000, 100, &ok);
    if (!ok)
        return;

    flushActionParamsEditor();
    RegionStep step;
    step.isWaitStep = true;
    step.waitDurationMs = ms;
    m_steps.append(step);
    refreshStepList();
    m_stepListWidget->setCurrentRow(m_steps.size() - 1);
}

void MainWindow::onEditSelectedStep()
{
    const int row = m_stepListWidget->currentRow();
    if (row < 0 || row >= m_steps.size())
        return;

    if (m_steps[row].isWaitStep) {
        bool ok = false;
        const int ms = QInputDialog::getInt(this, QStringLiteral("待機ステップを編集"),
                                             QStringLiteral("待機時間 (ms):"),
                                             m_steps[row].waitDurationMs, 1, 600000, 100, &ok);
        if (!ok)
            return;
        m_steps[row].waitDurationMs = ms;
        refreshStepList();
        m_stepListWidget->setCurrentRow(row);
        return;
    }

    StepEditorDialog dialog(m_steps[row], m_namedRegions, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.useWholeWindow() && dialog.regionName().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("ステップの設定エラー"),
                              QStringLiteral("操作領域が選択されていません。"));
        return;
    }

    // This dialog only changes which region the step operates in; its
    // kinds/weights/count and ActionParams (edited in column ③) are left
    // untouched.
    m_steps[row].useWholeWindow = dialog.useWholeWindow();
    m_steps[row].regionName = dialog.regionName();
    refreshStepList();
    m_stepListWidget->setCurrentRow(row);
}

void MainWindow::onRemoveSelectedStep()
{
    const int row = m_stepListWidget->currentRow();
    if (row < 0 || row >= m_steps.size())
        return;
    flushActionParamsEditor();
    m_steps.removeAt(row);
    refreshStepList();
    const int newRow = qMin(row, m_steps.size() - 1);
    if (newRow >= 0)
        m_stepListWidget->setCurrentRow(newRow);
    else
        loadActionParamsEditorForSelection();
}

void MainWindow::onMoveStepUp()
{
    const int row = m_stepListWidget->currentRow();
    if (row > 0 && row < m_steps.size()) {
        flushActionParamsEditor();
        m_steps.move(row, row - 1);
        refreshStepList();
        m_stepListWidget->setCurrentRow(row - 1);
    }
}

void MainWindow::onMoveStepDown()
{
    const int row = m_stepListWidget->currentRow();
    if (row >= 0 && row < m_steps.size() - 1) {
        flushActionParamsEditor();
        m_steps.move(row, row + 1);
        refreshStepList();
        m_stepListWidget->setCurrentRow(row + 1);
    }
}

void MainWindow::onClearSteps()
{
    flushActionParamsEditor();
    m_steps.clear();
    refreshStepList();
    loadActionParamsEditorForSelection();
}

TestConfig MainWindow::buildConfigFromUi(bool &ok, QString &errorMessage) const
{
    ok = false;
    TestConfig config;

    const int idx = m_targetCombo->currentIndex();
    if (idx < 0 || idx >= m_windows.size()) {
        errorMessage = QStringLiteral("対象ウィンドウを選択してください。");
        return config;
    }
    const WindowInfo &target = m_windows[idx];
    config.targetPid = target.pid;
    config.targetWindowId = target.windowId;
    config.targetAppName = target.appName;

    if (m_steps.isEmpty()) {
        errorMessage = QStringLiteral("ステップを最低1つ追加してください。");
        return config;
    }
    config.steps = m_steps;
    config.namedRegions = m_namedRegions;
    config.defaultActionParams = m_defaultActionParams;

    for (int i = 0; i < m_steps.size(); ++i) {
        const RegionStep &step = m_steps[i];
        if (step.isWaitStep)
            continue;  // no region/action-kind/ActionParams fields to validate
        if (!step.hasAnyActionEnabled()) {
            errorMessage = QStringLiteral(
                "ステップ%1は操作種別が選択されていません。②でこのステップを選択し、③操作パラメータ"
                "パネルで操作種別を1つ以上有効にしてください。")
                               .arg(i + 1);
            return config;
        }
        const ActionParams &params = effectiveParamsOf(step, m_defaultActionParams);
        if (step.enableKey && params.allowedKeyChars.isEmpty()) {
            errorMessage = QStringLiteral("キー入力を有効にしたステップがあります。使用文字を指定してください"
                                            "（デフォルトまたはそのステップの専用設定）。");
            return config;
        }
        if (step.enableShortcut && params.shortcutSequences.isEmpty()) {
            errorMessage = QStringLiteral(
                "ショートカットキーを有効にしたステップがあります。ショートカットを最低1つ追加してください"
                "（デフォルトまたはそのステップの専用設定）。");
            return config;
        }
        if (step.enableClick && step.enableRightClick && params.enableContextMenuSelection) {
            if (params.contextMenuSelectionMode == ContextMenuSelectionMode::ByName &&
                params.contextMenuItemNames.isEmpty()) {
                errorMessage = QStringLiteral(
                    "メニュー項目選択（項目名指定）を有効にしたステップがあります。候補項目名を最低1つ追加してください"
                    "（デフォルトまたはそのステップの専用設定）。");
                return config;
            }
            if (params.contextMenuSelectionMode == ContextMenuSelectionMode::ByIndex &&
                params.contextMenuIndices.isEmpty()) {
                errorMessage = QStringLiteral(
                    "メニュー項目選択（番号指定）を有効にしたステップがあります。候補の番号を最低1つ追加してください"
                    "（デフォルトまたはそのステップの専用設定）。");
                return config;
            }
        }
    }

    if (m_intervalModeRateRadio->isChecked()) {
        // Rate (ops/sec) is the reciprocal of the interval (ms): the
        // fastest rate (max回/秒) gives the shortest interval, and the
        // slowest rate (min回/秒) gives the longest interval.
        config.minIntervalMs = qMax(1, int(1000.0 / qMax(0.01, m_maxRateSpin->value())));
        config.maxIntervalMs = qMax(1, int(1000.0 / qMax(0.01, m_minRateSpin->value())));
    } else {
        config.minIntervalMs = m_minIntervalSpin->value();
        config.maxIntervalMs = m_maxIntervalSpin->value();
    }
    config.maxIterations = m_maxIterationsSpin->value();
    config.maxDurationSec = m_maxDurationSecSpin->value();
    config.maxSequenceLoops = m_maxSequenceLoopsSpin->value();
    config.keepTargetActive = m_keepActiveCheck->isChecked();
    config.rngSeed = quint32(m_rngSeedSpin->value());

    ok = true;
    return config;
}

void MainWindow::setControlsEnabled(bool enabled)
{
    m_targetGroup->setEnabled(enabled);
    m_namedRegionGroup->setEnabled(enabled);
    m_stepsGroup->setEnabled(enabled);
    m_actionParamsGroup->setEnabled(enabled);
    const int selectedStepRow = m_stepListWidget->currentRow();
    const bool kindGroupApplicable =
        selectedStepRow >= 0 && selectedStepRow < m_steps.size() && !m_steps[selectedStepRow].isWaitStep;
    m_stepKindGroup->setEnabled(enabled && kindGroupApplicable);
    m_timingGroup->setEnabled(enabled);
    m_startButton->setEnabled(enabled);
    m_stopButton->setEnabled(!enabled);
    m_pauseResumeButton->setEnabled(!enabled);
    if (enabled) {
        m_pauseResumeButton->setText(QStringLiteral("‖ 一時停止"));
        m_resourceUsageLabel->clear();
    }
}

void MainWindow::onStart()
{
    if (!PlatformAutomation::isAccessibilityTrusted(true)) {
        QMessageBox::warning(this, QStringLiteral("権限が必要です"),
                              QStringLiteral("他のアプリケーションを操作するための権限が許可されていません。"
                                              "設定を許可してから、もう一度「開始」を押してください。"));
        onRefreshTargets();
        return;
    }

    flushActionParamsEditor();

    bool ok = false;
    QString errorMessage;
    const TestConfig config = buildConfigFromUi(ok, errorMessage);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("設定エラー"), errorMessage);
        return;
    }

    // Preflight safety self-test: RandomActionEngine refuses to operate
    // anything it can't positively confirm is the target (see SPEC.md
    // 6.7), so make sure that confirmation actually works *before*
    // starting, rather than have the run immediately self-stop with a
    // confusing message one action in. A mismatch here means either the
    // target window isn't currently reachable (covered, minimized, not
    // frontmost) or -- on some window managers, mainly on Linux -- the
    // safety-check APIs themselves aren't supported (see SPEC.md 8章).
    QRect targetBounds;
    if (PlatformAutomation::queryWindowBounds(config.targetWindowId, config.targetPid, targetBounds)) {
        PlatformAutomation::activateProcess(config.targetPid);
        if (PlatformAutomation::windowPidAtPoint(targetBounds.center()) != config.targetPid) {
            QMessageBox::warning(
                this, QStringLiteral("安全確認に失敗しました"),
                QStringLiteral(
                    "対象ウィンドウが安全に操作できることを確認できなかったため、開始できません。\n\n"
                    "対象ウィンドウが他のウィンドウに覆われていないか、最小化されていないか確認して"
                    "ください。それでも解決しない場合、この環境（特にLinuxの一部のウィンドウマネージャ）"
                    "では安全チェック機能自体が動作しない可能性があります（詳細はSPEC.md参照）。"));
            return;
        }
    }

    setControlsEnabled(false);
    m_statusLabel->setText(QStringLiteral("実行中"));
    m_runElapsed.restart();
    m_uiTimer->start();

    m_stopPanel = new StopPanel();
    connect(m_stopPanel, &StopPanel::stopRequested, this, &MainWindow::onStop);
    m_stopPanel->show();

    m_logView->clear();
    m_engine->start(config);
}

void MainWindow::onStop()
{
    m_engine->stop();
}

void MainWindow::onPauseResume()
{
    if (m_engine->isPaused())
        m_engine->resume();
    else
        m_engine->pause();
}

void MainWindow::onEnginePausedChanged(bool paused)
{
    m_pauseResumeButton->setText(paused ? QStringLiteral("▶ 再開") : QStringLiteral("‖ 一時停止"));
    m_statusLabel->setText(paused ? QStringLiteral("一時停止中") : QStringLiteral("実行中"));
}

void MainWindow::onResourceUsageUpdated(double residentMemoryMB, double cpuPercent)
{
    m_resourceUsageLabel->setText(
        QStringLiteral("メモリ: %1 MB / CPU: %2%")
            .arg(residentMemoryMB, 0, 'f', 1)
            .arg(cpuPercent, 0, 'f', 1));
}

void MainWindow::onEngineFinished(const QString &reason)
{
    setControlsEnabled(true);
    m_statusLabel->setText(QStringLiteral("停止: %1").arg(reason));
    m_uiTimer->stop();
    if (m_stopPanel) {
        m_stopPanel->close();
        m_stopPanel->deleteLater();
    }

    if (m_currentRunningStepIndex >= 0 && m_currentRunningStepIndex < m_steps.size()) {
        if (auto *item = m_stepListWidget->item(m_currentRunningStepIndex))
            item->setText(describeStep(m_steps[m_currentRunningStepIndex], m_currentRunningStepIndex));
    }
    m_currentRunningStepIndex = -1;
}

void MainWindow::onActionLog(const QString &message)
{
    appendLog(message);
}

void MainWindow::appendLog(const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_logView->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, message));
}

void MainWindow::onIterationCountChanged(qint64 count)
{
    m_iterationLabel->setText(QStringLiteral("実行回数: %1").arg(count));
    if (m_stopPanel)
        m_stopPanel->setIterationCount(count);
}

void MainWindow::onCurrentStepChanged(int index)
{
    if (index == m_currentRunningStepIndex)
        return;
    // Update just the two affected rows' text in place (not a full
    // refreshStepList(), which would clear/restore the list's current
    // selection and needlessly re-trigger onStepSelectionChanged while a
    // run is in progress).
    const int previous = m_currentRunningStepIndex;
    m_currentRunningStepIndex = index;
    if (previous >= 0 && previous < m_steps.size()) {
        if (auto *item = m_stepListWidget->item(previous))
            item->setText(describeStep(m_steps[previous], previous));
    }
    if (index >= 0 && index < m_steps.size()) {
        if (auto *item = m_stepListWidget->item(index)) {
            item->setText(describeStep(m_steps[index], index));
            m_stepListWidget->scrollToItem(item);
        }
    }
}

void MainWindow::updateElapsedLabel()
{
    const qint64 secs = m_runElapsed.elapsed() / 1000;
    m_elapsedLabel->setText(QStringLiteral("経過: %1秒").arg(secs));
}

void MainWindow::onClearLog()
{
    m_logView->clear();
}

void MainWindow::onSaveLog()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("ログを保存"), QString(),
                                                        QStringLiteral("テキストファイル (*.txt)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(m_logView->toPlainText().toUtf8());
        file.close();
    }
}

void MainWindow::onOpenAccessibilitySettings()
{
    PlatformAutomation::openAccessibilitySettings();
}
