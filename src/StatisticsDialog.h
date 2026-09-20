#pragma once

#include <QDialog>

#include "TestStatistics.h"

// Shows the aggregated crash statistics for the currently loaded test setup
// (SPEC.md 10 ③, "確率的なクラッシュの解析"): out of every run ever recorded
// for this exact ①②③ configuration (this session's and previous sessions'
// alike -- TestStatistics persists to disk regardless of whether a run was
// started by hand or by the automatic batch loop), how many crashed and at
// which step. Opened from the "統計..." button below the log
// (MainWindow::onShowStatistics()).
class StatisticsDialog : public QDialog
{
    Q_OBJECT

public:
    // `stats` is not owned; it must outlive this dialog (MainWindow keeps
    // one for its whole lifetime). `fingerprint` identifies which test
    // setup's history to show/export/reset, `stepCount` is used only to
    // validate that a step index from history still makes sense against the
    // currently loaded step list (a config that has since been edited may
    // have fewer steps than when some of its history was recorded).
    StatisticsDialog(TestStatistics *stats, const QString &fingerprint, int stepCount,
                      QWidget *parent = nullptr);

signals:
    // Emitted after "統計をリセット" actually clears this fingerprint's
    // history, so MainWindow can refresh the ②ステップ一覧's crash badges
    // (SPEC.md 10 ④) immediately rather than leaving them stale until the
    // next run.
    void statisticsReset();

private slots:
    void onExportCsv();
    void onReset();

private:
    void refresh();

    TestStatistics *m_stats = nullptr;
    QString m_fingerprint;
    int m_stepCount = 0;

    class QLabel *m_summaryLabel = nullptr;
    class QTableWidget *m_stepTable = nullptr;
};
