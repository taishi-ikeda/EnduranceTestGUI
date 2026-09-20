#include "StatisticsDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "I18n.h"

StatisticsDialog::StatisticsDialog(TestStatistics *stats, const QString &fingerprint, int stepCount,
                                    QWidget *parent)
    : QDialog(parent), m_stats(stats), m_fingerprint(fingerprint), m_stepCount(stepCount)
{
    setWindowTitle(I18n::t(QStringLiteral("統計（クラッシュ率）")));
    resize(520, 420);

    auto *layout = new QVBoxLayout(this);

    auto *introLabel = new QLabel(
        I18n::t(QStringLiteral("現在読み込まれているテスト設定（①②③の内容）について、これまでに"
                                "記録された全実行結果の集計です。手動で対象アプリを再起動して同じ"
                                "テストを繰り返した場合も、自動連続実行を使った場合も、同じ集計に"
                                "含まれます。")),
        this);
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    m_stepTable = new QTableWidget(this);
    m_stepTable->setColumnCount(3);
    m_stepTable->setHorizontalHeaderLabels({I18n::t(QStringLiteral("ステップ")),
                                             I18n::t(QStringLiteral("クラッシュ回数")),
                                             I18n::t(QStringLiteral("全試行回数に対する割合"))});
    m_stepTable->horizontalHeader()->setStretchLastSection(true);
    m_stepTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stepTable->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_stepTable, 1);

    auto *buttonBox = new QDialogButtonBox(this);
    auto *exportButton = buttonBox->addButton(I18n::t(QStringLiteral("CSVエクスポート...")), QDialogButtonBox::ActionRole);
    auto *resetButton = buttonBox->addButton(I18n::t(QStringLiteral("この統計をリセット...")), QDialogButtonBox::ActionRole);
    buttonBox->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttonBox);
    connect(exportButton, &QPushButton::clicked, this, &StatisticsDialog::onExportCsv);
    connect(resetButton, &QPushButton::clicked, this, &StatisticsDialog::onReset);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refresh();
}

void StatisticsDialog::refresh()
{
    const TestStatistics::Aggregate agg = m_stats->aggregate(m_fingerprint);

    if (agg.totalRuns == 0) {
        m_summaryLabel->setText(
            I18n::t(QStringLiteral("このテスト設定での実行記録はまだありません。「開始」でテストを実行すると"
                                    "ここに集計されます。")));
    } else {
        m_summaryLabel->setText(
            I18n::t(QStringLiteral("試行回数: %1回\nクラッシュ回数: %2回（クラッシュ率 %3%）\n"
                                    "クラッシュまでの平均実行回数: %4回\nクラッシュまでの平均経過時間: %5秒"))
                .arg(agg.totalRuns)
                .arg(agg.crashRuns)
                .arg(agg.crashRate() * 100.0, 0, 'f', 1)
                .arg(agg.crashRuns > 0 ? QString::number(agg.meanIterationsToCrash, 'f', 1)
                                       : I18n::t(QStringLiteral("(なし)")))
                .arg(agg.crashRuns > 0 ? QString::number(agg.meanElapsedMsToCrash / 1000.0, 'f', 1)
                                       : I18n::t(QStringLiteral("(なし)"))));
    }

    m_stepTable->setRowCount(agg.crashesByStep.size());
    int row = 0;
    for (const TestStatistics::StepCrashCount &sc : agg.crashesByStep) {
        const QString stepLabel = (sc.stepIndex >= 0 && sc.stepIndex < m_stepCount)
                                       ? I18n::t(QStringLiteral("ステップ %1")).arg(sc.stepIndex + 1)
                                       : I18n::t(QStringLiteral("ステップ %1（現在の構成には存在しません）"))
                                             .arg(sc.stepIndex + 1);
        m_stepTable->setItem(row, 0, new QTableWidgetItem(stepLabel));
        m_stepTable->setItem(row, 1, new QTableWidgetItem(QString::number(sc.crashCount)));
        const double pct = agg.totalRuns > 0 ? 100.0 * sc.crashCount / agg.totalRuns : 0.0;
        m_stepTable->setItem(
            row, 2,
            new QTableWidgetItem(I18n::t(QStringLiteral("%1/%2回 (%3%)"))
                                      .arg(sc.crashCount)
                                      .arg(agg.totalRuns)
                                      .arg(pct, 0, 'f', 1)));
        ++row;
    }
    m_stepTable->resizeColumnsToContents();
}

void StatisticsDialog::onExportCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, I18n::t(QStringLiteral("統計をCSVでエクスポート")),
                                                        QStringLiteral("crash_stats.csv"),
                                                        I18n::t(QStringLiteral("CSV (*.csv)")));
    if (path.isEmpty())
        return;
    if (m_stats->exportCsv(path))
        QMessageBox::information(this, I18n::t(QStringLiteral("エクスポート完了")),
                                  I18n::t(QStringLiteral("記録されている全テスト設定分の実行履歴をCSVに"
                                                          "書き出しました: %1"))
                                      .arg(path));
    else
        QMessageBox::warning(this, I18n::t(QStringLiteral("エクスポートエラー")),
                              I18n::t(QStringLiteral("ファイルに書き込めませんでした。")));
}

void StatisticsDialog::onReset()
{
    const auto reply = QMessageBox::question(
        this, I18n::t(QStringLiteral("確認")),
        I18n::t(QStringLiteral("このテスト設定についてこれまでに記録された統計（試行回数・クラッシュ回数・"
                                "ステップ別内訳）をすべて削除します。よろしいですか？"
                                "（他のテスト設定の統計には影響しません）")));
    if (reply != QMessageBox::Yes)
        return;
    m_stats->resetFingerprint(m_fingerprint);
    refresh();
    emit statisticsReset();
}
