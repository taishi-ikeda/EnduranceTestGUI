#include "TestTargetWindow.h"

#include <QCheckBox>
#include <QDateTime>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include "CounterButton.h"

TestTargetWindow::TestTargetWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("TestTarget - EnduranceTestGUI 動作確認用"));
    m_uptime.start();

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *rootLayout = new QVBoxLayout(central);

    // --- Button grid ---
    auto *buttonGroup = new QGroupBox(QStringLiteral("クリック対象ボタン (左/右/二重クリックを別々に集計)"), central);
    auto *grid = new QGridLayout(buttonGroup);
    constexpr int kButtonCount = 9;
    m_leftCounts.resize(kButtonCount);
    m_rightCounts.resize(kButtonCount);
    m_doubleCounts.resize(kButtonCount);
    for (int i = 0; i < kButtonCount; ++i) {
        auto *btn = new CounterButton(QStringLiteral("ボタン%1\n左:0 右:0 二重:0").arg(i + 1), buttonGroup);
        btn->setMinimumSize(120, 60);
        connect(btn, &QPushButton::clicked, this, [this, i] { onButtonLeftClicked(i); });
        connect(btn, &CounterButton::rightClicked, this, [this, i] { onButtonRightClicked(i); });
        connect(btn, &CounterButton::doubleClicked, this, [this, i] { onButtonDoubleClicked(i); });
        grid->addWidget(btn, i / 3, i % 3);
        m_buttons.append(btn);
    }
    rootLayout->addWidget(buttonGroup);

    // --- Checkbox / slider row ---
    auto *miscGroup = new QGroupBox(QStringLiteral("チェックボックス / スライダー"), central);
    auto *miscLayout = new QHBoxLayout(miscGroup);

    m_checkBox = new QCheckBox(QStringLiteral("チェックボックス (切替回数: 0)"), miscGroup);
    connect(m_checkBox, &QCheckBox::toggled, this, &TestTargetWindow::onCheckBoxToggled);
    miscLayout->addWidget(m_checkBox);

    m_slider = new QSlider(Qt::Horizontal, miscGroup);
    m_slider->setRange(0, 100);
    connect(m_slider, &QSlider::valueChanged, this, &TestTargetWindow::onSliderValueChanged);
    miscLayout->addWidget(m_slider, 1);
    m_sliderLabel = new QLabel(QStringLiteral("値:0 (変化回数:0)"), miscGroup);
    miscLayout->addWidget(m_sliderLabel);

    rootLayout->addWidget(miscGroup);

    // --- Key input / shortcuts ---
    auto *keyGroup = new QGroupBox(
        QStringLiteral("キー入力欄 (Tab/Return/Escape/矢印キーやCtrl+C/V/A/Zショートカットもここで受信)"), central);
    auto *keyLayout = new QVBoxLayout(keyGroup);
    m_keyInputEdit = new QLineEdit(keyGroup);
    m_keyInputEdit->setPlaceholderText(QStringLiteral("ここにランダムキー入力・ショートカットが届きます"));
    m_keyInputEdit->installEventFilter(this);
    keyLayout->addWidget(m_keyInputEdit);
    rootLayout->addWidget(keyGroup);

    // --- Scroll areas (vertical + horizontal) ---
    auto *scrollGroup = new QGroupBox(QStringLiteral("スクロール対象領域 (縦)"), central);
    auto *scrollGroupLayout = new QVBoxLayout(scrollGroup);
    m_scrollArea = new QScrollArea(scrollGroup);
    m_scrollArea->setFixedHeight(120);
    auto *scrollContent = new QWidget;
    auto *scrollContentLayout = new QVBoxLayout(scrollContent);
    for (int i = 0; i < 40; ++i)
        scrollContentLayout->addWidget(new QLabel(QStringLiteral("行 %1").arg(i + 1)));
    m_scrollArea->setWidget(scrollContent);
    m_scrollArea->setWidgetResizable(true);
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &TestTargetWindow::onScrollValueChanged);
    scrollGroupLayout->addWidget(m_scrollArea);
    rootLayout->addWidget(scrollGroup);

    auto *hScrollGroup = new QGroupBox(QStringLiteral("スクロール対象領域 (横。scrollHorizontal設定の確認用)"), central);
    auto *hScrollGroupLayout = new QVBoxLayout(hScrollGroup);
    m_hScrollArea = new QScrollArea(hScrollGroup);
    m_hScrollArea->setFixedHeight(60);
    m_hScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_hScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    auto *hScrollContent = new QWidget;
    auto *hScrollContentLayout = new QHBoxLayout(hScrollContent);
    for (int i = 0; i < 40; ++i)
        hScrollContentLayout->addWidget(new QLabel(QStringLiteral("列%1").arg(i + 1)));
    m_hScrollArea->setWidget(hScrollContent);
    connect(m_hScrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &TestTargetWindow::onHScrollValueChanged);
    hScrollGroupLayout->addWidget(m_hScrollArea);
    rootLayout->addWidget(hScrollGroup);

    // --- Context menu target ---
    auto *contextGroup = new QGroupBox(
        QStringLiteral("右クリックメニュー対象 (enableContextMenuSelection設定の確認用)"), central);
    auto *contextLayout = new QVBoxLayout(contextGroup);
    m_contextMenuTarget =
        new QPushButton(QStringLiteral("このボタンを右クリックするとメニューが出ます (呼出:0)"), contextGroup);
    m_contextMenuTarget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_contextMenuTarget, &QPushButton::customContextMenuRequested, this,
            &TestTargetWindow::onContextMenuRequested);
    contextLayout->addWidget(m_contextMenuTarget);
    m_contextMenuItemLabels = {QStringLiteral("項目1"), QStringLiteral("項目2"), QStringLiteral("項目3"),
                               QStringLiteral("キャンセル")};
    m_contextMenuItemCounts.fill(0, m_contextMenuItemLabels.size());
    m_contextMenuLabel = new QLabel(
        QStringLiteral("選択内訳: 項目1:0 項目2:0 項目3:0 キャンセル:0"), contextGroup);
    contextLayout->addWidget(m_contextMenuLabel);
    rootLayout->addWidget(contextGroup);

    // --- Window geometry / state ---
    m_geometryLabel = new QLabel(central);
    rootLayout->addWidget(m_geometryLabel);

    // --- Stats & hazard button ---
    auto *bottomRow = new QHBoxLayout;
    m_statsLabel = new QLabel(central);
    bottomRow->addWidget(m_statsLabel, 1);

    auto *resetButton = new QPushButton(QStringLiteral("カウンタをリセット"), central);
    connect(resetButton, &QPushButton::clicked, this, &TestTargetWindow::onResetCounters);
    bottomRow->addWidget(resetButton);

    auto *quitButton = new QPushButton(QStringLiteral("終了 (クリック厳禁 - 除外領域テスト用)"), central);
    quitButton->setStyleSheet(
        "QPushButton { background-color: #c62828; color: white; font-weight: bold; }");
    connect(quitButton, &QPushButton::clicked, this, &TestTargetWindow::onQuitButtonClicked);
    bottomRow->addWidget(quitButton);

    rootLayout->addLayout(bottomRow);

    // --- Log ---
    m_logView = new QPlainTextEdit(central);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(2000);
    m_logView->setMaximumHeight(160);
    rootLayout->addWidget(m_logView);

    auto *statsTimer = new QTimer(this);
    connect(statsTimer, &QTimer::timeout, this, &TestTargetWindow::updateStatsLabel);
    statsTimer->start(500);
    updateStatsLabel();

    resize(560, 900);
    updateGeometryLabel();
}

void TestTargetWindow::onButtonLeftClicked(int index)
{
    ++m_leftCounts[index];
    ++m_totalLeftClicks;
    m_buttons[index]->setText(QStringLiteral("ボタン%1\n左:%2 右:%3 二重:%4")
                                   .arg(index + 1)
                                   .arg(m_leftCounts[index])
                                   .arg(m_rightCounts[index])
                                   .arg(m_doubleCounts[index]));
    appendLog(QStringLiteral("ボタン%1: 左クリック").arg(index + 1));
}

void TestTargetWindow::onButtonRightClicked(int index)
{
    ++m_rightCounts[index];
    ++m_totalRightClicks;
    m_buttons[index]->setText(QStringLiteral("ボタン%1\n左:%2 右:%3 二重:%4")
                                   .arg(index + 1)
                                   .arg(m_leftCounts[index])
                                   .arg(m_rightCounts[index])
                                   .arg(m_doubleCounts[index]));
    appendLog(QStringLiteral("ボタン%1: 右クリック").arg(index + 1));
}

void TestTargetWindow::onButtonDoubleClicked(int index)
{
    ++m_doubleCounts[index];
    ++m_totalDoubleClicks;
    m_buttons[index]->setText(QStringLiteral("ボタン%1\n左:%2 右:%3 二重:%4")
                                   .arg(index + 1)
                                   .arg(m_leftCounts[index])
                                   .arg(m_rightCounts[index])
                                   .arg(m_doubleCounts[index]));
    appendLog(QStringLiteral("ボタン%1: 二重クリック").arg(index + 1));
}

void TestTargetWindow::onCheckBoxToggled(bool checked)
{
    ++m_checkToggleCount;
    m_checkBox->setText(QStringLiteral("チェックボックス (切替回数: %1)").arg(m_checkToggleCount));
    appendLog(QStringLiteral("チェックボックス切替: %1").arg(checked ? QStringLiteral("ON") : QStringLiteral("OFF")));
}

void TestTargetWindow::onSliderValueChanged(int value)
{
    ++m_sliderChangeCount;
    m_sliderLabel->setText(QStringLiteral("値:%1 (変化回数:%2)").arg(value).arg(m_sliderChangeCount));
}

void TestTargetWindow::onScrollValueChanged(int value)
{
    ++m_scrollEventCount;
    Q_UNUSED(value);
}

void TestTargetWindow::onHScrollValueChanged(int value)
{
    ++m_hScrollEventCount;
    Q_UNUSED(value);
}

void TestTargetWindow::onContextMenuRequested(const QPoint &pos)
{
    ++m_contextMenuOpenCount;
    m_contextMenuTarget->setText(
        QStringLiteral("このボタンを右クリックするとメニューが出ます (呼出:%1)").arg(m_contextMenuOpenCount));

    QMenu menu(m_contextMenuTarget);
    for (const QString &label : m_contextMenuItemLabels) {
        QAction *action = menu.addAction(label);
        connect(action, &QAction::triggered, this, [this, label] { onContextMenuItemTriggered(label); });
    }
    menu.exec(m_contextMenuTarget->mapToGlobal(pos));
}

void TestTargetWindow::onContextMenuItemTriggered(const QString &itemName)
{
    const int idx = m_contextMenuItemLabels.indexOf(itemName);
    if (idx >= 0)
        ++m_contextMenuItemCounts[idx];
    appendLog(QStringLiteral("右クリックメニュー選択: %1").arg(itemName));
    m_contextMenuLabel->setText(QStringLiteral("選択内訳: %1")
                                     .arg([this] {
                                         QStringList parts;
                                         for (int i = 0; i < m_contextMenuItemLabels.size(); ++i)
                                             parts << QStringLiteral("%1:%2")
                                                          .arg(m_contextMenuItemLabels[i])
                                                          .arg(m_contextMenuItemCounts[i]);
                                         return parts.join(QStringLiteral(" "));
                                     }()));
}

void TestTargetWindow::onResetCounters()
{
    for (int i = 0; i < m_buttons.size(); ++i) {
        m_leftCounts[i] = 0;
        m_rightCounts[i] = 0;
        m_doubleCounts[i] = 0;
        m_buttons[i]->setText(QStringLiteral("ボタン%1\n左:0 右:0 二重:0").arg(i + 1));
    }
    m_totalLeftClicks = 0;
    m_totalRightClicks = 0;
    m_totalDoubleClicks = 0;
    m_checkToggleCount = 0;
    m_checkBox->setText(QStringLiteral("チェックボックス (切替回数: 0)"));
    m_sliderChangeCount = 0;
    m_keyPressCount = 0;
    m_shortcutCount = 0;
    m_scrollEventCount = 0;
    m_hScrollEventCount = 0;
    m_contextMenuOpenCount = 0;
    m_contextMenuItemCounts.fill(0, m_contextMenuItemLabels.size());
    m_contextMenuTarget->setText(QStringLiteral("このボタンを右クリックするとメニューが出ます (呼出:0)"));
    m_contextMenuLabel->setText(QStringLiteral("選択内訳: 項目1:0 項目2:0 項目3:0 キャンセル:0"));
    m_minimizeCount = 0;
    m_maximizeCount = 0;
    m_keyInputEdit->clear();
    m_logView->clear();
    m_uptime.restart();
    appendLog(QStringLiteral("カウンタをリセットしました"));
}

void TestTargetWindow::onQuitButtonClicked()
{
    const auto answer = QMessageBox::warning(
        this, QStringLiteral("終了確認"),
        QStringLiteral("本当に終了しますか？（誤操作防止のための確認です）"), QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer == QMessageBox::Yes)
        close();
}

void TestTargetWindow::updateStatsLabel()
{
    const qint64 secs = m_uptime.elapsed() / 1000;
    m_statsLabel->setText(
        QStringLiteral("起動から %1 秒 | 総クリック 左:%2 右:%3 二重:%4 | キー入力:%5 ショートカット:%6 | "
                        "縦スクロール:%7 横スクロール:%8 | 最小化:%9 最大化:%10")
            .arg(secs)
            .arg(m_totalLeftClicks)
            .arg(m_totalRightClicks)
            .arg(m_totalDoubleClicks)
            .arg(m_keyPressCount)
            .arg(m_shortcutCount)
            .arg(m_scrollEventCount)
            .arg(m_hScrollEventCount)
            .arg(m_minimizeCount)
            .arg(m_maximizeCount));
}

void TestTargetWindow::appendLog(const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_logView->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, message));
}

bool TestTargetWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_keyInputEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const bool isShortcutCombo = (keyEvent->modifiers() & Qt::ControlModifier) &&
                                      (keyEvent->key() == Qt::Key_C || keyEvent->key() == Qt::Key_V ||
                                       keyEvent->key() == Qt::Key_A || keyEvent->key() == Qt::Key_Z);
        if (isShortcutCombo) {
            ++m_shortcutCount;
            appendLog(QStringLiteral("ショートカット受信: Ctrl+%1")
                          .arg(QKeySequence(keyEvent->key()).toString()));
        } else {
            ++m_keyPressCount;
            const QString shown =
                keyEvent->text().isEmpty() ? QKeySequence(keyEvent->key()).toString() : keyEvent->text();
            appendLog(QStringLiteral("キー入力: '%1'").arg(shown));
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void TestTargetWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateGeometryLabel();
}

void TestTargetWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    updateGeometryLabel();
}

void TestTargetWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        const bool nowMinimized = isMinimized();
        const bool nowMaximized = isMaximized();
        if (nowMinimized && !m_wasMinimized)
            ++m_minimizeCount;
        if (nowMaximized && !m_wasMaximized)
            ++m_maximizeCount;
        m_wasMinimized = nowMinimized;
        m_wasMaximized = nowMaximized;
        updateGeometryLabel();
    }
}

void TestTargetWindow::updateGeometryLabel()
{
    const QRect g = geometry();
    m_geometryLabel->setText(QStringLiteral("ウインドウ位置・サイズ: x:%1 y:%2 幅:%3 高さ:%4 (windowOp設定の確認用)")
                                  .arg(g.x())
                                  .arg(g.y())
                                  .arg(g.width())
                                  .arg(g.height()));
}
