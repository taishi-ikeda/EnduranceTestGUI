#pragma once

#include <QWidget>

class QCheckBox;
class QSpinBox;
class QPushButton;
class LoadMonitor;
class LoadMonitorChartWidget;

// 「負荷モニターはメニューから選択してmain windowとは別のwindowで確認できる
// ようにしてください」との要望を受けて、従来MainWindowの⑦タイミング・制限の
// 隣に直接埋め込んでいた負荷モニターのUI（有効化チェック・サンプリング間隔・
// チャート・保存ボタン）を、独立した最上位ウィンドウへ切り出したもの。
// MainWindowの「表示」メニューの「負荷モニター...」から開閉する（モードレス
// ダイアログではなく通常のウィンドウなので、開いたままメイン画面側の操作も
// 並行して行える）。
//
// 実際にサンプリングを行うLoadMonitor自体は引き続きMainWindowが所有し、この
// ウィンドウが一度も開かれなくても（＝ユーザーが一度もメニューを選ばなくても）
// 既存どおり動作する -- このウィンドウは純粋な表示/設定コンテナであり、
// LoadMonitorへの参照はnon-owningで、MainWindowより先に破棄されない前提。
// chartWidget()はウィンドウが非表示のままでも異常停止時のPNG自動保存
// （LoadMonitorChartWidget::renderToPixmap()はウィジェットの実際の表示
// 状態に関わらず指定サイズで描画する）に使えるよう、常に構築済みを保証する。
class LoadMonitorWindow : public QWidget
{
    Q_OBJECT

public:
    // monitor: non-owning。MainWindowが所有するLoadMonitorへのポインタを
    // そのまま渡す（このウィンドウより長生きする前提）。
    explicit LoadMonitorWindow(LoadMonitor *monitor, QWidget *parent = nullptr);

    // 「負荷モニターを有効にする」チェックボックスの状態 -- QWidget自身の
    // isEnabled()/setEnabled()（ウィジェット自体の有効/無効）と紛らわしく
    // ならないよう別名にしている。
    bool monitoringEnabled() const;
    void setMonitoringEnabled(bool enabled);
    int intervalMs() const;
    void setIntervalMs(int ms);

    // MainWindowが異常停止時のPNG自動保存・手動保存の両方で使う。このウィンドウ
    // が一度も表示されていなくても常に非nullptr（コンストラクタで必ず構築）。
    LoadMonitorChartWidget *chartWidget() const { return m_chartWidget; }

signals:
    // 「負荷モニターのグラフを保存...」ボタンが押された -- 実際のファイル
    // ダイアログ表示・保存処理はMainWindow::onSaveLoadMonitorData()が行う
    // （ログパネルへの出力等、MainWindow側の機能に依存するため）。
    void saveRequested();

    // 「負荷モニターを有効にする」チェックボックスの状態が変化した（ユーザー
    // のクリック、またはsetMonitoringEnabled()経由のプリセット読込のいずれでも）。
    // 「Mac上で負荷モニターを開いて...オンにしてもデータなしのまま...テスト
    // 実行中でないと...有効ではないですか？」との指摘を受け、MainWindow側で
    // これを購読してテスト実行の有無に関わらず即座にLoadMonitor::start()/
    // stop()するようにした -- このウィンドウ自身は「どのpidを監視するか」を
    // 知らない（①対象選択の状態はMainWindowが持つ）ため、実際の開始/停止は
    // MainWindowに委ねる。
    void monitoringToggled(bool enabled);

private:
    QCheckBox *m_enabledCheck = nullptr;
    QSpinBox *m_intervalSpin = nullptr;
    LoadMonitorChartWidget *m_chartWidget = nullptr;
    QPushButton *m_saveButton = nullptr;
};
