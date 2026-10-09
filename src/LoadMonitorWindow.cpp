#include "LoadMonitorWindow.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "I18n.h"
#include "LoadMonitor.h"
#include "LoadMonitorChartWidget.h"

LoadMonitorWindow::LoadMonitorWindow(LoadMonitor *monitor, QWidget *parent) : QWidget(parent)
{
    // parentを受け取ってMainWindowに寿命を委ねつつ、常に独立した最上位
    // ウィンドウとして振る舞わせる（タイトルバー・最小化/最大化ボタン付き）。
    setWindowFlag(Qt::Window);
    setWindowTitle(I18n::t(QStringLiteral("負荷モニター（CPU/メモリの可視化）")));

    auto *layout = new QVBoxLayout(this);

    auto *headerRow = new QHBoxLayout;
    m_enabledCheck = new QCheckBox(I18n::t(QStringLiteral("負荷モニターを有効にする")), this);
    m_enabledCheck->setChecked(false);
    m_enabledCheck->setToolTip(
        I18n::t(QStringLiteral("有効にすると、テスト実行中に対象アプリ自身のCPU使用率・メモリ使用量と、"
                                "システム全体のCPU使用率（Linuxのみ）をリアルタイムにグラフ表示します。"
                                "異常停止（クラッシュ）時には、直前の推移をCSV・画像として自動保存するため、"
                                "クラッシュ直前にどの程度の負荷がかかっていたかを後から確認できます。"
                                "無効（デフォルト）のままなら、サンプリング用のタイマーすら動かないため、"
                                "耐久テスト本体の動作には一切影響しません。")));
    headerRow->addWidget(m_enabledCheck);
    headerRow->addWidget(new QLabel(I18n::t(QStringLiteral("サンプリング間隔:")), this));
    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(50, 60000);
    m_intervalSpin->setValue(500);
    m_intervalSpin->setSuffix(QStringLiteral(" ms"));
    m_intervalSpin->setToolTip(
        I18n::t(QStringLiteral("短くするほど細かい時間分解能でCPU/メモリの推移を記録できますが、"
                                "①の操作間隔（最短ms指定）に近づきすぎると、サンプリング自体が耐久テスト"
                                "本来の高速操作の妨げになり得ます。操作間隔より十分大きい値を推奨します"
                                "（デフォルト500msは、ほとんどの設定で安全な余裕を持った値です）。")));
    headerRow->addWidget(m_intervalSpin);
    headerRow->addStretch();
    layout->addLayout(headerRow);

    m_chartWidget = new LoadMonitorChartWidget(this);
    m_chartWidget->setLoadMonitor(monitor);
    layout->addWidget(m_chartWidget);

    m_saveButton = new QPushButton(I18n::t(QStringLiteral("負荷モニターのグラフを保存...")), this);
    m_saveButton->setEnabled(false);
    layout->addWidget(m_saveButton);
    connect(m_saveButton, &QPushButton::clicked, this, &LoadMonitorWindow::saveRequested);
    connect(monitor, &LoadMonitor::sampleAdded, this,
            [this, monitor]() { m_saveButton->setEnabled(!monitor->samples().isEmpty()); });

    resize(560, 360);
}

bool LoadMonitorWindow::monitoringEnabled() const
{
    return m_enabledCheck->isChecked();
}

void LoadMonitorWindow::setMonitoringEnabled(bool enabled)
{
    m_enabledCheck->setChecked(enabled);
}

int LoadMonitorWindow::intervalMs() const
{
    return m_intervalSpin->value();
}

void LoadMonitorWindow::setIntervalMs(int ms)
{
    m_intervalSpin->setValue(ms);
}
