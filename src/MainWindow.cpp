#include "MainWindow.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "ActionKindEditor.h"
#include "ActionParamsEditor.h"
#include "DefaultActionParamsDialog.h"
#include "NamedRegionEditorDialog.h"
#include "RegionSelectorOverlay.h"
#include "SetupActionEditorDialog.h"
#include "StatisticsDialog.h"
#include "StepEditorDialog.h"
#include "StepGroupEditorDialog.h"
#include "StopPanel.h"
#include "TestConfigJson.h"
#include "platform/GlobalHotkey.h"
#include "I18n.h"

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
    connect(m_engine, &RandomActionEngine::summaryReady, this, &MainWindow::onRunSummaryReady);
    connect(m_engine, &RandomActionEngine::regionScreenshotCaptured, this,
            &MainWindow::onRegionScreenshotCaptured);

    m_uiTimer = new QTimer(this);
    m_uiTimer->setInterval(500);
    connect(m_uiTimer, &QTimer::timeout, this, &MainWindow::updateElapsedLabel);

    // SPEC.md 10 ①: polls for the target app to reappear between batch
    // runs (waitForTargetThenContinueBatch()/onBatchWaitTick()). 1-second
    // cadence is frequent enough to feel responsive without meaningfully
    // taxing a CPU that's otherwise idle while nothing runs.
    m_batchWaitTimer = new QTimer(this);
    m_batchWaitTimer->setInterval(1000);
    connect(m_batchWaitTimer, &QTimer::timeout, this, &MainWindow::onBatchWaitTick);

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

    m_stats.reload();

    buildUi();
    onRefreshTargets();

    // SPEC.md 6.7/10: a global hotkey backstop for the floating StopPanel
    // button, in case the target has captured input in a way that makes
    // even that panel hard to reach. Best-effort -- if the combo can't be
    // grabbed (e.g. already used by the desktop environment), the app
    // simply continues without it; the panel/main-window buttons remain
    // the primary way to stop a run either way.
    m_globalHotkey = new GlobalHotkey(this);
    connect(m_globalHotkey, &GlobalHotkey::triggered, this, &MainWindow::onGlobalEmergencyStop);
    appendLog(m_globalHotkey->start()
                  ? I18n::t(QStringLiteral("グローバル緊急停止ホットキー（Ctrl+Alt+Shift+Esc）を登録しました。"))
                  : I18n::t(QStringLiteral("グローバル緊急停止ホットキーを登録できませんでした"
                                    "（他のアプリが同じ組み合わせを使用している可能性があります）。"
                                    "「■ 停止」ボタンは通常どおり使用できます。")));
}

void MainWindow::buildUi()
{
    setWindowTitle(I18n::t(QStringLiteral("EnduranceTestGUI - GUI耐久テストツール")));

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
    m_startButton = new QPushButton(I18n::t(QStringLiteral("▶ 開始")), central);
    m_stopButton = new QPushButton(I18n::t(QStringLiteral("■ 停止")), central);
    m_stopButton->setEnabled(false);
    m_pauseResumeButton = new QPushButton(I18n::t(QStringLiteral("‖ 一時停止")), central);
    m_pauseResumeButton->setEnabled(false);
    m_statusLabel = new QLabel(I18n::t(QStringLiteral("待機中")), central);
    controlsRow->addWidget(m_startButton);
    controlsRow->addWidget(m_stopButton);
    controlsRow->addWidget(m_pauseResumeButton);
    controlsRow->addWidget(m_statusLabel);
    controlsRow->addStretch();

    // SPEC.md 10 ①: repeat the whole test automatically to help reproduce a
    // crash that only happens some fraction of the time -- 1 (default)
    // means "no batching", same as before this feature existed.
    controlsRow->addWidget(new QLabel(I18n::t(QStringLiteral("連続実行回数:")), central));
    m_batchRunCountSpin = new QSpinBox(central);
    m_batchRunCountSpin->setRange(1, 100000);
    m_batchRunCountSpin->setValue(1);
    m_batchRunCountSpin->setToolTip(
        I18n::t(QStringLiteral("1より大きい値にすると、1回終わるたびに（対象アプリの再起動を待って）"
                                "自動的に次を開始し、指定回数繰り返します。")));
    controlsRow->addWidget(m_batchRunCountSpin);
    m_batchProgressLabel = new QLabel(central);
    controlsRow->addWidget(m_batchProgressLabel);
    bottomLayout->addLayout(controlsRow);

    auto *progressRow = new QHBoxLayout;
    m_resourceUsageLabel = new QLabel(QString(), central);
    m_elapsedLabel = new QLabel(I18n::t(QStringLiteral("経過: 0秒")), central);
    m_iterationLabel = new QLabel(I18n::t(QStringLiteral("実行回数: 0")), central);
    progressRow->addWidget(m_resourceUsageLabel);
    progressRow->addStretch();
    progressRow->addWidget(m_elapsedLabel);
    progressRow->addWidget(m_iterationLabel);
    bottomLayout->addLayout(progressRow);

    // このテスト設定について、これまでに記録された全実行結果からの
    // クラッシュ率の要約（SPEC.md 10 ③）。updateStatisticsDisplay()が
    // 読み込み時・開始直前・実行終了後に更新する。
    auto *statsRow = new QHBoxLayout;
    m_statisticsSummaryLabel = new QLabel(central);
    m_statisticsSummaryLabel->setWordWrap(true);
    m_showStatisticsButton = new QPushButton(I18n::t(QStringLiteral("統計...")), central);
    statsRow->addWidget(m_statisticsSummaryLabel, 1);
    statsRow->addWidget(m_showStatisticsButton);
    bottomLayout->addLayout(statsRow);
    connect(m_showStatisticsButton, &QPushButton::clicked, this, &MainWindow::onShowStatistics);

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_pauseResumeButton, &QPushButton::clicked, this, &MainWindow::onPauseResume);

    // --- Log ---
    auto *logGroup = new QGroupBox(I18n::t(QStringLiteral("ログ")), central);
    auto *logLayout = new QVBoxLayout(logGroup);
    m_logView = new QPlainTextEdit(logGroup);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(5000);
    logLayout->addWidget(m_logView);
    auto *logButtonsRow = new QHBoxLayout;
    auto *clearLogButton = new QPushButton(I18n::t(QStringLiteral("ログをクリア")), logGroup);
    auto *saveLogButton = new QPushButton(I18n::t(QStringLiteral("ログを保存...")), logGroup);
    m_saveSummaryButton = new QPushButton(I18n::t(QStringLiteral("実行結果サマリーを保存...")), logGroup);
    m_saveSummaryButton->setEnabled(false);
    m_saveSummaryButton->setToolTip(I18n::t(QStringLiteral("テストを一度実行すると保存できるようになります。")));
    m_saveRegionScreenshotButton = new QPushButton(I18n::t(QStringLiteral("操作領域画像を保存...")), logGroup);
    m_saveRegionScreenshotButton->setEnabled(false);
    m_saveRegionScreenshotButton->setToolTip(
        I18n::t(QStringLiteral("テスト実行中に操作領域のスクリーンショットが撮影されると保存できるようになります。")));
    logButtonsRow->addWidget(clearLogButton);
    logButtonsRow->addWidget(saveLogButton);
    logButtonsRow->addWidget(m_saveSummaryButton);
    logButtonsRow->addWidget(m_saveRegionScreenshotButton);
    logButtonsRow->addStretch();
    logLayout->addLayout(logButtonsRow);
    connect(clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLog);
    connect(saveLogButton, &QPushButton::clicked, this, &MainWindow::onSaveLog);
    connect(m_saveSummaryButton, &QPushButton::clicked, this, &MainWindow::onSaveSummary);
    connect(m_saveRegionScreenshotButton, &QPushButton::clicked, this,
            &MainWindow::onSaveRegionScreenshot);

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
    updateStatisticsDisplay();

    resize(1200, 900);
}

void MainWindow::buildMenuBar()
{
    // SPEC.md 10: save/load the whole editable test setup as a reusable
    // preset file, so the same ①②③ configuration doesn't have to be
    // rebuilt by hand for every run (e.g. against a new build of the same
    // target app, or on another machine).
    auto *fileMenu = menuBar()->addMenu(I18n::t(QStringLiteral("ファイル")));
    m_savePresetAction = fileMenu->addAction(I18n::t(QStringLiteral("テスト設定を保存...")));
    connect(m_savePresetAction, &QAction::triggered, this, &MainWindow::onSavePreset);
    m_loadPresetAction = fileMenu->addAction(I18n::t(QStringLiteral("テスト設定を読み込む...")));
    connect(m_loadPresetAction, &QAction::triggered, this, &MainWindow::onLoadPreset);

    // SPEC.md "多言語対応": language takes effect on next launch (I18n.h
    // explains why a live retranslate isn't done), so the handlers below
    // just persist the choice and tell the user to restart.
    auto *languageMenu = menuBar()->addMenu(I18n::t(QStringLiteral("言語 / Language")));
    auto *languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);
    m_languageJapaneseAction = languageMenu->addAction(QStringLiteral("日本語"));
    m_languageJapaneseAction->setCheckable(true);
    languageGroup->addAction(m_languageJapaneseAction);
    connect(m_languageJapaneseAction, &QAction::triggered, this, &MainWindow::onSelectLanguageJapanese);
    m_languageEnglishAction = languageMenu->addAction(QStringLiteral("English"));
    m_languageEnglishAction->setCheckable(true);
    languageGroup->addAction(m_languageEnglishAction);
    connect(m_languageEnglishAction, &QAction::triggered, this, &MainWindow::onSelectLanguageEnglish);
    if (I18n::currentLanguage() == I18n::Language::English)
        m_languageEnglishAction->setChecked(true);
    else
        m_languageJapaneseAction->setChecked(true);

    auto *helpMenu = menuBar()->addMenu(I18n::t(QStringLiteral("ヘルプ")));

    auto *aboutAppAction = helpMenu->addAction(I18n::t(QStringLiteral("EnduranceTestGUIについて...")));
    connect(aboutAppAction, &QAction::triggered, this, &MainWindow::onAboutApp);

    // Required by the Qt LGPLv3/GPL license terms this app is built
    // against: end users must be able to see which license Qt itself is
    // distributed under and read its full text. QMessageBox::aboutQt()
    // is Qt's own standard dialog for this -- it names the exact license
    // and has a "Show License" button with the complete text, so no
    // license text needs to be bundled/reproduced separately here.
    auto *aboutQtAction = helpMenu->addAction(I18n::t(QStringLiteral("Qtについて...")));
    connect(aboutQtAction, &QAction::triggered, this, &MainWindow::onAboutQt);
}

void MainWindow::onAboutApp()
{
    QMessageBox::about(
        this, I18n::t(QStringLiteral("EnduranceTestGUIについて")),
        I18n::t(QStringLiteral("EnduranceTestGUI\n\n"
            "GUIアプリケーションの耐久テスト（ランダムクリック・ランダム操作）を行うための"
            "デスクトップツールです。\n\n"
            "Qt %1 を使用して構築されています。Qtはフリーソフトウェア版の場合"
            "GNU LGPL バージョン3（一部モジュールはGPL）の下で配布されています。Qt自体の"
            "ライセンス条文は、このメニューの「Qtについて...」から確認できます。"))
            .arg(QStringLiteral(QT_VERSION_STR)));
}

void MainWindow::onAboutQt()
{
    QMessageBox::aboutQt(this);
}

void MainWindow::onSelectLanguageJapanese()
{
    I18n::setLanguage(I18n::Language::Japanese);
    QMessageBox::information(this, QStringLiteral("言語 / Language"),
                              QStringLiteral("言語設定を保存しました。変更を反映するにはアプリを再起動してください。\n\n"
                                              "Language preference saved. Please restart the app for the change to take effect."));
}

void MainWindow::onSelectLanguageEnglish()
{
    I18n::setLanguage(I18n::Language::English);
    QMessageBox::information(this, QStringLiteral("言語 / Language"),
                              QStringLiteral("言語設定を保存しました。変更を反映するにはアプリを再起動してください。\n\n"
                                              "Language preference saved. Please restart the app for the change to take effect."));
}

QWidget *MainWindow::buildTargetColumn(QWidget *parent)
{
    auto *wrapper = new QWidget(parent);
    auto *wrapperLayout = new QVBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addWidget(columnHeader(I18n::t(QStringLiteral("① 対象選択")), wrapper));

    auto *scroll = new QScrollArea(wrapper);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    wrapperLayout->addWidget(scroll, 1);

    auto *container = new QWidget;
    scroll->setWidget(container);
    auto *layout = new QVBoxLayout(container);

    // --- Target group ---
    m_targetGroup = new QGroupBox(I18n::t(QStringLiteral("対象 (Target)")), container);
    auto *targetLayout = new QVBoxLayout(m_targetGroup);
    auto *targetRow = new QHBoxLayout;
    m_targetCombo = new QComboBox(m_targetGroup);
    m_refreshButton = new QPushButton(I18n::t(QStringLiteral("更新")), m_targetGroup);
    targetRow->addWidget(m_targetCombo, 1);
    targetRow->addWidget(m_refreshButton);
    targetLayout->addLayout(targetRow);

    // Label above, button below (not side-by-side) so the label has room
    // to wrap instead of squeezing the button when this column is narrow.
    m_permissionLabel = new QLabel(m_targetGroup);
    m_permissionLabel->setWordWrap(true);
    targetLayout->addWidget(m_permissionLabel);
    m_openSettingsButton = new QPushButton(I18n::t(QStringLiteral("権限設定を開く")), m_targetGroup);
    targetLayout->addWidget(m_openSettingsButton);

    // SPEC.md 10 ②: an optional command to (re)launch the target app, used
    // by ①の連続自動実行 (batch mode) when it finds the target gone between
    // runs, and by "今すぐ起動" below for a manual one-off relaunch anytime.
    // Entirely optional -- batch mode works without it too, just waiting
    // for a manual relaunch instead (SPEC.md 10 ①).
    auto *launchCommandLabel =
        new QLabel(I18n::t(QStringLiteral("対象アプリの自動起動コマンド（任意、①の連続実行で使用）:")), m_targetGroup);
    launchCommandLabel->setWordWrap(true);
    targetLayout->addWidget(launchCommandLabel);
    auto *launchCommandRow = new QHBoxLayout;
    m_targetLaunchCommandEdit = new QLineEdit(m_targetGroup);
    m_targetLaunchCommandEdit->setPlaceholderText(
        I18n::t(QStringLiteral("例: /path/to/TestTarget --option")));
    m_browseLaunchCommandButton = new QPushButton(I18n::t(QStringLiteral("参照...")), m_targetGroup);
    launchCommandRow->addWidget(m_targetLaunchCommandEdit, 1);
    launchCommandRow->addWidget(m_browseLaunchCommandButton);
    targetLayout->addLayout(launchCommandRow);
    m_launchTargetNowButton = new QPushButton(I18n::t(QStringLiteral("今すぐ起動")), m_targetGroup);
    targetLayout->addWidget(m_launchTargetNowButton);

    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshTargets);
    connect(m_openSettingsButton, &QPushButton::clicked, this,
            &MainWindow::onOpenAccessibilitySettings);
    connect(m_browseLaunchCommandButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::t(QStringLiteral("対象アプリの実行ファイルを選択")));
        if (!path.isEmpty())
            m_targetLaunchCommandEdit->setText(path);
    });
    connect(m_launchTargetNowButton, &QPushButton::clicked, this,
            &MainWindow::launchTargetAppFromConfiguredCommand);

    layout->addWidget(m_targetGroup);

    // --- Startup setup macro (SPEC.md 6.x) ---
    m_setupActionsGroup = new QGroupBox(
        I18n::t(QStringLiteral("起動時セットアップ（対象アプリ起動直後に一度だけ実行。ログイン等）")), container);
    auto *setupActionsLayout = new QVBoxLayout(m_setupActionsGroup);
    m_setupActionListWidget = new QListWidget(m_setupActionsGroup);
    m_setupActionListWidget->setMaximumHeight(110);
    setupActionsLayout->addWidget(m_setupActionListWidget);
    auto *setupActionsButtonsRow = new QHBoxLayout;
    m_addSetupActionButton = new QPushButton(I18n::t(QStringLiteral("追加...")), m_setupActionsGroup);
    m_editSetupActionButton = new QPushButton(I18n::t(QStringLiteral("編集...")), m_setupActionsGroup);
    m_removeSetupActionButton = new QPushButton(I18n::t(QStringLiteral("削除")), m_setupActionsGroup);
    setupActionsButtonsRow->addWidget(m_addSetupActionButton);
    setupActionsButtonsRow->addWidget(m_editSetupActionButton);
    setupActionsButtonsRow->addWidget(m_removeSetupActionButton);
    setupActionsLayout->addLayout(setupActionsButtonsRow);
    auto *setupActionsOrderRow = new QHBoxLayout;
    m_moveSetupActionUpButton = new QPushButton(I18n::t(QStringLiteral("↑ 上へ")), m_setupActionsGroup);
    m_moveSetupActionDownButton = new QPushButton(I18n::t(QStringLiteral("↓ 下へ")), m_setupActionsGroup);
    setupActionsOrderRow->addWidget(m_moveSetupActionUpButton);
    setupActionsOrderRow->addWidget(m_moveSetupActionDownButton);
    setupActionsLayout->addLayout(setupActionsOrderRow);
    m_setupActionsFirstRunOnlyCheck = new QCheckBox(
        I18n::t(QStringLiteral("連続自動実行（①バッチ）では初回のみ実行する")), m_setupActionsGroup);
    m_setupActionsFirstRunOnlyCheck->setToolTip(
        I18n::t(QStringLiteral("オフの場合、バッチの自動再起動のたびに毎回このセットアップを実行します。"
                        "オンの場合、2回目以降の自動再起動ではスキップします"
                        "（初回だけ出るライセンス同意等を想定）。")));
    setupActionsLayout->addWidget(m_setupActionsFirstRunOnlyCheck);

    connect(m_addSetupActionButton, &QPushButton::clicked, this, &MainWindow::onAddSetupAction);
    connect(m_editSetupActionButton, &QPushButton::clicked, this,
            &MainWindow::onEditSelectedSetupAction);
    connect(m_removeSetupActionButton, &QPushButton::clicked, this,
            &MainWindow::onRemoveSelectedSetupAction);
    connect(m_moveSetupActionUpButton, &QPushButton::clicked, this, &MainWindow::onMoveSetupActionUp);
    connect(m_moveSetupActionDownButton, &QPushButton::clicked, this, &MainWindow::onMoveSetupActionDown);

    layout->addWidget(m_setupActionsGroup);

    // --- Named operation regions ---
    m_namedRegionGroup = new QGroupBox(
        I18n::t(QStringLiteral("操作領域（名前付き。②でステップ作成時に選択して使う）")), container);
    auto *namedRegionLayout = new QVBoxLayout(m_namedRegionGroup);
    m_namedRegionListWidget = new QListWidget(m_namedRegionGroup);
    m_namedRegionListWidget->setMaximumHeight(110);
    namedRegionLayout->addWidget(m_namedRegionListWidget);
    auto *namedRegionButtonsRow = new QHBoxLayout;
    m_addNamedRegionButton = new QPushButton(I18n::t(QStringLiteral("追加...")), m_namedRegionGroup);
    m_editNamedRegionButton = new QPushButton(I18n::t(QStringLiteral("編集...")), m_namedRegionGroup);
    m_removeNamedRegionButton = new QPushButton(I18n::t(QStringLiteral("削除")), m_namedRegionGroup);
    namedRegionButtonsRow->addWidget(m_addNamedRegionButton);
    namedRegionButtonsRow->addWidget(m_editNamedRegionButton);
    namedRegionButtonsRow->addWidget(m_removeNamedRegionButton);
    namedRegionLayout->addLayout(namedRegionButtonsRow);

    connect(m_addNamedRegionButton, &QPushButton::clicked, this, &MainWindow::onAddNamedRegion);
    connect(m_editNamedRegionButton, &QPushButton::clicked, this,
            &MainWindow::onEditSelectedNamedRegion);
    connect(m_removeNamedRegionButton, &QPushButton::clicked, this,
            &MainWindow::onRemoveSelectedNamedRegion);

    // --- Timing group ---
    m_timingGroup = new QGroupBox(I18n::t(QStringLiteral("タイミング・制限")), container);
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
    m_intervalModeMsRadio = new QRadioButton(I18n::t(QStringLiteral("ms指定")), intervalContainer);
    m_intervalModeRateRadio = new QRadioButton(I18n::t(QStringLiteral("回数/秒指定")), intervalContainer);
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
    msPageLayout->addWidget(new QLabel(I18n::t(QStringLiteral("〜")), msPage));
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
    ratePageLayout->addWidget(new QLabel(I18n::t(QStringLiteral("〜")), ratePage));
    ratePageLayout->addWidget(m_maxRateSpin);
    ratePageLayout->addWidget(new QLabel(I18n::t(QStringLiteral("回/秒")), ratePage));
    ratePageLayout->addStretch();
    m_intervalStack->addWidget(ratePage);

    intervalContainerLayout->addWidget(m_intervalStack);
    connect(m_intervalModeRateRadio, &QRadioButton::toggled, this, [this](bool checked) {
        m_intervalStack->setCurrentIndex(checked ? 1 : 0);
    });

    timingForm->addRow(I18n::t(QStringLiteral("操作間隔:")), intervalContainer);

    m_maxIterationsSpin = new QSpinBox(m_timingGroup);
    m_maxIterationsSpin->setRange(1, 100000000);
    m_maxIterationsSpin->setValue(1000);
    timingForm->addRow(I18n::t(QStringLiteral("最大実行回数（全ステップ合計、必須）:")), m_maxIterationsSpin);

    m_maxDurationSecSpin = new QSpinBox(m_timingGroup);
    m_maxDurationSecSpin->setRange(1, 6000000);
    m_maxDurationSecSpin->setValue(120);
    m_maxDurationSecSpin->setSuffix(I18n::t(QStringLiteral(" 秒")));
    timingForm->addRow(I18n::t(QStringLiteral("最大実行時間（必須）:")), m_maxDurationSecSpin);

    m_maxSequenceLoopsSpin = new QSpinBox(m_timingGroup);
    m_maxSequenceLoopsSpin->setRange(1, 100000000);
    m_maxSequenceLoopsSpin->setValue(10);
    timingForm->addRow(I18n::t(QStringLiteral("ステップ全体（シーケンス）の繰り返し回数（必須）:")), m_maxSequenceLoopsSpin);

    m_keepActiveCheck = new QCheckBox(I18n::t(QStringLiteral("対象アプリを常に最前面に保つ")), m_timingGroup);
    m_keepActiveCheck->setChecked(true);
    timingForm->addRow(QString(), m_keepActiveCheck);

    m_rngSeedSpin = new QSpinBox(m_timingGroup);
    m_rngSeedSpin->setRange(0, 2000000000);
    m_rngSeedSpin->setValue(0);
    m_rngSeedSpin->setSpecialValueText(I18n::t(QStringLiteral("ランダム")));
    timingForm->addRow(I18n::t(QStringLiteral("乱数シード（クラッシュ再現用。開始時にログに記録される）:")), m_rngSeedSpin);

    // Operation-region screenshot capture timing (SPEC.md 6.2/10): which of
    // the three radios is checked selects TestConfig::screenshotCaptureMode
    // in buildConfigFromUi(); m_screenshotIntervalSpin only matters for the
    // "一定間隔ごと" radio but stays visible/enabled together with it (no
    // separate stacked page needed -- it's a single extra field, unlike the
    // ms/rate interval switch above).
    auto *screenshotContainer = new QWidget(m_timingGroup);
    auto *screenshotLayout = new QVBoxLayout(screenshotContainer);
    screenshotLayout->setContentsMargins(0, 0, 0, 0);
    m_screenshotModeOnceRadio = new QRadioButton(I18n::t(QStringLiteral("実行前に一度だけ")), screenshotContainer);
    m_screenshotModePerStepRadio =
        new QRadioButton(I18n::t(QStringLiteral("ステップが変わるたび")), screenshotContainer);
    m_screenshotModePerStepRadio->setChecked(true);
    auto *screenshotIntervalRow = new QHBoxLayout;
    m_screenshotModeIntervalRadio = new QRadioButton(I18n::t(QStringLiteral("一定間隔ごと:")), screenshotContainer);
    m_screenshotIntervalSpin = new QSpinBox(screenshotContainer);
    m_screenshotIntervalSpin->setRange(1, 1000000);
    m_screenshotIntervalSpin->setValue(50);
    m_screenshotIntervalSpin->setSuffix(I18n::t(QStringLiteral(" 操作ごと")));
    screenshotIntervalRow->addWidget(m_screenshotModeIntervalRadio);
    screenshotIntervalRow->addWidget(m_screenshotIntervalSpin);
    screenshotIntervalRow->addStretch();
    screenshotLayout->addWidget(m_screenshotModeOnceRadio);
    screenshotLayout->addWidget(m_screenshotModePerStepRadio);
    screenshotLayout->addLayout(screenshotIntervalRow);
    timingForm->addRow(I18n::t(QStringLiteral("操作領域スクリーンショットの撮影タイミング:")), screenshotContainer);

    // Opt-in anomaly diagnostics (SPEC.md 6.7/10) -- see TestConfig::
    // enableScreenRecording/enableCrashDumpCollection.
    m_recordingCheck =
        new QCheckBox(I18n::t(QStringLiteral("異常停止時に直近の画面を録画として保存する（追加負荷あり）")), m_timingGroup);
    m_recordingCheck->setChecked(false);
    m_recordingCheck->setToolTip(
        I18n::t(QStringLiteral("実行中、画面を一定間隔で撮影してリングバッファに保持し続けます。"
                        "異常停止時に、そこまでの数秒間の画面推移を連番PNGとして保存します。")));
    timingForm->addRow(QString(), m_recordingCheck);

    m_crashDumpCollectionCheck =
        new QCheckBox(I18n::t(QStringLiteral("異常停止時にOSのクラッシュダンプ・診断ログを収集する")), m_timingGroup);
    m_crashDumpCollectionCheck->setChecked(true);
    m_crashDumpCollectionCheck->setToolTip(
        I18n::t(QStringLiteral("対象アプリのものと思われるクラッシュレポート/コアダンプがシステム上に"
                        "見つかった場合、そのパスを異常停止時のログと記録一式に含めます。"
                        "見つからなくても実行には影響しません。")));
    timingForm->addRow(QString(), m_crashDumpCollectionCheck);

    // 操作領域とタイミング・制限を横並びに配置する（残りの縦方向の空きは
    // タイミング・制限側の入力欄の折り返し等に使われがちなので、少し広めに割り当てる）。
    auto *namedRegionAndTimingRow = new QHBoxLayout;
    namedRegionAndTimingRow->addWidget(m_namedRegionGroup, 1);
    namedRegionAndTimingRow->addWidget(m_timingGroup, 1);
    layout->addLayout(namedRegionAndTimingRow);
    layout->addStretch();

    return wrapper;
}

QWidget *MainWindow::buildStepsColumn(QWidget *parent)
{
    auto *wrapper = new QWidget(parent);
    auto *wrapperLayout = new QVBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addWidget(columnHeader(I18n::t(QStringLiteral("② ステップ構成")), wrapper));

    m_stepsGroup = new QGroupBox(
        I18n::t(QStringLiteral("領域ごとに操作種別・回数を指定し、順番に繰り返し実行")), wrapper);
    auto *stepsLayout = new QVBoxLayout(m_stepsGroup);
    m_stepListWidget = new QListWidget(m_stepsGroup);
    // Multiple steps can be selected at once so they can be combined into a
    // group (SPEC.md 6.2, m_groupStepsButton below).
    m_stepListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    stepsLayout->addWidget(m_stepListWidget, 1);

    auto *stepButtonsRow = new QHBoxLayout;
    m_addStepButton = new QPushButton(I18n::t(QStringLiteral("追加...")), m_stepsGroup);
    m_addWaitStepButton = new QPushButton(I18n::t(QStringLiteral("待機を追加...")), m_stepsGroup);
    m_editStepButton = new QPushButton(I18n::t(QStringLiteral("編集...")), m_stepsGroup);
    m_removeStepButton = new QPushButton(I18n::t(QStringLiteral("削除")), m_stepsGroup);
    stepButtonsRow->addWidget(m_addStepButton);
    stepButtonsRow->addWidget(m_addWaitStepButton);
    stepButtonsRow->addWidget(m_editStepButton);
    stepButtonsRow->addWidget(m_removeStepButton);
    stepsLayout->addLayout(stepButtonsRow);

    auto *stepOrderRow = new QHBoxLayout;
    m_moveStepUpButton = new QPushButton(I18n::t(QStringLiteral("↑ 上へ")), m_stepsGroup);
    m_moveStepDownButton = new QPushButton(I18n::t(QStringLiteral("↓ 下へ")), m_stepsGroup);
    m_clearStepsButton = new QPushButton(I18n::t(QStringLiteral("すべて削除")), m_stepsGroup);
    stepOrderRow->addWidget(m_moveStepUpButton);
    stepOrderRow->addWidget(m_moveStepDownButton);
    stepOrderRow->addWidget(m_clearStepsButton);
    stepsLayout->addLayout(stepOrderRow);

    // SPEC.md 6.2: select 2+ plain steps and combine them into a group that
    // repeatedly performs one random action from a randomly (weight-)
    // chosen member until a configured total call count is reached, then
    // advances like any other step. "グループ解除" reverses this.
    auto *groupButtonsRow = new QHBoxLayout;
    m_groupStepsButton = new QPushButton(I18n::t(QStringLiteral("グループ化")), m_stepsGroup);
    m_ungroupStepButton = new QPushButton(I18n::t(QStringLiteral("グループ解除")), m_stepsGroup);
    groupButtonsRow->addWidget(m_groupStepsButton);
    groupButtonsRow->addWidget(m_ungroupStepButton);
    stepsLayout->addLayout(groupButtonsRow);

    connect(m_stepListWidget, &QListWidget::currentRowChanged, this,
            &MainWindow::onStepSelectionChanged);
    connect(m_stepListWidget, &QListWidget::itemSelectionChanged, this,
            &MainWindow::updateGroupButtonsEnabled);
    connect(m_addStepButton, &QPushButton::clicked, this, &MainWindow::onAddStep);
    connect(m_addWaitStepButton, &QPushButton::clicked, this, &MainWindow::onAddWaitStep);
    connect(m_editStepButton, &QPushButton::clicked, this, &MainWindow::onEditSelectedStep);
    connect(m_removeStepButton, &QPushButton::clicked, this, &MainWindow::onRemoveSelectedStep);
    connect(m_moveStepUpButton, &QPushButton::clicked, this, &MainWindow::onMoveStepUp);
    connect(m_moveStepDownButton, &QPushButton::clicked, this, &MainWindow::onMoveStepDown);
    connect(m_clearStepsButton, &QPushButton::clicked, this, &MainWindow::onClearSteps);
    connect(m_groupStepsButton, &QPushButton::clicked, this, &MainWindow::onGroupSelectedSteps);
    connect(m_ungroupStepButton, &QPushButton::clicked, this, &MainWindow::onUngroupSelectedStep);

    wrapperLayout->addWidget(m_stepsGroup, 1);
    return wrapper;
}

QWidget *MainWindow::buildActionParamsColumn(QWidget *parent)
{
    auto *wrapper = new QWidget(parent);
    auto *wrapperLayout = new QVBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addWidget(columnHeader(I18n::t(QStringLiteral("③ 操作パラメータ")), wrapper));

    auto *contextRow = new QHBoxLayout;
    m_actionParamsContextLabel = new QLabel(I18n::t(QStringLiteral("デフォルト値を編集中（ステップ未選択）")), wrapper);
    contextRow->addWidget(m_actionParamsContextLabel, 1);
    m_editDefaultParamsButton = new QPushButton(I18n::t(QStringLiteral("デフォルト")), wrapper);
    m_editDefaultParamsButton->setToolTip(
        I18n::t(QStringLiteral("共通のデフォルト操作パラメータをダイアログで編集します（②の選択は変わりません）")));
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
        I18n::t(QStringLiteral("このステップの操作種別・重み・回数（②でステップを選択すると編集できます）")),
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
        I18n::t(QStringLiteral("操作の詳細設定（ドラッグ距離・キー文字種・スクロール量など）")), scrollContent);
    auto *layout = new QVBoxLayout(m_actionParamsGroup);

    auto *modeRow = new QHBoxLayout;
    m_stepUseDefaultParamsRadio = new QRadioButton(I18n::t(QStringLiteral("デフォルトを使う")), m_actionParamsGroup);
    m_stepUseCustomParamsRadio =
        new QRadioButton(I18n::t(QStringLiteral("このステップ専用の設定を使う")), m_actionParamsGroup);
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
    // Remember which app is currently selected (if any) *before* clearing
    // the combo below -- QComboBox::clear() forgets the selection outright,
    // so without this, every refresh (crash/stop, regaining focus, the
    // user's own "更新" click, ...) would silently reset ① back to nothing
    // selected. Captured here rather than via a currentIndexChanged signal
    // so it also picks up a selection the user made since the last refresh.
    const int previousIdx = m_targetCombo->currentIndex();
    if (previousIdx >= 0 && previousIdx < m_windows.size() && !m_windows[previousIdx].appName.isEmpty())
        m_lastTargetAppName = m_windows[previousIdx].appName;

    m_targetCombo->clear();
    m_windows = PlatformAutomation::listWindows();
    for (const WindowInfo &w : m_windows) {
        const QString label = QStringLiteral("%1 (pid %2)%3")
                                   .arg(w.appName.isEmpty() ? I18n::t(QStringLiteral("不明なアプリ")) : w.appName)
                                   .arg(w.pid)
                                   .arg(w.title.isEmpty() ? QString() : QStringLiteral(" - ") + w.title);
        m_targetCombo->addItem(label);
    }
    if (m_targetCombo->count() == 0)
        m_targetCombo->addItem(I18n::t(QStringLiteral("(ウィンドウが見つかりません)")));
    else
        tryReselectLastTarget();

    refreshPermissionLabel();
}

bool MainWindow::tryReselectLastTarget()
{
    if (m_lastTargetAppName.isEmpty())
        return false;
    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows[i].appName == m_lastTargetAppName) {
            m_targetCombo->setCurrentIndex(i);
            return true;
        }
    }
    return false;
}

void MainWindow::refreshPermissionLabel()
{
    const bool trusted = PlatformAutomation::isAccessibilityTrusted(false);
    m_permissionLabel->setText(trusted
                                    ? I18n::t(QStringLiteral("✓ 入力送信の権限は許可されています"))
                                    : I18n::t(QStringLiteral("✗ 権限が必要です（下のボタンから設定を開いてください）")));
    m_permissionLabel->setStyleSheet(trusted ? "color: green;" : "color: red;");
}

QString MainWindow::describeStep(const RegionStep &step, int index) const
{
    // SPEC.md 10 ④: how many of this fingerprint's recorded runs crashed
    // while this step was executing, out of the total recorded for it --
    // empty unless there's at least one crash on this exact step, so a
    // step with a clean history doesn't get cluttered with a "0/N" badge.
    QString crashBadge;
    if (m_statsTotalRuns > 0) {
        const int crashCount = m_stepCrashCounts.value(index, 0);
        if (crashCount > 0)
            crashBadge =
                I18n::t(QStringLiteral(" | ⚠ クラッシュ %1/%2回")).arg(crashCount).arg(m_statsTotalRuns);
    }

    if (step.isWaitStep) {
        const QString runningPrefix =
            index == m_currentRunningStepIndex ? I18n::t(QStringLiteral("▶ 実行中 ")) : QString();
        return I18n::t(QStringLiteral("%1ステップ%2: 待機（%3 ms）%4"))
            .arg(runningPrefix)
            .arg(index + 1)
            .arg(step.waitDurationMs)
            .arg(crashBadge);
    }

    if (step.isGroup) {
        const QString runningPrefix =
            index == m_currentRunningStepIndex ? I18n::t(QStringLiteral("▶ 実行中 ")) : QString();
        return I18n::t(QStringLiteral("%1ステップ%2: グループ（%3個のステップ、合計呼び出し%4回）%5"))
            .arg(runningPrefix)
            .arg(index + 1)
            .arg(step.groupMembers.size())
            .arg(step.groupTotalCallCount)
            .arg(crashBadge);
    }

    QStringList actions;
    if (step.enableClick)
        actions << I18n::t(QStringLiteral("クリック"));
    if (step.enableDoubleClick)
        actions << I18n::t(QStringLiteral("ダブルクリック"));
    if (step.enableDrag)
        actions << I18n::t(QStringLiteral("ドラッグ"));
    if (step.enableKey)
        actions << I18n::t(QStringLiteral("キー入力"));
    if (step.enableScrollUp)
        actions << I18n::t(QStringLiteral("スクロール(上)"));
    if (step.enableScrollDown)
        actions << I18n::t(QStringLiteral("スクロール(下)"));
    if (step.enableScrollHorizontal)
        actions << I18n::t(QStringLiteral("スクロール(横)"));
    if (step.enableShortcut)
        actions << I18n::t(QStringLiteral("ショートカット"));
    if (step.enableWindowOp)
        actions << I18n::t(QStringLiteral("ウィンドウ操作"));

    const QString regionDesc = step.useWholeWindow
                                    ? I18n::t(QStringLiteral("対象GUIの全領域"))
                                    : (step.regionName.isEmpty()
                                           ? I18n::t(QStringLiteral("(未選択)"))
                                           : I18n::t(QStringLiteral("操作領域「%1」")).arg(step.regionName));
    const QString paramsBadge =
        step.useDefaultActionParams ? QString() : I18n::t(QStringLiteral(" | [カスタム設定]"));
    const QString runningPrefix =
        index == m_currentRunningStepIndex ? I18n::t(QStringLiteral("▶ 実行中 ")) : QString();

    return I18n::t(QStringLiteral("%1ステップ%2: %3 | 操作: %4 | 回数: %5%6%7"))
        .arg(runningPrefix)
        .arg(index + 1)
        .arg(regionDesc)
        .arg(actions.isEmpty() ? I18n::t(QStringLiteral("(なし)")) : actions.join(QStringLiteral(", ")))
        .arg(step.actionCount)
        .arg(paramsBadge)
        .arg(crashBadge);
}

void MainWindow::refreshStepList()
{
    m_stepListWidget->clear();
    for (int i = 0; i < m_steps.size(); ++i)
        m_stepListWidget->addItem(describeStep(m_steps[i], i));
    updateGroupButtonsEnabled();
}

QString MainWindow::describeNamedRegion(const NamedRegion &region) const
{
    return I18n::t(QStringLiteral("%1（矩形%2個・除外%3個）%4"))
        .arg(region.name)
        .arg(region.regions.size())
        .arg(region.excludeRegions.size())
        .arg(region.followsTargetWindow ? I18n::t(QStringLiteral(" [ウィンドウ追従]")) : QString());
}

void MainWindow::refreshNamedRegionList()
{
    m_namedRegionListWidget->clear();
    for (const NamedRegion &region : m_namedRegions)
        m_namedRegionListWidget->addItem(describeNamedRegion(region));
}

QString MainWindow::describeSetupAction(const SetupAction &action, int index) const
{
    QString kindDesc;
    switch (action.type) {
    case SetupActionType::Click:
        kindDesc = I18n::t(QStringLiteral("クリック (%1, %2)")).arg(action.point.x()).arg(action.point.y());
        break;
    case SetupActionType::DoubleClick:
        kindDesc =
            I18n::t(QStringLiteral("ダブルクリック (%1, %2)")).arg(action.point.x()).arg(action.point.y());
        break;
    case SetupActionType::RightClick:
        kindDesc = I18n::t(QStringLiteral("右クリック (%1, %2)")).arg(action.point.x()).arg(action.point.y());
        break;
    case SetupActionType::Drag:
        kindDesc = I18n::t(QStringLiteral("ドラッグ (%1, %2) → (%3, %4)"))
                       .arg(action.point.x())
                       .arg(action.point.y())
                       .arg(action.dragToPoint.x())
                       .arg(action.dragToPoint.y());
        break;
    case SetupActionType::TypeText:
        kindDesc = I18n::t(QStringLiteral("文字入力 '%1'")).arg(action.text);
        break;
    case SetupActionType::KeyPress:
        kindDesc = I18n::t(QStringLiteral("キー入力 '%1'")).arg(action.keySequence);
        break;
    case SetupActionType::Wait:
        kindDesc = I18n::t(QStringLiteral("待機 %1ms")).arg(action.waitMs);
        break;
    }
    const QString labelSuffix =
        action.label.isEmpty() ? QString() : I18n::t(QStringLiteral(" [%1]")).arg(action.label);
    return I18n::t(QStringLiteral("%1: %2%3")).arg(index + 1).arg(kindDesc).arg(labelSuffix);
}

void MainWindow::refreshSetupActionList()
{
    m_setupActionListWidget->clear();
    for (int i = 0; i < m_setupActions.size(); ++i)
        m_setupActionListWidget->addItem(describeSetupAction(m_setupActions[i], i));
}

QStringList MainWindow::stepsReferencing(const QString &regionName) const
{
    QStringList result;
    for (int i = 0; i < m_steps.size(); ++i) {
        const RegionStep &step = m_steps[i];
        if (step.isGroup) {
            for (int j = 0; j < step.groupMembers.size(); ++j) {
                const RegionStep &member = step.groupMembers[j];
                if (!member.useWholeWindow && member.regionName == regionName)
                    result << I18n::t(QStringLiteral("ステップ%1（グループ内メンバー%2）")).arg(i + 1).arg(j + 1);
            }
        } else if (!step.useWholeWindow && step.regionName == regionName) {
            result << I18n::t(QStringLiteral("ステップ%1")).arg(i + 1);
        }
    }
    return result;
}

void MainWindow::renameRegionReferences(QList<RegionStep> &steps, const QString &oldName,
                                          const QString &newName) const
{
    for (RegionStep &step : steps) {
        if (step.isGroup)
            renameRegionReferences(step.groupMembers, oldName, newName);
        else if (!step.useWholeWindow && step.regionName == oldName)
            step.regionName = newName;
    }
}

QString MainWindow::generateDefaultRegionName() const
{
    for (int n = 1;; ++n) {
        // "操作領域N" (not just "領域N") so this doesn't read the same as
        // the "領域N"/"除外N" labels NamedRegionEditorDialog gives the
        // individual rectangles drawn inside one operation region -- those
        // are a different, unrelated numbering scope and having both say
        // "領域1" was confusing (SPEC.md 6.3).
        const QString candidate = I18n::t(QStringLiteral("操作領域%1")).arg(n);
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

bool MainWindow::currentTargetTopLeft(QPoint &outTopLeft) const
{
    const int idx = m_targetCombo->currentIndex();
    if (idx < 0 || idx >= m_windows.size())
        return false;
    QRect bounds;
    const WindowInfo &target = m_windows[idx];
    if (!PlatformAutomation::queryWindowBounds(target.windowId, target.pid, bounds))
        return false;
    outTopLeft = bounds.topLeft();
    return true;
}

void MainWindow::onAddNamedRegion()
{
    NamedRegion initial;
    initial.name = generateDefaultRegionName();
    QPoint targetTopLeft;
    const bool hasTarget = currentTargetTopLeft(targetTopLeft);
    // NamedRegionEditorDialog visualizes the region being built on screen
    // itself for the duration it's open (SPEC.md 6.3) -- MainWindow no
    // longer shows any on-screen highlight from the list selection.
    NamedRegionEditorDialog dialog(initial, targetTopLeft, hasTarget, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const NamedRegion region = dialog.result();
    for (const NamedRegion &existing : m_namedRegions) {
        if (existing.name == region.name) {
            QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                                  I18n::t(QStringLiteral("同じ名前の操作領域が既に存在します。")));
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

    QPoint targetTopLeft;
    const bool hasTarget = currentTargetTopLeft(targetTopLeft);
    NamedRegionEditorDialog dialog(m_namedRegions[row], targetTopLeft, hasTarget, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const NamedRegion region = dialog.result();

    for (int i = 0; i < m_namedRegions.size(); ++i) {
        if (i != row && m_namedRegions[i].name == region.name) {
            QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                                  I18n::t(QStringLiteral("同じ名前の操作領域が既に存在します。")));
            return;
        }
    }

    m_namedRegions[row] = region;
    if (oldName != region.name) {
        // Keep steps (top-level or inside a group) that referenced the old
        // name pointing at the same region rather than silently breaking
        // them.
        renameRegionReferences(m_steps, oldName, region.name);
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
            this, I18n::t(QStringLiteral("削除できません")),
            I18n::t(QStringLiteral("この操作領域は次のステップで使われているため削除できません: %1\n"
                            "先にそれらのステップの領域を変更するか、ステップを削除してください。"))
                .arg(referencingSteps.join(QStringLiteral(", "))));
        return;
    }

    m_namedRegions.removeAt(row);
    refreshNamedRegionList();
}

void MainWindow::onAddSetupAction()
{
    QPoint targetTopLeft;
    const bool hasTarget = currentTargetTopLeft(targetTopLeft);
    SetupAction initial;
    SetupActionEditorDialog dialog(initial, targetTopLeft, hasTarget, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_setupActions.append(dialog.result());
    refreshSetupActionList();
    m_setupActionListWidget->setCurrentRow(m_setupActions.size() - 1);
}

void MainWindow::onEditSelectedSetupAction()
{
    const int row = m_setupActionListWidget->currentRow();
    if (row < 0 || row >= m_setupActions.size())
        return;
    QPoint targetTopLeft;
    const bool hasTarget = currentTargetTopLeft(targetTopLeft);
    SetupActionEditorDialog dialog(m_setupActions[row], targetTopLeft, hasTarget, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_setupActions[row] = dialog.result();
    refreshSetupActionList();
    m_setupActionListWidget->setCurrentRow(row);
}

void MainWindow::onRemoveSelectedSetupAction()
{
    const int row = m_setupActionListWidget->currentRow();
    if (row < 0 || row >= m_setupActions.size())
        return;
    m_setupActions.removeAt(row);
    refreshSetupActionList();
}

void MainWindow::onMoveSetupActionUp()
{
    const int row = m_setupActionListWidget->currentRow();
    if (row > 0 && row < m_setupActions.size()) {
        m_setupActions.move(row, row - 1);
        refreshSetupActionList();
        m_setupActionListWidget->setCurrentRow(row - 1);
    }
}

void MainWindow::onMoveSetupActionDown()
{
    const int row = m_setupActionListWidget->currentRow();
    if (row >= 0 && row < m_setupActions.size() - 1) {
        m_setupActions.move(row, row + 1);
        refreshSetupActionList();
        m_setupActionListWidget->setCurrentRow(row + 1);
    }
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
        m_actionParamsContextLabel->setText(I18n::t(QStringLiteral("デフォルト値を編集中（ステップ未選択）")));
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
            I18n::t(QStringLiteral("ステップ %1 は待機ステップです（操作パラメータはありません）")).arg(row + 1));
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

    if (m_steps[row].isGroup) {
        // A group's members each have their own region/action-kind/
        // ActionParams settings, edited in StepGroupEditorDialog (via ②'s
        // "編集..." button) rather than here -- see RegionStep::isGroup.
        m_actionParamsContextLabel->setText(
            I18n::t(QStringLiteral("ステップ %1 はグループです。「編集...」からメンバーを設定してください")).arg(row + 1));
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

    // A *copy*, not a reference into m_steps[row]: ActionKindEditor::changed()
    // (fired by setKinds() below as it programmatically sets each widget) is
    // connected to flushActionParamsEditor(), which writes straight back
    // into m_steps[m_lastEditedStepRow] -- already equal to row by the time
    // setKinds() runs. If `step` aliased that same array slot, a reentrant
    // flush partway through setKinds() would overwrite fields setKinds()
    // hasn't read yet, and setKinds() would then read back its own
    // just-corrupted data for the remaining fields. A value copy is immune
    // to that.
    const RegionStep step = m_steps[row];
    m_actionParamsContextLabel->setText(I18n::t(QStringLiteral("ステップ %1 の操作種別・詳細設定を編集中")).arg(row + 1));
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
    if (m_suppressStepSelectionHandling)
        return;
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
        QMessageBox::warning(this, I18n::t(QStringLiteral("ステップの設定エラー")),
                              I18n::t(QStringLiteral("操作領域が選択されていません。")));
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
    const int ms = QInputDialog::getInt(this, I18n::t(QStringLiteral("待機ステップを追加")),
                                         I18n::t(QStringLiteral("待機時間 (ms):")), 1000, 1, 600000, 100, &ok);
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
        const int ms = QInputDialog::getInt(this, I18n::t(QStringLiteral("待機ステップを編集")),
                                             I18n::t(QStringLiteral("待機時間 (ms):")),
                                             m_steps[row].waitDurationMs, 1, 600000, 100, &ok);
        if (!ok)
            return;
        m_steps[row].waitDurationMs = ms;
        refreshStepList();
        m_stepListWidget->setCurrentRow(row);
        return;
    }

    if (m_steps[row].isGroup) {
        StepGroupEditorDialog dialog(m_steps[row], m_namedRegions, m_defaultActionParams,
                                      m_defaultActionKinds, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        m_steps[row] = dialog.result();
        refreshStepList();
        m_stepListWidget->setCurrentRow(row);
        return;
    }

    StepEditorDialog dialog(m_steps[row], m_namedRegions, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.useWholeWindow() && dialog.regionName().isEmpty()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("ステップの設定エラー")),
                              I18n::t(QStringLiteral("操作領域が選択されていません。")));
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
    m_lastEditedStepRow = -1;
    m_suppressStepSelectionHandling = true;
    m_steps.removeAt(row);
    refreshStepList();
    const int newRow = qMin(row, m_steps.size() - 1);
    if (newRow >= 0)
        m_stepListWidget->setCurrentRow(newRow);
    m_suppressStepSelectionHandling = false;
    loadActionParamsEditorForSelection();
}

void MainWindow::onMoveStepUp()
{
    const int row = m_stepListWidget->currentRow();
    if (row > 0 && row < m_steps.size()) {
        flushActionParamsEditor();
        m_lastEditedStepRow = -1;
        m_suppressStepSelectionHandling = true;
        m_steps.move(row, row - 1);
        refreshStepList();
        m_stepListWidget->setCurrentRow(row - 1);
        m_suppressStepSelectionHandling = false;
        loadActionParamsEditorForSelection();
    }
}

void MainWindow::onMoveStepDown()
{
    const int row = m_stepListWidget->currentRow();
    if (row >= 0 && row < m_steps.size() - 1) {
        flushActionParamsEditor();
        m_lastEditedStepRow = -1;
        m_suppressStepSelectionHandling = true;
        m_steps.move(row, row + 1);
        refreshStepList();
        m_stepListWidget->setCurrentRow(row + 1);
        m_suppressStepSelectionHandling = false;
        loadActionParamsEditorForSelection();
    }
}

void MainWindow::onClearSteps()
{
    flushActionParamsEditor();
    m_steps.clear();
    refreshStepList();
    loadActionParamsEditorForSelection();
}

void MainWindow::onGroupSelectedSteps()
{
    QList<int> rows;
    for (QListWidgetItem *item : m_stepListWidget->selectedItems())
        rows.append(m_stepListWidget->row(item));
    std::sort(rows.begin(), rows.end());
    if (rows.size() < 2)
        return;
    for (int row : rows) {
        if (row < 0 || row >= m_steps.size() || m_steps[row].isWaitStep || m_steps[row].isGroup) {
            QMessageBox::warning(
                this, I18n::t(QStringLiteral("グループ化できません")),
                I18n::t(QStringLiteral("待機ステップやグループ自体は、他のステップと一緒にグループ化できません"
                                "（グループの入れ子は未対応です）。")));
            return;
        }
    }

    flushActionParamsEditor();
    m_lastEditedStepRow = -1;
    m_suppressStepSelectionHandling = true;

    RegionStep group;
    group.isGroup = true;
    group.groupTotalCallCount = 50;
    for (int row : rows) {
        RegionStep member = m_steps[row];
        // Reset fields that only make sense at the top level or that this
        // step doesn't already carry a meaningful value for -- a step
        // being grouped for the first time defaults to equal weight among
        // its new siblings.
        member.isGroup = false;
        member.groupMembers.clear();
        member.groupWeight = 1;
        group.groupMembers.append(member);
    }

    const int insertAt = rows.first();
    for (int i = rows.size() - 1; i >= 0; --i)  // remove highest index first so earlier ones stay valid
        m_steps.removeAt(rows[i]);
    m_steps.insert(insertAt, group);

    refreshStepList();
    m_stepListWidget->setCurrentRow(insertAt);
    m_suppressStepSelectionHandling = false;
    loadActionParamsEditorForSelection();
}

void MainWindow::onUngroupSelectedStep()
{
    const int row = m_stepListWidget->currentRow();
    if (row < 0 || row >= m_steps.size() || !m_steps[row].isGroup)
        return;

    flushActionParamsEditor();
    m_lastEditedStepRow = -1;
    m_suppressStepSelectionHandling = true;
    const QList<RegionStep> members = m_steps[row].groupMembers;
    m_steps.removeAt(row);
    for (int i = 0; i < members.size(); ++i)
        m_steps.insert(row + i, members[i]);

    refreshStepList();
    if (!members.isEmpty())
        m_stepListWidget->setCurrentRow(row);
    m_suppressStepSelectionHandling = false;
    loadActionParamsEditorForSelection();
}

bool MainWindow::validateStepActionConfig(const RegionStep &step, const QString &stepLabel,
                                            QString &errorMessage) const
{
    if (step.isWaitStep || step.isGroup)
        return true;  // nothing here to validate (a group's members are validated individually)
    if (!step.hasAnyActionEnabled()) {
        errorMessage = I18n::t(QStringLiteral("%1は操作種別が選択されていません。②でこのステップを選択し、③操作パラメータ"
            "パネルで操作種別を1つ以上有効にしてください。"))
                           .arg(stepLabel);
        return false;
    }
    const ActionParams &params = effectiveParamsOf(step, m_defaultActionParams);
    if (step.enableKey && params.allowedKeyChars.isEmpty()) {
        errorMessage = I18n::t(QStringLiteral("キー入力を有効にした%1があります。使用文字を指定してください"
                                        "（デフォルトまたはそのステップの専用設定）。"))
                           .arg(stepLabel);
        return false;
    }
    if (step.enableShortcut && params.shortcutSequences.isEmpty()) {
        errorMessage = I18n::t(QStringLiteral("ショートカットキーを有効にした%1があります。ショートカットを最低1つ追加してください"
            "（デフォルトまたはそのステップの専用設定）。"))
                           .arg(stepLabel);
        return false;
    }
    if (step.enableClick && step.enableRightClick && params.enableContextMenuSelection) {
        if (params.contextMenuSelectionMode == ContextMenuSelectionMode::ByName &&
            params.contextMenuItemNames.isEmpty()) {
            errorMessage = I18n::t(QStringLiteral("メニュー項目選択（項目名指定）を有効にした%1があります。候補項目名を最低1つ"
                "追加してください（デフォルトまたはそのステップの専用設定）。"))
                               .arg(stepLabel);
            return false;
        }
        if (params.contextMenuSelectionMode == ContextMenuSelectionMode::ByIndex &&
            params.contextMenuIndices.isEmpty()) {
            errorMessage = I18n::t(QStringLiteral("メニュー項目選択（番号指定）を有効にした%1があります。候補の番号を最低1つ"
                "追加してください（デフォルトまたはそのステップの専用設定）。"))
                               .arg(stepLabel);
            return false;
        }
    }
    return true;
}

TestConfig MainWindow::buildConfigFromUi(bool &ok, QString &errorMessage) const
{
    ok = false;
    TestConfig config;

    const int idx = m_targetCombo->currentIndex();
    if (idx < 0 || idx >= m_windows.size()) {
        errorMessage = I18n::t(QStringLiteral("対象ウィンドウを選択してください。"));
        return config;
    }
    const WindowInfo &target = m_windows[idx];
    config.targetPid = target.pid;
    config.targetWindowId = target.windowId;
    config.targetAppName = target.appName;

    if (m_steps.isEmpty()) {
        errorMessage = I18n::t(QStringLiteral("ステップを最低1つ追加してください。"));
        return config;
    }
    config.steps = m_steps;
    config.namedRegions = m_namedRegions;
    config.setupActions = m_setupActions;
    config.defaultActionParams = m_defaultActionParams;

    for (int i = 0; i < m_steps.size(); ++i) {
        const RegionStep &step = m_steps[i];
        if (step.isWaitStep)
            continue;  // no region/action-kind/ActionParams fields to validate
        if (step.isGroup) {
            if (step.groupMembers.isEmpty()) {
                errorMessage =
                    I18n::t(QStringLiteral("ステップ%1（グループ）にステップが登録されていません。")).arg(i + 1);
                return config;
            }
            for (int j = 0; j < step.groupMembers.size(); ++j) {
                if (!validateStepActionConfig(
                        step.groupMembers[j],
                        I18n::t(QStringLiteral("ステップ%1（グループ内メンバー%2）")).arg(i + 1).arg(j + 1),
                        errorMessage))
                    return config;
            }
            continue;
        }
        if (!validateStepActionConfig(step, I18n::t(QStringLiteral("ステップ%1")).arg(i + 1), errorMessage))
            return config;
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
    if (m_screenshotModeOnceRadio->isChecked())
        config.screenshotCaptureMode = ScreenshotCaptureMode::OnceAtStart;
    else if (m_screenshotModeIntervalRadio->isChecked())
        config.screenshotCaptureMode = ScreenshotCaptureMode::FixedInterval;
    else
        config.screenshotCaptureMode = ScreenshotCaptureMode::PerStepChange;
    config.screenshotCaptureIntervalActions = m_screenshotIntervalSpin->value();
    config.enableScreenRecording = m_recordingCheck->isChecked();
    config.enableCrashDumpCollection = m_crashDumpCollectionCheck->isChecked();

    ok = true;
    return config;
}

void MainWindow::setControlsEnabled(bool enabled)
{
    m_targetGroup->setEnabled(enabled);
    m_setupActionsGroup->setEnabled(enabled);
    m_namedRegionGroup->setEnabled(enabled);
    m_stepsGroup->setEnabled(enabled);
    m_actionParamsGroup->setEnabled(enabled);
    const int selectedStepRow = m_stepListWidget->currentRow();
    const bool kindGroupApplicable = selectedStepRow >= 0 && selectedStepRow < m_steps.size() &&
                                      !m_steps[selectedStepRow].isWaitStep &&
                                      !m_steps[selectedStepRow].isGroup;
    m_stepKindGroup->setEnabled(enabled && kindGroupApplicable);
    m_editDefaultParamsButton->setEnabled(enabled);
    m_timingGroup->setEnabled(enabled);
    m_savePresetAction->setEnabled(enabled);
    m_loadPresetAction->setEnabled(enabled);
    m_batchRunCountSpin->setEnabled(enabled);
    m_startButton->setEnabled(enabled);
    m_stopButton->setEnabled(!enabled);
    m_pauseResumeButton->setEnabled(!enabled);
    if (enabled) {
        m_pauseResumeButton->setText(I18n::t(QStringLiteral("‖ 一時停止")));
        m_resourceUsageLabel->clear();
    }
    updateGroupButtonsEnabled();
}

void MainWindow::updateGroupButtonsEnabled()
{
    const QList<QListWidgetItem *> selected = m_stepListWidget->selectedItems();
    int plainCount = 0;
    for (QListWidgetItem *item : selected) {
        const int row = m_stepListWidget->row(item);
        if (row >= 0 && row < m_steps.size() && !m_steps[row].isWaitStep && !m_steps[row].isGroup)
            ++plainCount;
    }
    m_groupStepsButton->setEnabled(m_stepsGroup->isEnabled() && plainCount >= 2 &&
                                    plainCount == selected.size());
    const int row = m_stepListWidget->currentRow();
    m_ungroupStepButton->setEnabled(m_stepsGroup->isEnabled() && selected.size() == 1 && row >= 0 &&
                                     row < m_steps.size() && m_steps[row].isGroup);
}

bool MainWindow::beginRun(bool interactive)
{
    if (!PlatformAutomation::isAccessibilityTrusted(true)) {
        if (interactive) {
            QMessageBox::warning(this, I18n::t(QStringLiteral("権限が必要です")),
                                  I18n::t(QStringLiteral("他のアプリケーションを操作するための権限が許可されていません。"
                                                  "設定を許可してから、もう一度「開始」を押してください。")));
            onRefreshTargets();
        } else {
            appendLog(I18n::t(QStringLiteral("連続実行: 権限が確認できなかったため中断しました")));
            m_batchModeActive = false;
        }
        return false;
    }

    flushActionParamsEditor();

    bool ok = false;
    QString errorMessage;
    TestConfig config = buildConfigFromUi(ok, errorMessage);
    if (!ok) {
        if (interactive)
            QMessageBox::warning(this, I18n::t(QStringLiteral("設定エラー")), errorMessage);
        else {
            appendLog(I18n::t(QStringLiteral("連続実行: 設定エラーのため中断しました: %1")).arg(errorMessage));
            m_batchModeActive = false;
        }
        return false;
    }

    // SPEC.md 6.x: "連続自動実行では初回のみ実行する" -- TestConfig::
    // setupActions itself always means "run these now" (see its own
    // comment), so trimming it for batch runs after the first happens here,
    // not in RandomActionEngine.
    if (m_setupActionsFirstRunOnlyCheck->isChecked() && m_batchRunsCompleted > 0)
        config.setupActions.clear();

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
            if (interactive) {
                QMessageBox::warning(
                    this, I18n::t(QStringLiteral("安全確認に失敗しました")),
                    I18n::t(QStringLiteral("対象ウィンドウが安全に操作できることを確認できなかったため、開始できません。\n\n"
                        "対象ウィンドウが他のウィンドウに覆われていないか、最小化されていないか確認して"
                        "ください。それでも解決しない場合、この環境（特にLinuxの一部のウィンドウマネージャ）"
                        "では安全チェック機能自体が動作しない可能性があります（詳細はSPEC.md参照）。")));
            } else {
                appendLog(I18n::t(QStringLiteral("連続実行: 安全確認に失敗したため中断しました")));
                m_batchModeActive = false;
            }
            return false;
        }
    }

    // Refresh the fingerprint/badges/summary against exactly the config
    // that's about to run (SPEC.md 10 ③④), in case steps were edited since
    // the last load/finish.
    updateStatisticsDisplay();

    setControlsEnabled(false);
    m_statusLabel->setText(I18n::t(QStringLiteral("実行中")));
    m_runElapsed.restart();
    m_uiTimer->start();

    m_stopPanel = new StopPanel();
    connect(m_stopPanel, &StopPanel::stopRequested, this, &MainWindow::onStop);
    m_stopPanel->show();

    m_logView->clear();
    if (m_batchModeActive) {
        appendLog(I18n::t(QStringLiteral("連続実行: %1/%2回目を開始します"))
                       .arg(m_batchRunsCompleted + 1)
                       .arg(m_batchRunsRequested));
        m_batchProgressLabel->setText(
            I18n::t(QStringLiteral("連続実行: %1/%2回目")).arg(m_batchRunsCompleted + 1).arg(m_batchRunsRequested));
    }

    // Full, uncapped write-through log for this run (SPEC.md 6.8/10) -- see
    // the m_fullLogFile field comment for why this exists separately from
    // m_logView's capped display. Opened fresh per run; closed in
    // onEngineFinished().
    if (m_fullLogFile.isOpen())
        m_fullLogFile.close();
    const QString logDir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/EnduranceTestGUI_Logs");
    QDir().mkpath(logDir);
    const QString logPath = QStringLiteral("%1/run_%2.log").arg(
        logDir, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    m_fullLogFile.setFileName(logPath);
    if (m_fullLogFile.open(QIODevice::WriteOnly | QIODevice::Text))
        appendLog(I18n::t(QStringLiteral("完全なログをこのファイルに逐次保存します（画面表示とは別に、途中で"
                                  "切れることなく全操作を記録): %1"))
                       .arg(logPath));
    else
        appendLog(I18n::t(QStringLiteral("完全なログファイルを開けませんでした（画面表示のみになります）: %1")).arg(logPath));

    m_engine->start(config);
    return true;
}

void MainWindow::onStart()
{
    // SPEC.md 10 ①: N > 1 starts batch mode -- see beginRun()/
    // continueBatchIfNeeded()/waitForTargetThenContinueBatch() for how each
    // subsequent run is started automatically once this one finishes.
    m_batchRunsRequested = qMax(1, m_batchRunCountSpin->value());
    m_batchRunsCompleted = 0;
    m_batchModeActive = m_batchRunsRequested > 1;
    m_batchProgressLabel->setText(QString());
    beginRun(/*interactive=*/true);
}

void MainWindow::onStop()
{
    // A manual stop always cancels the whole batch, not just whatever run
    // is currently in progress (or being waited for -- see below) --
    // otherwise onEngineFinished()'s continueBatchIfNeeded() would just
    // start the next one right back up.
    const bool wasBatching = m_batchModeActive;
    m_batchModeActive = false;

    if (m_batchWaitTimer->isActive()) {
        // Between batch runs: the target app crashed/exited and we're
        // polling for it to come back (SPEC.md 10 ①). Nothing is actually
        // running for m_engine to stop, so undo the waiting state directly.
        m_batchWaitTimer->stop();
        appendLog(I18n::t(QStringLiteral("連続実行を中断しました（対象アプリの再起動待ち中でした）")));
        m_batchProgressLabel->setText(QString());
        setControlsEnabled(true);
        return;
    }

    if (wasBatching)
        appendLog(I18n::t(QStringLiteral("連続実行を中断します（現在の実行が終わり次第停止します）")));
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
    m_pauseResumeButton->setText(paused ? I18n::t(QStringLiteral("▶ 再開")) : I18n::t(QStringLiteral("‖ 一時停止")));
    m_statusLabel->setText(paused ? I18n::t(QStringLiteral("一時停止中")) : I18n::t(QStringLiteral("実行中")));
}

void MainWindow::onResourceUsageUpdated(double residentMemoryMB, double cpuPercent)
{
    m_resourceUsageLabel->setText(
        I18n::t(QStringLiteral("メモリ: %1 MB / CPU: %2%"))
            .arg(residentMemoryMB, 0, 'f', 1)
            .arg(cpuPercent, 0, 'f', 1));
}

void MainWindow::onEngineFinished(const QString &reason)
{
    setControlsEnabled(true);
    m_statusLabel->setText(I18n::t(QStringLiteral("停止: %1")).arg(reason));
    m_uiTimer->stop();
    if (m_stopPanel) {
        m_stopPanel->close();
        m_stopPanel->deleteLater();
    }

    // Re-list windows now rather than waiting for the user to click "更新"
    // or alt-tab back (SPEC.md 6.1/6.7): most relevant right after a crash,
    // where ①'s target entry is now stale/gone, so the user sees that
    // immediately and, once they relaunch the target app, tryReselectLastTarget()
    // (called by onRefreshTargets()) picks the new instance back up on the
    // very next refresh without them having to hunt for it in the list --
    // the steps/regions/timing config was never lost either way (it isn't
    // tied to a pid), so this is what lets them get back to testing with
    // the same setup with the least friction.
    onRefreshTargets();

    // Clear the "▶ 実行中" marker from whichever row still has it *before*
    // regenerating that row's text -- describeStep() decides the marker by
    // comparing its `index` argument against m_currentRunningStepIndex, so
    // resetting the member first (rather than after, as this used to do) is
    // what actually makes the comparison come out false once the run has
    // stopped, instead of it trivially matching itself and leaving the
    // marker shown forever until something else happens to touch this row.
    const int finishedStepIndex = m_currentRunningStepIndex;
    m_currentRunningStepIndex = -1;
    if (finishedStepIndex >= 0 && finishedStepIndex < m_steps.size()) {
        if (auto *item = m_stepListWidget->item(finishedStepIndex))
            item->setText(describeStep(m_steps[finishedStepIndex], finishedStepIndex));
    }

    if (m_fullLogFile.isOpen())
        m_fullLogFile.close();

    continueBatchIfNeeded();
}

void MainWindow::continueBatchIfNeeded()
{
    if (!m_batchModeActive)
        return;
    ++m_batchRunsCompleted;
    if (m_batchRunsCompleted >= m_batchRunsRequested) {
        appendLog(I18n::t(QStringLiteral("連続実行が完了しました（%1/%2回）")).arg(m_batchRunsCompleted).arg(m_batchRunsRequested));
        m_batchModeActive = false;
        m_batchProgressLabel->setText(QString());
        return;
    }
    appendLog(I18n::t(QStringLiteral("連続実行: %1/%2回が終了しました。次の実行の準備をします..."))
                   .arg(m_batchRunsCompleted)
                   .arg(m_batchRunsRequested));
    m_batchProgressLabel->setText(
        I18n::t(QStringLiteral("連続実行: %1回目の準備中...")).arg(m_batchRunsCompleted + 1));
    // Keep ①②③ locked and the stop button available while the batch
    // continues, even though onEngineFinished() (our caller) just
    // re-enabled everything for the "no batch" case.
    setControlsEnabled(false);
    m_stopButton->setEnabled(true);
    waitForTargetThenContinueBatch();
}

void MainWindow::waitForTargetThenContinueBatch()
{
    onRefreshTargets();
    if (tryReselectLastTarget()) {
        beginRun(/*interactive=*/false);
        return;
    }
    if (!m_targetLaunchCommandEdit->text().trimmed().isEmpty()) {
        appendLog(I18n::t(QStringLiteral("連続実行: 対象アプリが見つからないため、登録された起動コマンドで"
                                          "自動的に起動します")));
        launchTargetAppFromConfiguredCommand();
    } else {
        appendLog(I18n::t(QStringLiteral("連続実行: 対象アプリが見つかりません。手動で再起動してください"
                                          "（再起動を検知したら自動的に次の実行を開始します）")));
    }
    m_batchWaitTimer->start();
}

void MainWindow::onBatchWaitTick()
{
    onRefreshTargets();
    if (!tryReselectLastTarget())
        return;
    m_batchWaitTimer->stop();
    appendLog(I18n::t(QStringLiteral("連続実行: 対象アプリの起動を検知しました。次の実行を開始します")));
    beginRun(/*interactive=*/false);
}

void MainWindow::launchTargetAppFromConfiguredCommand()
{
    const QString commandLine = m_targetLaunchCommandEdit->text().trimmed();
    if (commandLine.isEmpty())
        return;
    const QStringList parts = QProcess::splitCommand(commandLine);
    if (parts.isEmpty())
        return;
    const QString program = parts.first();
    const QStringList arguments = parts.mid(1);
    if (!QProcess::startDetached(program, arguments))
        appendLog(I18n::t(QStringLiteral("対象アプリの自動起動に失敗しました: %1")).arg(commandLine));
}

void MainWindow::onActionLog(const QString &message)
{
    appendLog(message);
}

void MainWindow::appendLog(const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString line = QStringLiteral("[%1] %2").arg(timestamp, message);
    m_logView->appendPlainText(line);
    // m_logView above silently drops its oldest lines past 5000 blocks
    // (display performance); this file does not, so nothing from a long
    // run is ever lost before "ログを保存..." gets a chance to run (SPEC.md
    // 6.8/10). Only open while a run is in progress -- see onStart()/
    // onEngineFinished().
    if (m_fullLogFile.isOpen()) {
        m_fullLogFile.write(line.toUtf8());
        m_fullLogFile.write("\n");
        m_fullLogFile.flush();
    }
}

void MainWindow::onIterationCountChanged(qint64 count)
{
    m_iterationLabel->setText(I18n::t(QStringLiteral("実行回数: %1")).arg(count));
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
    m_elapsedLabel->setText(I18n::t(QStringLiteral("経過: %1秒")).arg(secs));
}

void MainWindow::onClearLog()
{
    m_logView->clear();
}

void MainWindow::onSaveLog()
{
    const QString path = QFileDialog::getSaveFileName(this, I18n::t(QStringLiteral("ログを保存")), QString(),
                                                        I18n::t(QStringLiteral("テキストファイル (*.txt)")));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(m_logView->toPlainText().toUtf8());
        file.close();
    }
}

void MainWindow::onRunSummaryReady(const RandomActionEngine::RunSummary &summary)
{
    m_lastSummary = summary;
    m_hasLastSummary = true;
    m_saveSummaryButton->setEnabled(true);
    m_saveSummaryButton->setToolTip(QString());
    // Also written straight into the log (SPEC.md 10) so it's visible right
    // away without a separate save step, and is included in "ログを保存...".
    for (const QString &line : RandomActionEngine::formatSummaryText(summary).split(QLatin1Char('\n')))
        appendLog(line);

    // Record this run for the crash-rate statistics (SPEC.md 10 ③④),
    // regardless of how it was started -- a manual "開始" click after the
    // user relaunched a crashed target app by hand counts exactly the same
    // as one the batch loop started automatically. m_targetCombo/m_windows
    // still reflect the target as it was during this just-finished run;
    // onEngineFinished() (which fires right after this) is what refreshes
    // them for the next one.
    {
        const int targetIdx = m_targetCombo->currentIndex();
        const QString appName = (targetIdx >= 0 && targetIdx < m_windows.size())
                                     ? m_windows[targetIdx].appName
                                     : m_lastTargetAppName;
        TestStatistics::RunRecord record;
        record.finishedAt = QDateTime::currentDateTime();
        record.configFingerprint = m_currentConfigFingerprint;
        record.targetAppName = appName;
        record.crashed = summary.targetCrashed;
        record.anomaly = summary.anomaly;
        record.crashStepIndex = summary.crashStepIndex;
        record.totalIterations = summary.totalIterations;
        record.elapsedMs = summary.elapsedMs;
        record.rngSeedUsed = summary.rngSeedUsed;
        record.stopReason = summary.stopReason;
        m_stats.addRun(record);
    }
    updateStatisticsDisplay();

    // Auto-save the exact test setup used (as a JSON preset) and the
    // operation-region screenshot alongside RandomActionEngine's own
    // anomaly screenshots/recording/crash-report search, all under the
    // same timestamp prefix, so a bug report is just "everything with this
    // prefix" (SPEC.md 6.7/10) -- no separate manual "save preset" step to
    // remember in the moment right after something went wrong.
    if (summary.anomaly && !summary.anomalyArtifactTimestamp.isEmpty()) {
        const QString baseDir = RandomActionEngine::anomalyArtifactsDirectory();
        QDir().mkpath(baseDir);

        QJsonObject preset = buildPresetJson();
        // The UI's own rngSeed field may be 0 ("ランダム"); what actually
        // reproduces this run is the seed the engine ended up using.
        QJsonObject timing = preset["timing"].toObject();
        timing["rngSeed"] = int(summary.rngSeedUsed);
        preset["timing"] = timing;
        const QString presetPath =
            QStringLiteral("%1/anomaly_%2_preset.json").arg(baseDir, summary.anomalyArtifactTimestamp);
        QFile presetFile(presetPath);
        if (presetFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            presetFile.write(QJsonDocument(preset).toJson(QJsonDocument::Indented));
            appendLog(I18n::t(QStringLiteral("異常停止時のテスト設定（この乱数シードで再現できるはずです）を"
                                      "保存しました: %1"))
                           .arg(presetPath));
        }

        if (m_engine->hasRegionScreenshot()) {
            QString regionName = m_engine->lastRegionScreenshotRegionName().remove(QLatin1Char(' ')).remove(QLatin1Char('/'));
            if (regionName.isEmpty())
                regionName = QStringLiteral("region");
            const QString shotPath = QStringLiteral("%1/anomaly_%2_region_%3.png")
                                          .arg(baseDir, summary.anomalyArtifactTimestamp, regionName);
            if (m_engine->lastRegionScreenshot().save(shotPath))
                appendLog(I18n::t(QStringLiteral("異常停止時点の操作領域画像を保存しました: %1")).arg(shotPath));
        }
    }

    // SPEC.md 6.7: pop up a dedicated notification the moment a target-app
    // crash is detected, on top of the usual log/summary/anomaly-artifact
    // handling above -- a developer watching the test may not be looking at
    // the log pane when it happens, and summary.stopReason already names
    // which step was running (RandomActionEngine::performRandomAction
    // builds it right before calling doStop()), so this dialog answers both
    // "did it crash?" and "at what step?" without digging through the log.
    //
    // Shown non-modally (show(), not the blocking exec() that
    // QMessageBox::critical() would run) and WA_DeleteOnClose'd instead of
    // kept around: a modal dialog here would freeze the whole app --
    // including SPEC.md 10 ①'s batch loop -- until a human clicks OK, which
    // defeats the point of an unattended overnight run reproducing a
    // probabilistic crash many times over. The crash is already fully
    // recorded (log, statistics, saved artifacts) by this point regardless
    // of whether anyone is watching to dismiss the dialog.
    if (summary.targetCrashed) {
        auto *dialog = new QMessageBox(QMessageBox::Critical, I18n::t(QStringLiteral("対象アプリのクラッシュを検知しました")),
            I18n::t(QStringLiteral("%1\n\n実行回数: %2\n乱数シード: %3\n\n"
                                    "異常停止時の記録（設定・操作領域画像・ログ等）は自動保存されています。"
                                    "詳細はログ欄を確認してください。"))
                .arg(summary.stopReason)
                .arg(summary.totalIterations)
                .arg(summary.rngSeedUsed),
            QMessageBox::Ok, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }
}

void MainWindow::onSaveSummary()
{
    if (!m_hasLastSummary)
        return;
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::t(QStringLiteral("実行結果サマリーを保存")), QStringLiteral("summary.json"),
        I18n::t(QStringLiteral("JSON (*.json);;テキスト (*.txt)")));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("保存エラー")), I18n::t(QStringLiteral("ファイルに書き込めませんでした。")));
        return;
    }
    if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        const QJsonDocument doc(RandomActionEngine::summaryToJson(m_lastSummary));
        file.write(doc.toJson(QJsonDocument::Indented));
    } else {
        file.write(RandomActionEngine::formatSummaryText(m_lastSummary).toUtf8());
    }
}

void MainWindow::onRegionScreenshotCaptured()
{
    m_saveRegionScreenshotButton->setEnabled(true);
    m_saveRegionScreenshotButton->setToolTip(QString());
}

void MainWindow::onSaveRegionScreenshot()
{
    if (!m_engine->hasRegionScreenshot())
        return;
    const QString dir =
        QFileDialog::getExistingDirectory(this, I18n::t(QStringLiteral("操作領域画像の保存先フォルダを選択")));
    if (dir.isEmpty())
        return;

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString label = m_engine->lastRegionScreenshotLabel().remove(QLatin1Char(' '));
    if (label.isEmpty())
        label = QStringLiteral("screenshot");
    QString regionName = m_engine->lastRegionScreenshotRegionName().remove(QLatin1Char(' ')).remove(QLatin1Char('/'));
    if (regionName.isEmpty())
        regionName = QStringLiteral("region");
    const QString path =
        I18n::t(QStringLiteral("%1/操作領域_%2_%3_%4.png")).arg(dir, label, regionName, timestamp);
    if (m_engine->lastRegionScreenshot().save(path))
        appendLog(I18n::t(QStringLiteral("操作領域画像を保存しました: %1")).arg(path));
    else
        QMessageBox::warning(this, I18n::t(QStringLiteral("保存エラー")), I18n::t(QStringLiteral("画像を保存できませんでした。")));
}

void MainWindow::onGlobalEmergencyStop()
{
    if (!m_engine->isRunning())
        return;
    appendLog(I18n::t(QStringLiteral("グローバル緊急停止ホットキーが押されました。")));
    m_engine->stop();
}

void MainWindow::onOpenAccessibilitySettings()
{
    PlatformAutomation::openAccessibilitySettings();
}

QJsonObject MainWindow::buildPresetJson() const
{
    QJsonObject root;
    root["formatVersion"] = 1;
    // Reference only -- pids aren't stable across runs, so the target still
    // has to be picked from ①'s live-enumerated list after loading; this
    // just helps the user recognize which entry to pick.
    const int targetIdx = m_targetCombo->currentIndex();
    root["targetAppNameHint"] =
        (targetIdx >= 0 && targetIdx < m_windows.size()) ? m_windows[targetIdx].appName : QString();
    // SPEC.md 10 ②: saved/loaded alongside the rest of the setup so a
    // preset built for unattended batch runs (①) stays fully self-contained.
    root["targetLaunchCommand"] = m_targetLaunchCommandEdit->text();

    QJsonArray regionsArr;
    for (const NamedRegion &r : m_namedRegions)
        regionsArr.append(namedRegionToJson(r));
    root["namedRegions"] = regionsArr;

    QJsonArray stepsArr;
    for (const RegionStep &s : m_steps)
        stepsArr.append(regionStepToJson(s));
    root["steps"] = stepsArr;

    QJsonArray setupActionsArr;
    for (const SetupAction &a : m_setupActions)
        setupActionsArr.append(setupActionToJson(a));
    root["setupActions"] = setupActionsArr;
    root["setupActionsFirstRunOnly"] = m_setupActionsFirstRunOnlyCheck->isChecked();

    root["defaultActionParams"] = actionParamsToJson(m_defaultActionParams);
    root["defaultActionKinds"] = regionStepToJson(m_defaultActionKinds);

    QJsonObject timing;
    timing["intervalMode"] = m_intervalModeRateRadio->isChecked() ? QStringLiteral("rate") : QStringLiteral("ms");
    timing["minIntervalMs"] = m_minIntervalSpin->value();
    timing["maxIntervalMs"] = m_maxIntervalSpin->value();
    timing["minRate"] = m_minRateSpin->value();
    timing["maxRate"] = m_maxRateSpin->value();
    timing["maxIterations"] = m_maxIterationsSpin->value();
    timing["maxDurationSec"] = m_maxDurationSecSpin->value();
    timing["maxSequenceLoops"] = m_maxSequenceLoopsSpin->value();
    timing["keepTargetActive"] = m_keepActiveCheck->isChecked();
    timing["rngSeed"] = m_rngSeedSpin->value();
    timing["screenshotCaptureMode"] = m_screenshotModeOnceRadio->isChecked()   ? QStringLiteral("onceAtStart")
                                       : m_screenshotModeIntervalRadio->isChecked() ? QStringLiteral("fixedInterval")
                                                                                    : QStringLiteral("perStepChange");
    timing["screenshotCaptureIntervalActions"] = m_screenshotIntervalSpin->value();
    timing["enableScreenRecording"] = m_recordingCheck->isChecked();
    timing["enableCrashDumpCollection"] = m_crashDumpCollectionCheck->isChecked();
    root["timing"] = timing;
    return root;
}

void MainWindow::onSavePreset()
{
    flushActionParamsEditor();

    const QString path = QFileDialog::getSaveFileName(this, I18n::t(QStringLiteral("テスト設定を保存")),
                                                        QStringLiteral("preset.json"),
                                                        QStringLiteral("JSON (*.json)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("保存エラー")), I18n::t(QStringLiteral("ファイルに書き込めませんでした。")));
        return;
    }
    file.write(QJsonDocument(buildPresetJson()).toJson(QJsonDocument::Indented));
    appendLog(I18n::t(QStringLiteral("テスト設定を保存しました: %1")).arg(path));
}

void MainWindow::onLoadPreset()
{
    if (!m_steps.isEmpty() || !m_namedRegions.isEmpty() || !m_setupActions.isEmpty()) {
        const auto reply = QMessageBox::question(
            this, I18n::t(QStringLiteral("確認")),
            I18n::t(QStringLiteral("現在の起動時セットアップ・操作領域・ステップ構成は読み込んだ内容で上書きされます。よろしいですか？")));
        if (reply != QMessageBox::Yes)
            return;
    }

    const QString path = QFileDialog::getOpenFileName(this, I18n::t(QStringLiteral("テスト設定を読み込む")), QString(),
                                                        QStringLiteral("JSON (*.json)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("読み込みエラー")), I18n::t(QStringLiteral("ファイルを開けませんでした。")));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("読み込みエラー")),
                              I18n::t(QStringLiteral("JSONとして解釈できませんでした: %1")).arg(parseError.errorString()));
        return;
    }
    const QJsonObject root = doc.object();

    m_namedRegions.clear();
    for (const QJsonValue &v : root["namedRegions"].toArray())
        m_namedRegions.append(namedRegionFromJson(v.toObject()));

    m_steps.clear();
    for (const QJsonValue &v : root["steps"].toArray())
        m_steps.append(regionStepFromJson(v.toObject()));

    m_setupActions.clear();
    for (const QJsonValue &v : root["setupActions"].toArray())
        m_setupActions.append(setupActionFromJson(v.toObject()));
    m_setupActionsFirstRunOnlyCheck->setChecked(root["setupActionsFirstRunOnly"].toBool(false));

    m_defaultActionParams = actionParamsFromJson(root["defaultActionParams"].toObject());
    m_defaultActionKinds = regionStepFromJson(root["defaultActionKinds"].toObject());

    const QJsonObject timing = root["timing"].toObject();
    if (timing["intervalMode"].toString() == QStringLiteral("rate"))
        m_intervalModeRateRadio->setChecked(true);
    else
        m_intervalModeMsRadio->setChecked(true);
    m_minIntervalSpin->setValue(timing["minIntervalMs"].toInt(m_minIntervalSpin->value()));
    m_maxIntervalSpin->setValue(timing["maxIntervalMs"].toInt(m_maxIntervalSpin->value()));
    m_minRateSpin->setValue(timing["minRate"].toDouble(m_minRateSpin->value()));
    m_maxRateSpin->setValue(timing["maxRate"].toDouble(m_maxRateSpin->value()));
    m_maxIterationsSpin->setValue(timing["maxIterations"].toInt(m_maxIterationsSpin->value()));
    m_maxDurationSecSpin->setValue(timing["maxDurationSec"].toInt(m_maxDurationSecSpin->value()));
    m_maxSequenceLoopsSpin->setValue(timing["maxSequenceLoops"].toInt(m_maxSequenceLoopsSpin->value()));
    m_keepActiveCheck->setChecked(timing["keepTargetActive"].toBool(m_keepActiveCheck->isChecked()));
    m_rngSeedSpin->setValue(timing["rngSeed"].toInt(m_rngSeedSpin->value()));
    const QString screenshotMode = timing["screenshotCaptureMode"].toString();
    if (screenshotMode == QStringLiteral("onceAtStart"))
        m_screenshotModeOnceRadio->setChecked(true);
    else if (screenshotMode == QStringLiteral("fixedInterval"))
        m_screenshotModeIntervalRadio->setChecked(true);
    else if (screenshotMode == QStringLiteral("perStepChange"))
        m_screenshotModePerStepRadio->setChecked(true);
    m_screenshotIntervalSpin->setValue(
        timing["screenshotCaptureIntervalActions"].toInt(m_screenshotIntervalSpin->value()));
    m_recordingCheck->setChecked(timing["enableScreenRecording"].toBool(m_recordingCheck->isChecked()));
    m_crashDumpCollectionCheck->setChecked(
        timing["enableCrashDumpCollection"].toBool(m_crashDumpCollectionCheck->isChecked()));

    m_lastEditedStepRow = -1;
    refreshNamedRegionList();
    refreshSetupActionList();
    refreshStepList();
    loadActionParamsEditorForSelection();
    m_targetLaunchCommandEdit->setText(root["targetLaunchCommand"].toString());

    const QString hint = root["targetAppNameHint"].toString();
    if (!hint.isEmpty()) {
        // Reuse the same match-by-appName mechanism a crash recovery uses
        // (tryReselectLastTarget(), see onEngineFinished()) so loading a
        // preset auto-selects the right target in ① whenever a window for
        // it is currently open, instead of always making the user pick it
        // by hand even when it's obvious which one it is.
        m_lastTargetAppName = hint;
        onRefreshTargets();
    }
    const bool targetSelected =
        !hint.isEmpty() && m_targetCombo->currentIndex() >= 0 &&
        m_targetCombo->currentIndex() < m_windows.size() &&
        m_windows[m_targetCombo->currentIndex()].appName == hint;
    appendLog(hint.isEmpty()
                  ? I18n::t(QStringLiteral("テスト設定を読み込みました: %1")).arg(path)
                  : targetSelected
                        ? I18n::t(QStringLiteral("テスト設定を読み込みました: %1（対象アプリ「%2」を自動選択しました）"))
                              .arg(path, hint)
                        : I18n::t(QStringLiteral("テスト設定を読み込みました: %1（保存時の対象アプリ: %2 -- "
                                    "①で対象ウィンドウを選び直してください）"))
                              .arg(path, hint));

    updateStatisticsDisplay();
}

void MainWindow::updateStatisticsDisplay()
{
    m_currentConfigFingerprint = TestStatistics::computeFingerprint(buildPresetJson());
    const TestStatistics::Aggregate agg = m_stats.aggregate(m_currentConfigFingerprint);
    m_statsTotalRuns = agg.totalRuns;
    m_stepCrashCounts.clear();
    for (const TestStatistics::StepCrashCount &sc : agg.crashesByStep)
        m_stepCrashCounts.insert(sc.stepIndex, sc.crashCount);

    m_statisticsSummaryLabel->setText(
        agg.totalRuns > 0
            ? I18n::t(QStringLiteral("このテスト設定: %1回中%2回クラッシュ（%3%）"))
                  .arg(agg.totalRuns)
                  .arg(agg.crashRuns)
                  .arg(agg.crashRate() * 100.0, 0, 'f', 1)
            : I18n::t(QStringLiteral("このテスト設定での実行記録はまだありません。")));

    // Update each row in place (not a full refreshStepList(), which would
    // clear/restore ②'s current selection needlessly -- see
    // m_suppressStepSelectionHandling's own comment for why that matters).
    for (int i = 0; i < m_stepListWidget->count() && i < m_steps.size(); ++i)
        m_stepListWidget->item(i)->setText(describeStep(m_steps[i], i));
}

void MainWindow::onShowStatistics()
{
    auto *dialog = new StatisticsDialog(&m_stats, m_currentConfigFingerprint, m_steps.size(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &StatisticsDialog::statisticsReset, this, &MainWindow::updateStatisticsDisplay);
    dialog->exec();
}
