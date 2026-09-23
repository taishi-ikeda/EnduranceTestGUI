#include "I18n.h"

#include <QHash>
#include <QSettings>

namespace I18n
{
namespace
{

// See I18n.h for the design rationale. Every value here must preserve the
// %1/%2/... placeholders of its key exactly (same count, same meaning/order
// -- Qt's QString::arg() calls at each site are unaware of which language is
// in effect and just fill them in positionally) so translated log/status
// messages keep interpolating the same runtime values as their Japanese
// originals.
const QHash<QString, QString> &translationTable()
{
    static const QHash<QString, QString> table = {
        {QStringLiteral(" [ウィンドウ追従]"), QStringLiteral(" [Follows window]")},
        {QStringLiteral(" [対象ウィンドウ相対: (%1, %2)]"), QStringLiteral(" [Relative to target window: (%1, %2)]")},
        {QStringLiteral(" | [カスタム設定]"), QStringLiteral(" | [Custom settings]")},
        {QStringLiteral(" → メニュー項目(上から%1番目)を選択"), QStringLiteral(" → Select menu item (position %1 from top)")},
        {QStringLiteral(" → メニュー項目「%1」を選択"), QStringLiteral(" → Select menu item \"%1\"")},
        {QStringLiteral(" 操作ごと"), QStringLiteral(" actions")},
        {QStringLiteral(" 秒"), QStringLiteral(" sec")},
        {QStringLiteral("%1/操作領域_%2_%3_%4.png"), QStringLiteral("%1/region_%2_%3_%4.png")},
        {QStringLiteral("%1に有効な操作がありません"), QStringLiteral("%1 has no enabled actions")},
        {QStringLiteral("%1ステップ%2: %3 | 操作: %4 | 回数: %5%6%7"), QStringLiteral("%1Step %2: %3 | Action: %4 | Count: %5%6%7")},
        {QStringLiteral("%1ステップ%2: グループ（%3個のステップ、合計呼び出し%4回）%5"),
         QStringLiteral("%1Step %2: Group (%3 steps, %4 total calls)%5")},
        {QStringLiteral("%1ステップ%2: 待機（%3 ms）%4"), QStringLiteral("%1Step %2: Wait (%3 ms)%4")},
        {QStringLiteral("%1（矩形%2個・除外%3個）%4"), QStringLiteral("%1 (%2 rects, %3 excludes)%4")},
        {QStringLiteral("(なし)"), QStringLiteral("(None)")},
        {QStringLiteral("(ウィンドウが見つかりません)"), QStringLiteral("(Window not found)")},
        {QStringLiteral("(未選択)"), QStringLiteral("(Not selected)")},
        {QStringLiteral("==== 実行結果サマリー ===="), QStringLiteral("==== Run Summary ====")},
        {QStringLiteral("EnduranceTestGUI - GUI耐久テストツール"), QStringLiteral("EnduranceTestGUI - GUI Endurance Testing Tool")},
        {QStringLiteral("EnduranceTestGUIについて"), QStringLiteral("About EnduranceTestGUI")},
        {QStringLiteral("EnduranceTestGUIについて..."), QStringLiteral("About EnduranceTestGUI...")},
        {QStringLiteral("JSON (*.json);;テキスト (*.txt)"), QStringLiteral("JSON (*.json);;Text (*.txt)")},
        {QStringLiteral("JSONとして解釈できませんでした: %1"), QStringLiteral("Could not parse as JSON: %1")},
        {QStringLiteral("OKを押した時点の対象ウィンドウの位置を基準点として記録します。"),
         QStringLiteral("Records the target window's position at the moment OK is pressed as the reference point.")},
        {QStringLiteral("Qtについて..."), QStringLiteral("About Qt...")},
        {QStringLiteral("ms指定"), QStringLiteral("Specify ms")},
        {QStringLiteral("‖ 一時停止"), QStringLiteral("‖ Pause")},
        {QStringLiteral("↑ 上へ"), QStringLiteral("↑ Move Up")},
        {QStringLiteral("↓ 下へ"), QStringLiteral("↓ Move Down")},
        {QStringLiteral("① 対象選択"), QStringLiteral("① Target Selection")},
        {QStringLiteral("② ステップ構成"), QStringLiteral("② Steps")},
        {QStringLiteral("②で「デフォルトを使う」になっているすべてのステップに適用されます。"),
         QStringLiteral("Applies to every step in ② that is set to \"Use default\".")},
        {QStringLiteral("③ 操作パラメータ"), QStringLiteral("③ Action Parameters")},
        {QStringLiteral("■ 停止 (STOP)"), QStringLiteral("■ Stop (STOP)")},
        {QStringLiteral("■ 停止"), QStringLiteral("■ Stop")},
        {QStringLiteral("▶ 再開"), QStringLiteral("▶ Resume")},
        {QStringLiteral("▶ 実行中 "), QStringLiteral("▶ Running ")},
        {QStringLiteral("▶ 開始"), QStringLiteral("▶ Start")},
        {QStringLiteral("✓ 入力送信の権限は許可されています"), QStringLiteral("✓ Permission to send input is granted")},
        {QStringLiteral("✗ 権限が必要です（下のボタンから設定を開いてください）"),
         QStringLiteral("✗ Permission required (open settings via the button below)")},
        {QStringLiteral("〜"), QStringLiteral(" - ")},
        {QStringLiteral("このステップの操作種別・重み・回数（②でステップを選択すると編集できます）"),
         QStringLiteral("This step's action types, weights, and counts (editable by selecting a step in ②)")},
        {QStringLiteral("このステップ専用の設定を使う"), QStringLiteral("Use settings specific to this step")},
        {QStringLiteral("この操作領域内でクリックしたくない除外(マスク)矩形があれば指定してください（任意、複数可）:"),
         QStringLiteral("Specify any exclude (mask) rectangles within this region you don't want clicked (optional, multiple allowed):")},
        {QStringLiteral("すべて削除"), QStringLiteral("Delete All")},
        {QStringLiteral("ウィンドウを最大化"), QStringLiteral("Maximize Window")},
        {QStringLiteral("ウィンドウを最小化"), QStringLiteral("Minimize Window")},
        {QStringLiteral("ウィンドウリサイズ → %1 x %2"), QStringLiteral("Window resize → %1 x %2")},
        {QStringLiteral("ウィンドウ操作"), QStringLiteral("Window Operation")},
        {QStringLiteral("ウィンドウ操作\n（移動/リサイズ/最小化/最大化）"),
         QStringLiteral("Window Operation\n(Move/Resize/Minimize/Maximize)")},
        {QStringLiteral("ウィンドウ操作で許可する種類:"), QStringLiteral("Allowed window operation types:")},
        {QStringLiteral("ウィンドウ操作の種類が選択されていません"), QStringLiteral("No window operation type is selected")},
        {QStringLiteral("ウィンドウ移動 → (%1, %2)"), QStringLiteral("Window move → (%1, %2)")},
        {QStringLiteral("キー入力 '%1'"), QStringLiteral("Key input '%1'")},
        {QStringLiteral("キー入力 [%1]"), QStringLiteral("Key input [%1]")},
        {QStringLiteral("キー入力"), QStringLiteral("Key Input")},
        {QStringLiteral("キー入力の使用文字:"), QStringLiteral("Characters used for key input:")},
        {QStringLiteral("キー入力の候補がありません（使用文字・名前付きキーのいずれも未設定）"),
         QStringLiteral("No key input candidates (neither characters nor named keys are configured)")},
        {QStringLiteral("クリック"), QStringLiteral("Click")},
        {QStringLiteral("クリック(%1) at (%2, %3)"), QStringLiteral("Click(%1) at (%2, %3)")},
        {QStringLiteral("グループにステップを最低1つ追加してください。"), QStringLiteral("Please add at least one step to the group.")},
        {QStringLiteral("グループの編集"), QStringLiteral("Edit Group")},
        {QStringLiteral("グループ内での重み（大きいほど選ばれやすい）:"),
         QStringLiteral("Weight within the group (higher = more likely to be picked):")},
        {QStringLiteral("グループ化"), QStringLiteral("Group")},
        {QStringLiteral("グループ化できません"), QStringLiteral("Cannot Group")},
        {QStringLiteral("グループ解除"), QStringLiteral("Ungroup")},
        {QStringLiteral("グローバル緊急停止ホットキーが押されました。"), QStringLiteral("The global emergency-stop hotkey was pressed.")},
        {QStringLiteral("グローバル緊急停止ホットキー（Ctrl+Alt+Shift+Esc）を登録しました。"),
         QStringLiteral("Registered the global emergency-stop hotkey (Ctrl+Alt+Shift+Esc).")},
        {QStringLiteral("コピー"), QStringLiteral("Copy")},
        {QStringLiteral("ショートカット '%1' を解釈できませんでした"), QStringLiteral("Could not interpret shortcut '%1'")},
        {QStringLiteral("ショートカット '%1'"), QStringLiteral("Shortcut '%1'")},
        {QStringLiteral("ショートカット"), QStringLiteral("Shortcut")},
        {QStringLiteral("ショートカットが設定されていません"), QStringLiteral("No shortcut is configured")},
        {QStringLiteral("ショートカットキー"), QStringLiteral("Shortcut Key")},
        {QStringLiteral("ショートカットキー一覧（例: Ctrl+C, Ctrl+Shift+Z — macOSではCtrlはCmdとして送信されます）:"),
         QStringLiteral("Shortcut key list (e.g. Ctrl+C, Ctrl+Shift+Z — on macOS, Ctrl is sent as Cmd):")},
        {QStringLiteral("シーケンス %1 回目の実行を開始します"), QStringLiteral("Starting sequence run #%1")},
        {QStringLiteral("シーケンスの繰り返し回数の上限に達したため停止しました"),
         QStringLiteral("Stopped because the sequence repeat-count limit was reached")},
        {QStringLiteral("スクロール(上) at (%1, %2) dy=%3"), QStringLiteral("Scroll(Up) at (%1, %2) dy=%3")},
        {QStringLiteral("スクロール(上)"), QStringLiteral("Scroll(Up)")},
        {QStringLiteral("スクロール(下) at (%1, %2) dy=%3"), QStringLiteral("Scroll(Down) at (%1, %2) dy=%3")},
        {QStringLiteral("スクロール(下)"), QStringLiteral("Scroll(Down)")},
        {QStringLiteral("スクロール(横) at (%1, %2) dx=%3"), QStringLiteral("Scroll(Horizontal) at (%1, %2) dx=%3")},
        {QStringLiteral("スクロール(横)"), QStringLiteral("Scroll(Horizontal)")},
        {QStringLiteral("スクロール量(上):"), QStringLiteral("Scroll amount (Up):")},
        {QStringLiteral("スクロール量(下):"), QStringLiteral("Scroll amount (Down):")},
        {QStringLiteral("スクロール量(横。左右はランダム):"), QStringLiteral("Scroll amount (Horizontal; left/right chosen randomly):")},
        {QStringLiteral("スクロール（上）"), QStringLiteral("Scroll (Up)")},
        {QStringLiteral("スクロール（下）"), QStringLiteral("Scroll (Down)")},
        {QStringLiteral("スクロール（横）"), QStringLiteral("Scroll (Horizontal)")},
        {QStringLiteral("ステップ %1 の操作種別・詳細設定を編集中"), QStringLiteral("Editing action types/details for Step %1")},
        {QStringLiteral("ステップ %1 はグループです。「編集...」からメンバーを設定してください"),
         QStringLiteral("Step %1 is a group. Configure its members via \"Edit...\"")},
        {QStringLiteral("ステップ %1 は待機ステップです（操作パラメータはありません）"),
         QStringLiteral("Step %1 is a wait step (no action parameters)")},
        {QStringLiteral("ステップ %1 へ移行します"), QStringLiteral("Moving to step %1")},
        {QStringLiteral("ステップ %1"), QStringLiteral("Step %1")},
        {QStringLiteral("ステップ %1: %2 ms 待機します"), QStringLiteral("Step %1: waiting %2 ms")},
        {QStringLiteral("ステップ %1（グループ内メンバー %2）"), QStringLiteral("Step %1 (group member %2)")},
        {QStringLiteral("ステップ %1（グループ）にステップが登録されていません"),
         QStringLiteral("Step %1 (group) has no steps registered")},
        {QStringLiteral("ステップ%1"), QStringLiteral("Step%1")},
        {QStringLiteral("ステップ%1（グループ内メンバー%2）"), QStringLiteral("Step%1 (group member%2)")},
        {QStringLiteral("ステップ%1（グループ）にステップが登録されていません。"),
         QStringLiteral("Step%1 (group) has no steps registered.")},
        {QStringLiteral("ステップが変わるたび"), QStringLiteral("Every time the step changes")},
        {QStringLiteral("ステップが設定されていません"), QStringLiteral("No step is configured")},
        {QStringLiteral("ステップの操作領域を選択"), QStringLiteral("Select the Step's Operation Region")},
        {QStringLiteral("ステップの設定エラー"), QStringLiteral("Step Configuration Error")},
        {QStringLiteral("ステップを最低1つ追加してください。"), QStringLiteral("Please add at least one step.")},
        {QStringLiteral("ステップ全体（シーケンス）の繰り返し回数（必須）:"),
         QStringLiteral("Repeat count for the whole step sequence (required):")},
        {QStringLiteral("タイミング・制限"), QStringLiteral("Timing / Limits")},
        {QStringLiteral("ダブルクリック at (%1, %2)"), QStringLiteral("Double-click at (%1, %2)")},
        {QStringLiteral("ダブルクリック"), QStringLiteral("Double-click")},
        {QStringLiteral("ダブルクリック（左のみ）"), QStringLiteral("Double-click (left only)")},
        {QStringLiteral("テキストファイル (*.txt)"), QStringLiteral("Text File (*.txt)")},
        {QStringLiteral("テストを一度実行すると保存できるようになります。"), QStringLiteral("This becomes available once you run a test.")},
        {QStringLiteral("テストを開始しました（ステップ数: %1）"), QStringLiteral("Started the test (steps: %1)")},
        {QStringLiteral("テスト実行中に操作領域のスクリーンショットが撮影されると保存できるようになります。"),
         QStringLiteral("This becomes available once an operation-region screenshot is captured during a run.")},
        {QStringLiteral("テスト設定を保存"), QStringLiteral("Save Test Settings")},
        {QStringLiteral("テスト設定を保存..."), QStringLiteral("Save Test Settings...")},
        {QStringLiteral("テスト設定を保存しました: %1"), QStringLiteral("Saved test settings: %1")},
        {QStringLiteral("テスト設定を読み込みました: %1"), QStringLiteral("Loaded test settings: %1")},
        {QStringLiteral("テスト設定を読み込む"), QStringLiteral("Load Test Settings")},
        {QStringLiteral("テスト設定を読み込む..."), QStringLiteral("Load Test Settings...")},
        {QStringLiteral("デフォルト"), QStringLiteral("Default")},
        {QStringLiteral("デフォルトの操作の詳細設定"), QStringLiteral("Default Action Details")},
        {QStringLiteral("デフォルトの操作種別・重み・回数"), QStringLiteral("Default Action Types, Weights, and Counts")},
        {QStringLiteral("デフォルトの操作設定"), QStringLiteral("Default Action Settings")},
        {QStringLiteral("デフォルトを使う"), QStringLiteral("Use default")},
        {QStringLiteral("デフォルト値を編集中（ステップ未選択）"), QStringLiteral("Editing default values (no step selected)")},
        {QStringLiteral("ドラッグ (%1, %2) → (%3, %4)"), QStringLiteral("Drag (%1, %2) → (%3, %4)")},
        {QStringLiteral("ドラッグ"), QStringLiteral("Drag")},
        {QStringLiteral("ドラッグ方向:"), QStringLiteral("Drag direction:")},
        {QStringLiteral("ドラッグ距離:"), QStringLiteral("Drag distance:")},
        {QStringLiteral("ファイル"), QStringLiteral("File")},
        {QStringLiteral("ファイルに書き込めませんでした。"), QStringLiteral("Could not write the file.")},
        {QStringLiteral("ファイルを開けませんでした。"), QStringLiteral("Could not open the file.")},
        {QStringLiteral("ヘルプ"), QStringLiteral("Help")},
        {QStringLiteral("メモリ: %1 MB / CPU: %2%"), QStringLiteral("Memory: %1 MB / CPU: %2%")},
        {QStringLiteral("メンバー%1: %2 | 操作: %3 | 重み: %4"), QStringLiteral("Member%1: %2 | Action: %3 | Weight: %4")},
        {QStringLiteral("メンバー%1の操作種別・詳細設定を編集中"), QStringLiteral("Editing action types/details for Member%1")},
        {QStringLiteral("メンバー未選択"), QStringLiteral("No member selected")},
        {QStringLiteral("メンバー（各ステップの重みに応じてランダムに選ばれます）:"),
         QStringLiteral("Members (chosen randomly according to each one's weight):")},
        {QStringLiteral("ユーザーにより停止されました"), QStringLiteral("Stopped by the user")},
        {QStringLiteral("ランダム"), QStringLiteral("Random")},
        {QStringLiteral("リサイズ"), QStringLiteral("Resize")},
        {QStringLiteral("リソース使用状況: メモリ %1 MB, CPU %2%"), QStringLiteral("Resource usage: Memory %1 MB, CPU %2%")},
        {QStringLiteral("ログ"), QStringLiteral("Log")},
        {QStringLiteral("ログをクリア"), QStringLiteral("Clear Log")},
        {QStringLiteral("ログを保存"), QStringLiteral("Save Log")},
        {QStringLiteral("ログを保存..."), QStringLiteral("Save Log...")},
        {QStringLiteral("一定間隔ごと:"), QStringLiteral("At fixed intervals:")},
        {QStringLiteral("一時停止しました"), QStringLiteral("Paused")},
        {QStringLiteral("一時停止中"), QStringLiteral("Paused")},
        {QStringLiteral("上"), QStringLiteral("Up")},
        {QStringLiteral("上から %1 番目"), QStringLiteral("Position %1 from the top")},
        {QStringLiteral("上から何番目かで指定"), QStringLiteral("Specify by position from the top")},
        {QStringLiteral("下"), QStringLiteral("Down")},
        {QStringLiteral("不明なアプリ"), QStringLiteral("Unknown app")},
        {QStringLiteral("乱数シード: %1（クラッシュ等の再現に使う場合はこの値を記録してください）"),
         QStringLiteral("RNG seed: %1 (record this value if you need to reproduce a crash, etc.)")},
        {QStringLiteral("乱数シード（クラッシュ再現用。開始時にログに記録される）:"),
         QStringLiteral("RNG seed (for reproducing crashes; recorded in the log at start):")},
        {QStringLiteral("使用した乱数シード: %1"), QStringLiteral("RNG seed used: %1")},
        {QStringLiteral("例: メインメニュー"), QStringLiteral("e.g. Main Menu")},
        {QStringLiteral("保存エラー"), QStringLiteral("Save Error")},
        {QStringLiteral("候補項目名（開いたメニューにあるものの中からランダムに1つ選択。無ければメニューを閉じる):"),
         QStringLiteral("Candidate item names (one is chosen at random from those in the opened menu; the menu is closed if none match):")},
        {QStringLiteral("停止: %1"), QStringLiteral("Stopped: %1")},
        {QStringLiteral("停止理由: %1%2"), QStringLiteral("Stop reason: %1%2")},
        {QStringLiteral("入力エラー"), QStringLiteral("Input Error")},
        {QStringLiteral("共通のデフォルト操作パラメータをダイアログで編集します（②の選択は変わりません）"),
         QStringLiteral("Edit the shared default action parameters in a dialog (the selection in ② is unchanged)")},
        {QStringLiteral("再開しました"), QStringLiteral("Resumed")},
        {QStringLiteral("削除"), QStringLiteral("Delete")},
        {QStringLiteral("削除できません"), QStringLiteral("Cannot Delete")},
        {QStringLiteral("右"), QStringLiteral("Right")},
        {QStringLiteral("右クリック後にメニュー項目を選択する\n（実験的機能。macOS/Linuxのアクセシビリティ機能に依存）"),
         QStringLiteral("Select a menu item after right-clicking\n(Experimental. Relies on macOS/Linux accessibility features)")},
        {QStringLiteral("同じ名前の操作領域が既に存在します。"), QStringLiteral("An operation region with this name already exists.")},
        {QStringLiteral("名前:"), QStringLiteral("Name:")},
        {QStringLiteral("名前を入力してください。"), QStringLiteral("Please enter a name.")},
        {QStringLiteral("名前付きキーも候補に含める:"), QStringLiteral("Also include named keys as candidates:")},
        {QStringLiteral("含める領域を選択中"), QStringLiteral("Selecting Include Region")},
        {QStringLiteral("回/秒"), QStringLiteral("/sec")},
        {QStringLiteral("回数/秒指定"), QStringLiteral("Specify actions/sec")},
        {QStringLiteral("回数制限に達したため停止しました"), QStringLiteral("Stopped because the action-count limit was reached")},
        {QStringLiteral("安全確認に失敗しました"), QStringLiteral("Safety check failed")},
        {QStringLiteral("完全なログファイルを開けませんでした（画面表示のみになります）: %1"),
         QStringLiteral("Could not open the full log file (log will only be shown on screen): %1")},
        {QStringLiteral("完走したシーケンス回数: %1"), QStringLiteral("Sequence loops completed: %1")},
        {QStringLiteral("実行中"), QStringLiteral("Running")},
        {QStringLiteral("実行前に一度だけ"), QStringLiteral("Once, before the run starts")},
        {QStringLiteral("実行回数: %1"), QStringLiteral("Actions run: %1")},
        {QStringLiteral("実行回数: 0"), QStringLiteral("Actions run: 0")},
        {QStringLiteral("実行回数（全ステップ合計）: %1"), QStringLiteral("Actions run (total across all steps): %1")},
        {QStringLiteral("実行結果サマリーを保存"), QStringLiteral("Save Run Summary")},
        {QStringLiteral("実行結果サマリーを保存..."), QStringLiteral("Save Run Summary...")},
        {QStringLiteral("対象 (Target)"), QStringLiteral("Target")},
        {QStringLiteral("対象GUIの全領域"), QStringLiteral("Entire target GUI area")},
        {QStringLiteral("対象GUIの全領域（自動追従）"), QStringLiteral("Entire target GUI area (auto-follows)")},
        {QStringLiteral("対象アプリが応答していない（ハング）ことを検知したため停止しました"),
         QStringLiteral("Stopped because the target app was detected as not responding (hung)")},
        {QStringLiteral("対象アプリのものと思われるクラッシュレポートを見つけました: %1"),
         QStringLiteral("Found a crash report that appears to belong to the target app: %1")},
        {QStringLiteral("このファイルは現在のユーザー権限では読み取れません（root権限、またはcoredumpctl等の"
                        "専用ツールが必要な場合があります）。パスの記録のみ行いました。"),
         QStringLiteral("This file cannot be read with the current user's permissions (root privileges, or a "
                        "dedicated tool such as coredumpctl, may be required). Only the path has been recorded.")},
        {QStringLiteral("対象アプリの応答確認に失敗しました（%1回連続）"),
         QStringLiteral("Failed to confirm the target app is responding (%1 times in a row)")},
        {QStringLiteral("対象アプリのクラッシュを検知しました"), QStringLiteral("Target App Crash Detected")},
        {QStringLiteral("%1\n\n実行回数: %2\n乱数シード: %3\n\n"
                        "異常停止時の記録（設定・操作領域画像・ログ等）は自動保存されています。"
                        "詳細はログ欄を確認してください。"),
         QStringLiteral("%1\n\nActions run: %2\nRNG seed: %3\n\n"
                        "The abnormal-stop records (settings, operation-region image, log, etc.) have been "
                        "saved automatically. See the log for details.")},
        {QStringLiteral("対象アプリを常に最前面に保つ"), QStringLiteral("Always keep the target app in front")},
        {QStringLiteral("%1の実行中に対象アプリケーションの異常終了（クラッシュ）を検知したため停止しました"),
         QStringLiteral("Detected the target application's abnormal termination (crash) while %1 was running, and stopped")},
        {QStringLiteral("対象ウィンドウが見つからないため停止しました"), QStringLiteral("Stopped because the target window could not be found")},
        {QStringLiteral("対象ウィンドウの移動に追従させる\n（保存時の対象ウィンドウ位置を基準に記録）"),
         QStringLiteral("Follow the target window if it moves\n(recorded relative to its position when saved)")},
        {QStringLiteral("対象ウィンドウを選択してください。"), QStringLiteral("Please select a target window.")},
        {QStringLiteral("左"), QStringLiteral("Left")},
        {QStringLiteral("待機を追加..."), QStringLiteral("Add Wait...")},
        {QStringLiteral("待機ステップを編集"), QStringLiteral("Edit Wait Step")},
        {QStringLiteral("待機ステップを追加"), QStringLiteral("Add Wait Step")},
        {QStringLiteral("待機中"), QStringLiteral("Waiting")},
        {QStringLiteral("待機時間 (ms):"), QStringLiteral("Wait time (ms):")},
        {QStringLiteral("操作の詳細設定（ドラッグ距離・キー文字種・スクロール量など）"),
         QStringLiteral("Action details (drag distance, key character set, scroll amount, etc.)")},
        {QStringLiteral("操作回数:"), QStringLiteral("Action count:")},
        {QStringLiteral("操作種別ごとの回数: (なし)"), QStringLiteral("Count per action type: (None)")},
        {QStringLiteral("操作種別ごとの回数:"), QStringLiteral("Count per action type:")},
        {QStringLiteral("操作間隔:"), QStringLiteral("Action interval:")},
        {QStringLiteral("操作領域%1"), QStringLiteral("Region%1")},
        {QStringLiteral("操作領域「%1」"), QStringLiteral("Operation region \"%1\"")},
        {QStringLiteral("操作領域が選択されていません。"), QStringLiteral("No operation region is selected.")},
        {QStringLiteral("操作領域の設定"), QStringLiteral("Operation Region Settings")},
        {QStringLiteral("操作領域スクリーンショットの撮影タイミング:"), QStringLiteral("When to capture operation-region screenshots:")},
        {QStringLiteral("操作領域画像の保存先フォルダを選択"), QStringLiteral("Select the Folder to Save the Operation-Region Image")},
        {QStringLiteral("操作領域画像を保存..."), QStringLiteral("Save Operation-Region Image...")},
        {QStringLiteral("操作領域画像を保存しました: %1"), QStringLiteral("Saved the operation-region image: %1")},
        {QStringLiteral("操作領域（名前付き。②でステップ作成時に選択して使う）"),
         QStringLiteral("Operation Regions (named; selected when creating a step in ②)")},
        {QStringLiteral("時間制限に達したため停止しました"), QStringLiteral("Stopped because the time limit was reached")},
        {QStringLiteral("更新"), QStringLiteral("Update")},
        {QStringLiteral("最大化"), QStringLiteral("Maximize")},
        {QStringLiteral("最大実行回数（全ステップ合計、必須）:"), QStringLiteral("Max action count (total across all steps, required):")},
        {QStringLiteral("最大実行時間（必須）:"), QStringLiteral("Max run time (required):")},
        {QStringLiteral("最小化"), QStringLiteral("Minimize")},
        {QStringLiteral("有効な座標が見つかりませんでした（除外領域が広すぎる可能性があります）"),
         QStringLiteral("No valid coordinate could be found (the exclude region may be too large)")},
        {QStringLiteral("権限が必要です"), QStringLiteral("Permission Required")},
        {QStringLiteral("権限設定を開く"), QStringLiteral("Open Permission Settings")},
        {QStringLiteral("現在の起動時セットアップ・操作領域・ステップ構成は読み込んだ内容で上書きされます。よろしいですか？"),
         QStringLiteral("The current startup setup, operation regions, and step configuration will be overwritten with the loaded content. Continue?")},
        {QStringLiteral("画像を保存できませんでした。"), QStringLiteral("Could not save the image.")},
        {QStringLiteral("画面録画（直近%1秒分をリングバッファ保持）を有効にしました"),
         QStringLiteral("Enabled screen recording (keeping a ring buffer of the last %1 seconds)")},
        {QStringLiteral("異常停止時にOSのクラッシュダンプ・診断ログを収集する"),
         QStringLiteral("Collect OS crash dumps/diagnostic logs on abnormal stop")},
        {QStringLiteral("異常停止時に直近の画面を録画として保存する（追加負荷あり）"),
         QStringLiteral("Save a recording of the recent screen on abnormal stop (adds overhead)")},
        {QStringLiteral("異常停止時の記録の保存先作成に失敗しました: %1"),
         QStringLiteral("Failed to create the save location for abnormal-stop records: %1")},
        {QStringLiteral("異常停止時の記録一式: %1 内の「anomaly_%2」で始まるファイル/フォルダ"),
         QStringLiteral("Full abnormal-stop record set: files/folders starting with \"anomaly_%2\" inside %1")},
        {QStringLiteral("異常停止時点の操作領域画像を保存しました: %1"),
         QStringLiteral("Saved the operation-region image at the moment of abnormal stop: %1")},
        {QStringLiteral("異常停止直前の画面録画（%1フレーム、約%2秒分）を保存しました: %3"),
         QStringLiteral("Saved the screen recording from just before the abnormal stop (%1 frames, approx. %2 sec): %3")},
        {QStringLiteral("異常検知時のスクリーンショットを保存しました: %1"),
         QStringLiteral("Saved the screenshot taken on anomaly detection: %1")},
        {QStringLiteral("登録済みの操作領域から選択:"), QStringLiteral("Select from registered operation regions:")},
        {QStringLiteral("矢印キー"), QStringLiteral("Arrow keys")},
        {QStringLiteral("矩形"), QStringLiteral("Rectangle")},
        {QStringLiteral("矩形を描画..."), QStringLiteral("Draw Rectangle...")},
        {QStringLiteral("矩形を画面上で描画してください（複数可）:"), QStringLiteral("Please draw rectangles on screen (multiple allowed):")},
        {QStringLiteral("確認"), QStringLiteral("Confirm")},
        {QStringLiteral("移動"), QStringLiteral("Move")},
        {QStringLiteral("経過: %1秒"), QStringLiteral("Elapsed: %1 sec")},
        {QStringLiteral("経過: 0秒"), QStringLiteral("Elapsed: 0 sec")},
        {QStringLiteral("経過時間: %1 秒"), QStringLiteral("Elapsed time: %1 sec")},
        {QStringLiteral("編集..."), QStringLiteral("Edit...")},
        {QStringLiteral("設定エラー"), QStringLiteral("Configuration Error")},
        {QStringLiteral("読み込みエラー"), QStringLiteral("Load Error")},
        {QStringLiteral("追加"), QStringLiteral("Add")},
        {QStringLiteral("追加..."), QStringLiteral("Add...")},
        {QStringLiteral("選択を削除"), QStringLiteral("Delete Selection")},
        {QStringLiteral("重み"), QStringLiteral("Weight")},
        {QStringLiteral("録画フレームの保存先作成に失敗しました: %1"),
         QStringLiteral("Failed to create the save location for recording frames: %1")},
        {QStringLiteral("除外"), QStringLiteral("Exclude")},
        {QStringLiteral("除外(マスク)領域を選択中"), QStringLiteral("Selecting Exclude (Mask) Region")},
        {QStringLiteral("除外矩形を描画..."), QStringLiteral("Draw Exclude Rectangle...")},
        {QStringLiteral("項目名で指定"), QStringLiteral("Specify by item name")},
        {QStringLiteral("領域ごとに操作種別・回数を指定し、順番に繰り返し実行"),
         QStringLiteral("Specify an action type and count per region, and repeat them in order")},
        {QStringLiteral("領域を最低1つ描画してください。"), QStringLiteral("Please draw at least one rectangle.")},
        {QStringLiteral("（①対象選択パネルで操作領域を追加してください）"),
         QStringLiteral(" (Add an operation region in the ① Target Selection panel)")},
        {QStringLiteral("（異常停止）"), QStringLiteral(" (Abnormal Stop)")},
        {QStringLiteral("グローバル緊急停止ホットキーを登録できませんでした"
                                    "（他のアプリが同じ組み合わせを使用している可能性があります）。"
                                    "「■ 停止」ボタンは通常どおり使用できます。"), QStringLiteral("Could not register the global emergency-stop hotkey (another app may already be using the same combination). The \"■ Stop\" button can still be used as normal.")},
        {QStringLiteral("EnduranceTestGUI\n\n"
            "GUIアプリケーションの耐久テスト（ランダムクリック・ランダム操作）を行うための"
            "デスクトップツールです。\n\n"
            "Qt %1 を使用して構築されています。Qtはフリーソフトウェア版の場合"
            "GNU LGPL バージョン3（一部モジュールはGPL）の下で配布されています。Qt自体の"
            "ライセンス条文は、このメニューの「Qtについて...」から確認できます。"), QStringLiteral("EnduranceTestGUI\n\nA desktop tool for endurance-testing GUI applications (random clicks / random actions).\n\nBuilt using Qt %1. The free-software edition of Qt is distributed under the GNU LGPL version 3 (some modules under the GPL). Qt's own license text can be viewed via \"About Qt...\" in this menu.")},
        {QStringLiteral("実行中、画面を一定間隔で撮影してリングバッファに保持し続けます。"
                        "異常停止時に、そこまでの数秒間の画面推移を連番PNGとして保存します。"), QStringLiteral("While running, periodically captures the screen and keeps it in a ring buffer. On an abnormal stop, saves the preceding few seconds of screen activity as numbered PNGs.")},
        {QStringLiteral("対象アプリのものと思われるクラッシュレポート/コアダンプがシステム上に"
                        "見つかった場合、そのパスを異常停止時のログと記録一式に含めます。"
                        "見つからなくても実行には影響しません。"), QStringLiteral("If a crash report/core dump that appears to belong to the target app is found on the system, its path is included in the abnormal-stop log and record set. Not finding one has no effect on the run.")},
        {QStringLiteral("この操作領域は次のステップで使われているため削除できません: %1\n"
                            "先にそれらのステップの領域を変更するか、ステップを削除してください。"), QStringLiteral("This operation region cannot be deleted because it is used by the following steps: %1\nPlease change those steps' region or delete the steps first.")},
        {QStringLiteral("%1は操作種別が選択されていません。②でこのステップを選択し、③操作パラメータ"
            "パネルで操作種別を1つ以上有効にしてください。"), QStringLiteral("%1 has no action type selected. Select this step in ② and enable at least one action type in the ③ Action Parameters panel.")},
        {QStringLiteral("キー入力を有効にした%1があります。使用文字を指定してください"
                                        "（デフォルトまたはそのステップの専用設定）。"), QStringLiteral("%1 has key input enabled. Please specify characters to use (in the default or that step's own settings).")},
        {QStringLiteral("ショートカットキーを有効にした%1があります。ショートカットを最低1つ追加してください"
            "（デフォルトまたはそのステップの専用設定）。"), QStringLiteral("%1 has shortcut keys enabled. Please add at least one shortcut (in the default or that step's own settings).")},
        {QStringLiteral("メニュー項目選択（項目名指定）を有効にした%1があります。候補項目名を最低1つ"
                "追加してください（デフォルトまたはそのステップの専用設定）。"), QStringLiteral("%1 has menu item selection (by item name) enabled. Please add at least one candidate item name (in the default or that step's own settings).")},
        {QStringLiteral("メニュー項目選択（番号指定）を有効にした%1があります。候補の番号を最低1つ"
                "追加してください（デフォルトまたはそのステップの専用設定）。"), QStringLiteral("%1 has menu item selection (by position) enabled. Please add at least one candidate position (in the default or that step's own settings).")},
        {QStringLiteral("他のアプリケーションを操作するための権限が許可されていません。"
                                              "設定を許可してから、もう一度「開始」を押してください。"), QStringLiteral("Permission to control other applications has not been granted. Please grant it in settings, then press \"Start\" again.")},
        {QStringLiteral("対象ウィンドウが安全に操作できることを確認できなかったため、開始できません。\n\n"
                    "対象ウィンドウが他のウィンドウに覆われていないか、最小化されていないか確認して"
                    "ください。それでも解決しない場合、この環境（特にLinuxの一部のウィンドウマネージャ）"
                    "では安全チェック機能自体が動作しない可能性があります（詳細はSPEC.md参照）。"), QStringLiteral("Cannot start because it could not be confirmed that the target window can be operated safely.\n\nPlease check that the target window is not covered by another window and is not minimized. If that doesn't resolve it, this environment (particularly some Linux window managers) may not support the safety-check feature itself (see SPEC.md for details).")},
        {QStringLiteral("完全なログをこのファイルに逐次保存します（画面表示とは別に、途中で"
                                  "切れることなく全操作を記録): %1"), QStringLiteral("The full log is saved incrementally to this file (recording every action without being truncated, separately from the on-screen display): %1")},
        {QStringLiteral("異常停止時のテスト設定（この乱数シードで再現できるはずです）を"
                                      "保存しました: %1"), QStringLiteral("Saved the test settings at the time of the abnormal stop (should be reproducible with this RNG seed): %1")},
        {QStringLiteral("テスト設定を読み込みました: %1（保存時の対象アプリ: %2 -- "
                                    "①で対象ウィンドウを選び直してください）"), QStringLiteral("Loaded test settings: %1 (target app at save time: %2 -- please reselect the target window in ①)")},
        {QStringLiteral("テスト設定を読み込みました: %1（対象アプリ「%2」を自動選択しました）"),
         QStringLiteral("Loaded test settings: %1 (automatically selected target app \"%2\")")},
        {QStringLiteral("スクリーンショットの保存に失敗しました（macOSでは画面収録の権限が必要な場合があります）"), QStringLiteral("Failed to save the screenshot (macOS may require Screen Recording permission)")},
        {QStringLiteral("対象アプリのクラッシュレポート/コアダンプは見つかりませんでした"
                "（このシステムでその機能自体が無効になっている可能性があります）"), QStringLiteral("No crash report/core dump was found for the target app (this system may have that feature disabled entirely)")},
        {QStringLiteral("対象アプリが応答確認（WM_PING）に一度も応答しないため、ハング検知を無効にします"
                    "（対応していないツールキット/実装の可能性があります）"), QStringLiteral("Disabling hang detection because the target app has never responded to the response check (WM_PING) (it may use a toolkit/implementation that doesn't support it)")},
        {QStringLiteral("メニュー選択の直前に対象アプリがアクティブでなくなったため、安全のため"
                   "テストを停止しました"), QStringLiteral("Stopped the test for safety because the target app stopped being active right before the menu selection")},
        {QStringLiteral("対象アプリに予期しないウィンドウ（ダイアログ等）が開いたまま閉じられない"
                              "ため、安全のためテストを停止しました"), QStringLiteral("Stopped the test for safety because an unexpected window (a dialog, etc.) appeared on the target app and could not be closed")},
        {QStringLiteral("対象アプリに予期しないウィンドウ（ダイアログ等）を検出しました。Escapeで閉じてみます"
        "（%1/%2回目）"), QStringLiteral("Detected an unexpected window (a dialog, etc.) on the target app. Trying to close it with Escape (%1/%2)")},
        {QStringLiteral("%1の対象領域が見つからないため停止しました（対象ウィンドウが"
                              "消失した、または参照している操作領域が削除された可能性があります）"), QStringLiteral("Stopped because %1's target region could not be found (the target window may have disappeared, or the referenced operation region may have been deleted)")},
        {QStringLiteral("対象アプリ以外のウィンドウを操作しそうになったため、安全のためテストを停止しました"), QStringLiteral("Stopped the test for safety because it was about to operate a window other than the target app")},
        {QStringLiteral("対象アプリがアクティブでないため（キー入力が他アプリに送られる可能性があるため）、"
                       "安全のためテストを停止しました"), QStringLiteral("Stopped the test for safety because the target app is not active (key input could otherwise be sent to a different app)")},
        {QStringLiteral("ドラッグ先が対象アプリ以外のウィンドウになりそうなため、安全のためテストを停止しました"), QStringLiteral("Stopped the test for safety because the drag destination was about to be a window other than the target app")},
        {QStringLiteral("グループ全体の合計呼び出し回数（毎回ランダムに選ばれたメンバーが\n"
                        "1操作ずつ実行され、この回数に達すると次のステップへ進みます）:"), QStringLiteral("Total call count for the whole group (each time, a randomly chosen member\nexecutes one action; once this count is reached, it moves to the next step):")},
        {QStringLiteral("※「デフォルトを使う」場合の値は参照のみです。変更するには①対象選択の"
                        "「デフォルト」ボタンを使ってください。"), QStringLiteral("* When \"Use default\" is selected, these values are shown for reference only. To change them, use the \"Default\" button in ① Target Selection.")},
        {QStringLiteral("候補の番号（上から何番目か、1始まり）。開いたメニューの項目数に収まるものの中からランダムに1つ選択。"
            "無ければメニューを閉じる。※メニューの項目数や並びが状況によって変わる場合、意図しない項目を"
            "選んでしまう可能性があるため注意（項目名指定の方が安全）:"), QStringLiteral("Candidate positions (from the top, 1-based). One is chosen at random from those that fit within the opened menu's item count. The menu is closed if none fit. Note: if the menu's item count or order can vary, this may pick an unintended item (specifying by item name is safer):")},
        {QStringLiteral("対象ウィンドウが選択されていないため、今は変更できません"
                                     "（既存の設定はそのまま保持されます）。"), QStringLiteral("Cannot change this right now because no target window is selected (the existing setting is kept as-is).")},
        {QStringLiteral("このステップで操作する領域を選択してください。\n"
                                    "操作の種類・重み・回数や詳細パラメータは、追加後に③操作パラメータ"
                                    "パネルでこのステップを選択して設定します。"), QStringLiteral("Please select the region this step will operate on.\nThe action types, weights, counts, and detailed parameters are set afterward by selecting this step in the ③ Action Parameters panel.")},
        {QStringLiteral("②で新しくステップを追加したときの初期値です。既存のステップには"
                        "影響しません。"), QStringLiteral("These are the initial values used when a new step is added in ②. They do not affect existing steps.")},
        {QStringLiteral("%1  ―  ドラッグで矩形を追加（複数可） / Enter または右クリックで確定 / Esc でキャンセル"), QStringLiteral("%1  ―  Drag to add a rectangle (multiple allowed) / Enter or right-click to confirm / Esc to cancel")},

        // SPEC.md 10 ③④⑤: crash-rate statistics (TestStatistics/StatisticsDialog).
        {QStringLiteral("直近の操作（古い順、確率的な不具合の解析用）:"), QStringLiteral("Recent actions (oldest first, for diagnosing probabilistic bugs):")},
        {QStringLiteral(" | ⚠ クラッシュ %1/%2回"), QStringLiteral(" | ⚠ Crashed %1/%2 runs")},
        {QStringLiteral("統計..."), QStringLiteral("Statistics...")},
        {QStringLiteral("このテスト設定: %1回中%2回クラッシュ（%3%）"), QStringLiteral("This test setup: %2/%1 runs crashed (%3%)")},
        {QStringLiteral("このテスト設定での実行記録はまだありません。"), QStringLiteral("No runs have been recorded yet for this test setup.")},
        {QStringLiteral("統計（クラッシュ率）"), QStringLiteral("Statistics (Crash Rate)")},
        {QStringLiteral("現在読み込まれているテスト設定（①②③の内容）について、これまでに"
                        "記録された全実行結果の集計です。手動で対象アプリを再起動して同じ"
                        "テストを繰り返した場合も、自動連続実行を使った場合も、同じ集計に"
                        "含まれます。"),
         QStringLiteral("Aggregated results for the currently loaded test setup (①②③), across every "
                        "recorded run. Runs where you manually relaunched the target app and repeated "
                        "the same test are counted the same as runs started by the automatic batch loop.")},
        {QStringLiteral("ステップ"), QStringLiteral("Step")},
        {QStringLiteral("クラッシュ回数"), QStringLiteral("Crash Count")},
        {QStringLiteral("全試行回数に対する割合"), QStringLiteral("Share of Total Attempts")},
        {QStringLiteral("CSVエクスポート..."), QStringLiteral("Export CSV...")},
        {QStringLiteral("この統計をリセット..."), QStringLiteral("Reset This Statistics...")},
        {QStringLiteral("このテスト設定での実行記録はまだありません。「開始」でテストを実行すると"
                        "ここに集計されます。"),
         QStringLiteral("No runs have been recorded yet for this test setup. Running a test with "
                        "\"Start\" will add to these statistics.")},
        {QStringLiteral("試行回数: %1回\nクラッシュ回数: %2回（クラッシュ率 %3%）\n"
                        "クラッシュまでの平均実行回数: %4回\nクラッシュまでの平均経過時間: %5秒"),
         QStringLiteral("Attempts: %1\nCrashed runs: %2 (crash rate %3%)\n"
                        "Mean actions before crash: %4\nMean elapsed time before crash: %5 sec")},
        {QStringLiteral("ステップ %1（現在の構成には存在しません）"), QStringLiteral("Step %1 (not present in the current setup)")},
        {QStringLiteral("%1/%2回 (%3%)"), QStringLiteral("%1/%2 (%3%)")},
        {QStringLiteral("統計をCSVでエクスポート"), QStringLiteral("Export Statistics as CSV")},
        {QStringLiteral("CSV (*.csv)"), QStringLiteral("CSV (*.csv)")},
        {QStringLiteral("エクスポート完了"), QStringLiteral("Export Complete")},
        {QStringLiteral("記録されている全テスト設定分の実行履歴をCSVに書き出しました: %1"),
         QStringLiteral("Exported the run history for every recorded test setup to CSV: %1")},
        {QStringLiteral("エクスポートエラー"), QStringLiteral("Export Error")},
        {QStringLiteral("このテスト設定についてこれまでに記録された統計（試行回数・クラッシュ回数・"
                        "ステップ別内訳）をすべて削除します。よろしいですか？"
                        "（他のテスト設定の統計には影響しません）"),
         QStringLiteral("This will delete all recorded statistics for this test setup (attempts, crash "
                        "count, per-step breakdown). Are you sure? (Other test setups' statistics are not affected.)")},

        // SPEC.md 10 ①②: batch mode (consecutive automatic runs) and the
        // optional target-app auto-launch command.
        {QStringLiteral("連続実行回数:"), QStringLiteral("Batch run count:")},
        {QStringLiteral("対象アプリの自動起動コマンド（任意、①の連続実行で使用）:"),
         QStringLiteral("Target app auto-launch command (optional, used by batch mode):")},
        {QStringLiteral("例: /path/to/TestTarget --option"), QStringLiteral("e.g. /path/to/TestTarget --option")},
        {QStringLiteral("参照..."), QStringLiteral("Browse...")},
        {QStringLiteral("今すぐ起動"), QStringLiteral("Launch Now")},
        {QStringLiteral("対象アプリの実行ファイルを選択"), QStringLiteral("Select the Target App's Executable")},
        {QStringLiteral("1より大きい値にすると、1回終わるたびに（対象アプリの再起動を待って）"
                        "自動的に次を開始し、指定回数繰り返します。"),
         QStringLiteral("Setting this above 1 automatically starts the next run (waiting for the target "
                        "app to relaunch) each time one finishes, repeating this many times.")},
        {QStringLiteral("連続実行: 権限が確認できなかったため中断しました"),
         QStringLiteral("Batch run: cancelled because the permission could not be confirmed")},
        {QStringLiteral("連続実行: 設定エラーのため中断しました: %1"),
         QStringLiteral("Batch run: cancelled due to a configuration error: %1")},
        {QStringLiteral("連続実行: 安全確認に失敗したため中断しました"),
         QStringLiteral("Batch run: cancelled because the safety check failed")},
        {QStringLiteral("連続実行: %1/%2回目を開始します"), QStringLiteral("Batch run: starting run %1/%2")},
        {QStringLiteral("連続実行: %1/%2回目"), QStringLiteral("Batch run: %1/%2")},
        {QStringLiteral("連続実行を中断しました（対象アプリの再起動待ち中でした）"),
         QStringLiteral("Batch run cancelled (was waiting for the target app to relaunch)")},
        {QStringLiteral("連続実行を中断します（現在の実行が終わり次第停止します）"),
         QStringLiteral("Cancelling the batch run (will stop once the current run finishes)")},
        {QStringLiteral("連続実行が完了しました（%1/%2回）"), QStringLiteral("Batch run complete (%1/%2)")},
        {QStringLiteral("連続実行: %1/%2回が終了しました。次の実行の準備をします..."),
         QStringLiteral("Batch run: %1/%2 finished. Preparing the next run...")},
        {QStringLiteral("連続実行: %1回目の準備中..."), QStringLiteral("Batch run: preparing run %1...")},
        {QStringLiteral("連続実行: 対象アプリが見つからないため、登録された起動コマンドで"
                        "自動的に起動します"),
         QStringLiteral("Batch run: the target app was not found, launching it automatically with the "
                        "configured command")},
        {QStringLiteral("連続実行: 対象アプリが見つかりません。手動で再起動してください"
                        "（再起動を検知したら自動的に次の実行を開始します）"),
         QStringLiteral("Batch run: the target app was not found. Please relaunch it manually (the next "
                        "run will start automatically once it's detected)")},
        {QStringLiteral("連続実行: 対象アプリの起動を検知しました。次の実行を開始します"),
         QStringLiteral("Batch run: detected the target app launching. Starting the next run")},
        {QStringLiteral("対象アプリの自動起動に失敗しました: %1"), QStringLiteral("Failed to auto-launch the target app: %1")},

        // 起動時セットアップ ("startup setup macro" -- SPEC.md 6.x): PointPickerOverlay,
        // SetupActionEditorDialog, RandomActionEngine's setup phase, MainWindow's ① UI.
        {QStringLiteral("クリックした位置を座標として使用します  ―  Esc でキャンセル"),
         QStringLiteral("The point you click will be used as the coordinate  ―  Esc to cancel")},
        {QStringLiteral("起動時セットアップ操作の設定"), QStringLiteral("Startup Setup Action Settings")},
        {QStringLiteral("種類:"), QStringLiteral("Type:")},
        {QStringLiteral("文字入力"), QStringLiteral("Text Input")},
        {QStringLiteral("右クリック"), QStringLiteral("Right-click")},
        {QStringLiteral("待機"), QStringLiteral("Wait")},
        {QStringLiteral("説明（任意）:"), QStringLiteral("Description (optional):")},
        {QStringLiteral("例: ユーザー名欄"), QStringLiteral("e.g. Username field")},
        {QStringLiteral("対象ウィンドウを基準にした位置をクリックで指定してください。"),
         QStringLiteral("Click to specify a position relative to the target window.")},
        {QStringLiteral("位置を選択..."), QStringLiteral("Select Position...")},
        {QStringLiteral("対象ウィンドウを基準にしたドラッグの開始位置と終了位置をそれぞれ指定してください。"),
         QStringLiteral("Specify the drag's start and end positions, both relative to the target window.")},
        {QStringLiteral("開始位置を選択..."), QStringLiteral("Select Start Position...")},
        {QStringLiteral("終了位置を選択..."), QStringLiteral("Select End Position...")},
        {QStringLiteral("位置"), QStringLiteral("Position")},
        {QStringLiteral("開始"), QStringLiteral("Start")},
        {QStringLiteral("終了"), QStringLiteral("End")},
        {QStringLiteral("入力するテキスト:"), QStringLiteral("Text to type:")},
        {QStringLiteral("キー（例: Return, Tab, Ctrl+A）:"), QStringLiteral("Key (e.g. Return, Tab, Ctrl+A):")},
        {QStringLiteral("例: Return"), QStringLiteral("e.g. Return")},
        {QStringLiteral("待機時間（ミリ秒）:"), QStringLiteral("Wait duration (ms):")},
        {QStringLiteral("対象ウィンドウ未選択"), QStringLiteral("No Target Window Selected")},
        {QStringLiteral("対象ウィンドウを選択してから位置を指定してください。"),
         QStringLiteral("Select a target window before specifying a position.")},
        {QStringLiteral("入力するテキストを入力してください。"), QStringLiteral("Enter the text to type.")},
        {QStringLiteral("キーを入力してください。"), QStringLiteral("Enter a key.")},
        {QStringLiteral("「位置を選択...」から位置を指定してください。"),
         QStringLiteral("Specify a position via \"Select Position...\".")},
        {QStringLiteral("「終了位置を選択...」から位置を指定してください。"),
         QStringLiteral("Specify a position via \"Select End Position...\".")},
        {QStringLiteral("文字入力（%1文字、内容はログに記録しません）"),
         QStringLiteral("Text input (%1 characters, content not logged)")},
        {QStringLiteral("パスワード等も入力できます。実行ログにはこの内容自体は記録されません。"),
         QStringLiteral("Passwords and other sensitive text can be entered here too. The content "
                        "itself is never written to the run log.")},
        {QStringLiteral("起動時セットアップを開始します（%1件） -- 完了後にランダム操作を開始します"),
         QStringLiteral("Starting startup setup (%1 actions) -- random actions will begin once it's done")},
        {QStringLiteral("起動時セットアップが完了しました。ランダム操作を開始します"),
         QStringLiteral("Startup setup complete. Starting random actions")},
        {QStringLiteral("起動時セットアップ %1/%2: %3"), QStringLiteral("Startup setup %1/%2: %3")},
        {QStringLiteral("%1に失敗しました（%2）。%3回再試行しましたが解決しなかったため、安全のためテストを停止しました"),
         QStringLiteral("%1 failed (%2). Stopped the test for safety after %3 retries didn't resolve it")},
        {QStringLiteral("%1を再試行します（%2、%3/%4回目）"), QStringLiteral("Retrying %1 (%2, attempt %3/%4)")},
        {QStringLiteral("起動時セットアップ %1/%2"), QStringLiteral("Startup setup %1/%2")},
        {QStringLiteral("対象アプリがアクティブになっていません"), QStringLiteral("The target app is not active")},
        {QStringLiteral("文字入力 '%1'"), QStringLiteral("Text input '%1'")},
        {QStringLiteral("%1: キー '%2' を解釈できないため、テストを開始できません"),
         QStringLiteral("%1: Cannot start the test because the key '%2' could not be interpreted")},
        {QStringLiteral("対象ウィンドウが見つかりません"), QStringLiteral("The target window could not be found")},
        {QStringLiteral("指定位置に対象アプリのウィンドウが見つかりません"),
         QStringLiteral("The target app's window was not found at the specified position")},
        {QStringLiteral("クリック at (%1, %2)"), QStringLiteral("Click at (%1, %2)")},
        {QStringLiteral("右クリック at (%1, %2)"), QStringLiteral("Right-click at (%1, %2)")},
        {QStringLiteral("ドラッグ先に対象アプリのウィンドウが見つかりません"),
         QStringLiteral("The target app's window was not found at the drag destination")},
        {QStringLiteral("待機 %1ms"), QStringLiteral("Wait %1ms")},
        {QStringLiteral("（%1）"), QStringLiteral(" (%1)")},
        {QStringLiteral("起動時セットアップ（対象アプリ起動直後に一度だけ実行。ログイン等）"),
         QStringLiteral("Startup Setup (runs once right after the target app launches -- login, etc.)")},
        {QStringLiteral("連続自動実行（①バッチ）では初回のみ実行する"),
         QStringLiteral("Only run on the first run of consecutive auto-execution (① batch)")},
        {QStringLiteral("オフの場合、バッチの自動再起動のたびに毎回このセットアップを実行します。"
                        "オンの場合、2回目以降の自動再起動ではスキップします"
                        "（初回だけ出るライセンス同意等を想定）。"),
         QStringLiteral("If off, this setup runs every time the batch automatically restarts the target. "
                        "If on, it is skipped on the second and later automatic restarts "
                        "(for things like a one-time license agreement).")},
        {QStringLiteral("クリック (%1, %2)"), QStringLiteral("Click (%1, %2)")},
        {QStringLiteral("ダブルクリック (%1, %2)"), QStringLiteral("Double-click (%1, %2)")},
        {QStringLiteral("右クリック (%1, %2)"), QStringLiteral("Right-click (%1, %2)")},
        {QStringLiteral(" [%1]"), QStringLiteral(" [%1]")},
        {QStringLiteral("%1: %2%3"), QStringLiteral("%1: %2%3")},

        // v0.59: ▶開始の「起動してから開始する」オプション・独立した「連続実行」ボタン
        {QStringLiteral("⟳ 連続実行"), QStringLiteral("⟳ Continuous Run")},
        {QStringLiteral("対象アプリが起動中でも必ず一度終了してから新しく起動し、起動時セットアップと"
                        "ステップ構成の実行を行います。実行後に対象アプリが残っていれば終了し、"
                        "連続実行回数の分だけ繰り返します。①の自動起動コマンドの設定が必要です。"),
         QStringLiteral("Always terminates the target app first (even if it's already running) and "
                        "launches a fresh instance, then runs the startup setup and steps. If the "
                        "target app is still running afterward, it's terminated, and this repeats for "
                        "the batch run count. Requires ①'s auto-launch command to be set.")},
        {QStringLiteral("▶開始: 1より大きい値にすると、1回終わるたびに（対象アプリの再起動を待って）"
                        "自動的に次を開始し、指定回数繰り返します。\n"
                        "⟳連続実行: 常にこの回数だけ、対象アプリの終了→再起動→実行を繰り返します。"),
         QStringLiteral("▶ Start: setting this above 1 automatically starts the next run (waiting for "
                        "the target app to relaunch) each time one finishes, repeating that many "
                        "times.\n⟳ Continuous Run: always repeats terminate target -> relaunch -> run "
                        "this many times.")},
        {QStringLiteral("開始時にこのコマンドで対象ツールを起動してから開始する"),
         QStringLiteral("Launch the target tool with this command before starting")},
        {QStringLiteral("チェックすると、▶開始を押したときにまず上の自動起動コマンドで対象アプリを起動し、"
                        "起動を確認してから起動時セットアップ→ステップ構成の実行を始めます。"),
         QStringLiteral("When checked, pressing ▶ Start first launches the target app with the "
                        "auto-launch command above, then begins the startup setup and steps once its "
                        "launch is confirmed.")},
        {QStringLiteral("「開始時にこのコマンドで対象ツールを起動してから開始する」を有効にする"
                        "場合は、①に対象アプリの自動起動コマンドを設定してください。"),
         QStringLiteral("To enable \"Launch the target tool with this command before starting\", "
                        "please set the target app's auto-launch command in ①.")},
        {QStringLiteral("対象ツールを起動しています...起動を確認してから開始します"),
         QStringLiteral("Launching the target tool... will start once its launch is confirmed")},
        {QStringLiteral("連続実行を使うには、①に対象アプリの自動起動コマンドを設定してください。"),
         QStringLiteral("To use Continuous Run, please set the target app's auto-launch command in ①.")},
        {QStringLiteral("連続実行: 1回目の準備中..."), QStringLiteral("Continuous run: preparing run 1...")},
        {QStringLiteral("連続実行を開始します（対象アプリが残っている場合は終了してから起動します）"),
         QStringLiteral("Starting continuous run (if the target app is still running, it will be "
                        "terminated before launching)")},
        {QStringLiteral("連続実行を中断しました（対象アプリの終了/再起動待ち中でした）"),
         QStringLiteral("Continuous run interrupted (was waiting for the target app to exit/relaunch)")},
        {QStringLiteral("連続実行を中断しました（対象アプリの再起動待ち中でした）"),
         QStringLiteral("Batch run interrupted (was waiting for the target app to relaunch)")},
        {QStringLiteral("連続実行を中断します（現在の実行が終わり次第停止します）"),
         QStringLiteral("Interrupting the batch run (will stop once the current run finishes)")},
        {QStringLiteral("連続実行: 対象アプリ（PID %1）が残っているため終了します"),
         QStringLiteral("Continuous run: the target app (PID %1) is still running, terminating it")},
        {QStringLiteral("連続実行: 対象アプリの終了を確認しました。新しいインスタンスを"
                        "起動します"),
         QStringLiteral("Continuous run: confirmed the target app has exited. Launching a new "
                        "instance")},
        {QStringLiteral("連続実行: 対象アプリの起動を検知しました。次の実行を開始します"),
         QStringLiteral("Continuous run: detected the target app has launched. Starting the next run")},
        {QStringLiteral("対象ツールの起動を検知しました。開始します"),
         QStringLiteral("Detected the target tool has launched. Starting")},

        // v0.60: 起動時セットアップ「記録」機能 (InputRecorder/RecordingIndicatorPanel)
        {QStringLiteral("● 記録..."), QStringLiteral("● Record...")},
        {QStringLiteral("押すと、①で選択中の対象ウィンドウを基準に、次にEscキーが押されるまでの"
                        "マウスクリック・ドラッグ・キー入力（対象アプリが開くダイアログへの操作も"
                        "含む）を記録し、この一覧に追加していきます。"),
         QStringLiteral("Click to start recording mouse clicks/drags/keyboard input -- including "
                        "operating dialogs the target app opens -- relative to whichever target window "
                        "is selected in ①, and append them to this list, until Escape is pressed.")},
        {QStringLiteral("記録中"), QStringLiteral("Recording")},
        {QStringLiteral("対象が選択されていません"), QStringLiteral("No Target Selected")},
        {QStringLiteral("記録された座標は①で選択中の対象ウィンドウを基準に保存されるため、"
                        "先に①で対象アプリを選択してください。"),
         QStringLiteral("Recorded coordinates are saved relative to the target window selected in "
                        "①, so please select the target app in ① first.")},
        {QStringLiteral("記録を開始できませんでした"), QStringLiteral("Could Not Start Recording")},
        {QStringLiteral("システム全体の入力監視を開始できませんでした。OSの権限設定"
                        "（Linux: XInput2拡張が利用できるか / macOS: 入力監視の許可）"
                        "を確認してください。"),
         QStringLiteral("Could not start system-wide input observation. Please check your OS "
                        "permission settings (Linux: whether the XInput2 extension is available / "
                        "macOS: Input Monitoring permission).")},
        {QStringLiteral("起動時セットアップの記録を開始しました（Escキーで終了）"),
         QStringLiteral("Started recording the startup setup (press Escape to stop)")},
        {QStringLiteral("記録: 対象ウィンドウが見つからないため、この操作は記録されません"
                        "でした"),
         QStringLiteral("Record: the target window could not be found, so this action was not "
                        "recorded")},
        {QStringLiteral("記録: %1"), QStringLiteral("Recorded: %1")},
        {QStringLiteral("記録を終了しました（Escキー）。記録件数: %1"),
         QStringLiteral("Recording stopped (Escape). Actions recorded: %1")},
        {QStringLiteral("記録を終了しました。記録件数: %1"),
         QStringLiteral("Recording stopped. Actions recorded: %1")},
        {QStringLiteral("● 記録中... (Escで終了)"), QStringLiteral("● Recording... (Escape to stop)")},
        {QStringLiteral("記録件数: %1"), QStringLiteral("Actions recorded: %1")},
        {QStringLiteral("■ 記録終了"), QStringLiteral("■ Stop Recording")},

        // v0.61: ステップ構成「タスク」機能 (TaskEditorDialog)
        {QStringLiteral("タスク化"), QStringLiteral("Task")},
        {QStringLiteral("タスク化できません"), QStringLiteral("Cannot Task")},
        {QStringLiteral("タスク解除"), QStringLiteral("Untask")},
        {QStringLiteral("タスクの編集"), QStringLiteral("Edit Task")},
        {QStringLiteral("タスクの操作（一覧の順番通りに、毎回すべて1回ずつ実行されます。\n"
                        "タスク全体で1回分の操作としてカウントされます）:"),
         QStringLiteral("The task's actions (all run once each, in list order, every time.\n"
                        "The whole task counts as a single action):")},
        {QStringLiteral("操作（この順番で実行されます）:"), QStringLiteral("Actions (run in this order):")},
        {QStringLiteral("操作未選択"), QStringLiteral("No action selected")},
        {QStringLiteral("操作%1の操作種別・詳細設定を編集中"),
         QStringLiteral("Editing action types/details for Action%1")},
        {QStringLiteral("タスクに操作を最低1つ追加してください。"),
         QStringLiteral("Please add at least one action to the task.")},
        {QStringLiteral("%1: %2 | 操作: %3"), QStringLiteral("%1: %2 | Action: %3")},
        {QStringLiteral("%1ステップ%2: タスク（%3個の操作を順番に実行）%4"),
         QStringLiteral("%1Step%2: Task (%3 actions, run in order)%4")},
        {QStringLiteral("ステップ %1 はタスクです。「編集...」から操作を設定してください"),
         QStringLiteral("Step %1 is a task. Configure its actions via \"Edit...\"")},
        {QStringLiteral("ステップ %1（タスク内操作 %2/%3）"), QStringLiteral("Step %1 (task action %2/%3)")},
        {QStringLiteral("ステップ %1（タスク）にステップが登録されていません"),
         QStringLiteral("Step %1 (task) has no actions registered")},
        {QStringLiteral("ステップ%1（タスク内操作%2）"), QStringLiteral("Step%1 (task action%2)")},
        {QStringLiteral("ステップ%1（タスク）に操作が登録されていません。"),
         QStringLiteral("Step%1 (task) has no actions registered.")},
        {QStringLiteral("待機ステップ・グループ・タスク自体は、他のステップと一緒にグループ化"
                        "できません（コンテナの入れ子は未対応です）。"),
         QStringLiteral("Wait steps, groups, and tasks themselves cannot be combined with other "
                        "steps into a group (nesting containers is not supported).")},
        {QStringLiteral("待機ステップ・グループ・タスク自体は、他のステップと一緒にタスク化"
                        "できません（コンテナの入れ子は未対応です）。"),
         QStringLiteral("Wait steps, groups, and tasks themselves cannot be combined with other "
                        "steps into a task (nesting containers is not supported).")},

        // v0.62: タスクメンバーの「新しく出現したダイアログを対象にする」機能
        {QStringLiteral("新しく出現したウィンドウ（ダイアログ等）を対象にする（自動検出）"),
         QStringLiteral("Target a newly appeared window (dialog, etc.) (auto-detected)")},
        {QStringLiteral("※このタスク内で直前までに実行した操作が開いたダイアログ等、対象アプリの"
                        "メインウィンドウ以外に新しく出現したウィンドウ全体を操作領域にします。"
                        "実行時にそのようなウィンドウが見つからない場合は、見つかるまで待機します。"),
         QStringLiteral("Uses the entire area of whatever window newly appeared besides the target "
                        "app's main window as the operation region -- e.g. a dialog opened by an "
                        "earlier action in this task. If no such window is found yet when this runs, "
                        "it waits until one appears.")},
        {QStringLiteral("新しく出現したダイアログ（自動検出）"), QStringLiteral("Newly appeared dialog (auto-detected)")},
        {QStringLiteral("新しく出現したダイアログ"), QStringLiteral("Newly appeared dialog")},
        {QStringLiteral("%1が対象とする新しいウィンドウ（ダイアログ等）が現れないため、"
                        "安全のためテストを停止しました"),
         QStringLiteral("%1's target window (a dialog, etc.) never appeared, so the test was stopped "
                        "for safety")},

        // v0.64: 起動時セットアップに「全消去」ボタン
        {QStringLiteral("全消去"), QStringLiteral("Clear All")},

        // v0.65: 起動時セットアップに「ホイールスクロール」「メニュー項目を選択」
        {QStringLiteral("ホイールスクロール"), QStringLiteral("Wheel Scroll")},
        {QStringLiteral("メニュー項目を選択"), QStringLiteral("Select Menu Item")},
        {QStringLiteral("方向:"), QStringLiteral("Direction:")},
        {QStringLiteral("量（ホイールの「目盛り」数）:"), QStringLiteral("Amount (wheel \"notches\"):")},
        {QStringLiteral("対象ウィンドウを基準にした右クリックの位置を指定してください。開いた"
                        "メニューから、常に同じ1項目を選択します。"),
         QStringLiteral("Specify the right-click position, relative to the target window. Always "
                        "selects the same one item from the menu it opens.")},
        {QStringLiteral("項目名で指定:"), QStringLiteral("By item name:")},
        {QStringLiteral("上から何番目かで指定:"), QStringLiteral("By position from top:")},
        {QStringLiteral("選択する項目名を入力してください。"), QStringLiteral("Please enter the item name to select.")},
        {QStringLiteral("スクロール (%1, %2) dx=%3 dy=%4"), QStringLiteral("Scroll (%1, %2) dx=%3 dy=%4")},
        {QStringLiteral("メニュー選択 (%1, %2) → 上から%3番目"),
         QStringLiteral("Menu select (%1, %2) → position %3 from top")},
        {QStringLiteral("メニュー選択 (%1, %2) → 「%3」"), QStringLiteral("Menu select (%1, %2) → \"%3\"")},
        {QStringLiteral("スクロール at (%1, %2) dx=%3 dy=%4"), QStringLiteral("Scroll at (%1, %2) dx=%3 dy=%4")},
        {QStringLiteral(" → メニュー項目「%1」を選択"), QStringLiteral(" → selected menu item \"%1\"")},
        {QStringLiteral(" → メニュー項目(上から%1番目)を選択"),
         QStringLiteral(" → selected menu item (position %1 from top)")},
        {QStringLiteral(" → 指定した項目が見つからなかったため、メニューを閉じました"),
         QStringLiteral(" → the specified item wasn't found, so the menu was closed")},

        // v0.67: 起動時セットアップを単体で試す機能
        {QStringLiteral("▶ 起動時セットアップを試す"), QStringLiteral("▶ Test Startup Setup")},
        {QStringLiteral("①で選択中の対象ウィンドウに対して、この一覧のセットアップだけを実行して"
                        "確認します。②のステップ構成は実行しません（未設定でも構いません）。"),
         QStringLiteral("Runs just this list's setup against the target window selected in ①, to check "
                        "it works. Doesn't run ②'s step configuration at all (which doesn't need to be "
                        "set up yet).")},
        {QStringLiteral("起動時セットアップが設定されていません。"
                        "「追加...」または「記録...」でセットアップを作成してください。"),
         QStringLiteral("No startup setup is configured. Use \"Add...\" or \"Record...\" to create one "
                        "first.")},
        {QStringLiteral("他のアプリケーションを操作するための権限が許可されていません。"
                        "設定を許可してから、もう一度お試しください。"),
         QStringLiteral("Permission to operate other applications hasn't been granted. Please grant it, "
                        "then try again.")},
        {QStringLiteral("起動時セットアップを実行中..."), QStringLiteral("Running startup setup...")},
        {QStringLiteral("起動時セットアップの実行確認を開始します（%1件）。完了次第、自動的に"
                        "停止します"),
         QStringLiteral("Starting startup setup verification (%1 actions). Will stop automatically once "
                        "it's done")},
        {QStringLiteral("起動時セットアップの実行確認が完了しました（%1件）"),
         QStringLiteral("Startup setup verification completed (%1 actions)")},

        // v0.68: 連続実行に対象ツール起動待ち時間パラメータを追加
        {QStringLiteral("対象ツールの起動検知後、さらに待つ時間:"),
         QStringLiteral("Extra wait after target launch is detected:")},
        {QStringLiteral(" 秒"), QStringLiteral(" sec")},
        {QStringLiteral("対象ツールのウィンドウを検知してから実際にテストを開始するまで、ここで"
                        "指定した秒数だけ追加で待ちます。起動直後はまだ操作を受け付けられない"
                        "ツールに対して、0（デフォルト。待たずに即座に開始）だと安全確認に失敗する"
                        "場合に使います。連続実行・①バッチの自動再起動待ち・上の「起動してから"
                        "開始する」のいずれにも適用されます。"),
         QStringLiteral("After the target tool's window is detected, waits this many extra seconds "
                        "before actually starting the test. Useful when a tool isn't ready to receive "
                        "input right at launch and the safety check fails with 0 (the default -- start "
                        "immediately, no extra wait). Applies to 連続実行, ①batch mode's automatic "
                        "relaunch wait, and the \"launch then start\" option above alike.")},
        {QStringLiteral("対象ツールの起動を検知しました。安定するまでさらに%1秒待ちます..."),
         QStringLiteral("Target tool launch detected. Waiting %1 more seconds for it to settle...")},
        {QStringLiteral("連続実行を中断しました（起動待ち時間の経過待ち中でした）"),
         QStringLiteral("Continuous run interrupted (was waiting out the extra launch delay)")},

        // v0.70: タスク内ダイアログ操作を「ダイアログのボタンを押す」に拡張
        {QStringLiteral("ダイアログのボタンを押す"), QStringLiteral("Press dialog button")},
        {QStringLiteral("このステップ（タスク内のメンバー）が「新しく出現したダイアログ（自動検出）」"
                        "を操作対象にしている場合のみ有効です。③操作パラメータの「ダイアログの"
                        "ボタン名」で指定した名前のボタンを探して押します。"),
         QStringLiteral("Only takes effect when this step (a task member) targets \"Newly appeared "
                        "dialog (auto-detected)\". Looks for and presses a button named one of "
                        "\"Dialog button names\" in ③ Action Parameters.")},
        {QStringLiteral("ダイアログのボタン名（この中で実際に見つかったものからランダムに1つ選んで"
                        "押す。1つも見つからなければこの操作をスキップ):"),
         QStringLiteral("Dialog button names (randomly picks one of these that's actually found and "
                        "presses it; skips this action if none are found):")},
        {QStringLiteral("ダイアログのボタン名が設定されていません"),
         QStringLiteral("No dialog button names are configured")},
        {QStringLiteral("指定したボタンが見つからなかったため、この操作をスキップしました"),
         QStringLiteral("None of the specified buttons were found, so this action was skipped")},
        {QStringLiteral("ダイアログのボタン「%1」を押す"), QStringLiteral("Press dialog button \"%1\"")},
        {QStringLiteral("対象アプリがアクティブでないため、安全のためテストを停止しました"),
         QStringLiteral("The target app isn't active, so the test was stopped for safety")},

        // v0.71: 対象アプリの応答が遅い時に操作間隔を自動的に延ばすオプション
        {QStringLiteral("対象アプリの応答が遅い時は操作間隔を自動的に延ばす"),
         QStringLiteral("Automatically slow down when the target app is slow to respond")},
        {QStringLiteral("応答確認（WM_PING）が一度失敗すると、ハングと判定されるまでの間、"
                        "操作間隔を一時的に延ばして対象アプリの負荷を減らします。"
                        "応答が戻れば自動的に元の間隔に戻ります。"),
         QStringLiteral("If a responsiveness check (WM_PING) fails once, this temporarily lengthens "
                        "the action interval (until it either recovers or is confirmed as a hang) "
                        "to reduce load on the target app. Reverts automatically once it responds "
                        "again.")},
        {QStringLiteral("対象アプリの応答が遅いため、操作間隔を自動的に延ばします"),
         QStringLiteral("The target app is slow to respond, so the action interval is being "
                        "automatically lengthened")},
        {QStringLiteral("対象アプリの応答が回復したため、操作間隔を元に戻します"),
         QStringLiteral("The target app has recovered, so the action interval is being reverted")},

        // v0.72: 対象ツールのウィンドウサイズ保存・復元機能
        {QStringLiteral("現在のウィンドウサイズを保存"), QStringLiteral("Save current window size")},
        {QStringLiteral("保存したサイズに変更"), QStringLiteral("Restore saved size")},
        {QStringLiteral("保存したウィンドウサイズ: 幅%1 高さ%2"),
         QStringLiteral("Saved window size: %1 x %2")},
        {QStringLiteral("保存したウィンドウサイズ: (未保存)"), QStringLiteral("Saved window size: (none)")},
        {QStringLiteral("対象ウィンドウを取得できません"), QStringLiteral("Can't get the target window")},
        {QStringLiteral("ウィンドウサイズを保存しました: 幅%1 高さ%2"),
         QStringLiteral("Saved window size: %1 x %2")},
        {QStringLiteral("保存されたサイズがありません"), QStringLiteral("No saved size")},
        {QStringLiteral("先に「現在のウィンドウサイズを保存」でサイズを保存してください。"),
         QStringLiteral("Use \"Save current window size\" first to save a size.")},
        {QStringLiteral("ウィンドウサイズの変更に失敗しました。"), QStringLiteral("Failed to change the window size.")},
        {QStringLiteral("保存したウィンドウサイズに変更しました: 幅%1 高さ%2"),
         QStringLiteral("Changed to the saved window size: %1 x %2")},

        // v0.73: 対象ツールのウィンドウ位置保存・復元機能
        {QStringLiteral("現在のウィンドウ位置を保存"), QStringLiteral("Save current window position")},
        {QStringLiteral("保存した位置に変更"), QStringLiteral("Restore saved position")},
        {QStringLiteral("保存したウィンドウ位置: x%1 y%2"), QStringLiteral("Saved window position: (%1, %2)")},
        {QStringLiteral("保存したウィンドウ位置: (未保存)"), QStringLiteral("Saved window position: (none)")},
        {QStringLiteral("ウィンドウ位置を保存しました: x%1 y%2"),
         QStringLiteral("Saved window position: (%1, %2)")},
        {QStringLiteral("保存された位置がありません"), QStringLiteral("No saved position")},
        {QStringLiteral("先に「現在のウィンドウ位置を保存」で位置を保存してください。"),
         QStringLiteral("Use \"Save current window position\" first to save a position.")},
        {QStringLiteral("ウィンドウ位置の変更に失敗しました。"), QStringLiteral("Failed to change the window position.")},
        {QStringLiteral("保存したウィンドウ位置に変更しました: x%1 y%2"),
         QStringLiteral("Changed to the saved window position: (%1, %2)")},

        // v0.74: 連続実行時に保存済みのウィンドウ位置・サイズへ変更するオプション
        {QStringLiteral("連続実行時に保存済みのウィンドウ位置・サイズへ変更する"),
         QStringLiteral("Apply the saved window position/size on each continuous-run relaunch")},
        {QStringLiteral("対象アプリが（再）起動して検知されるたびに、上で保存済みのウィンドウ"
                        "サイズ・位置があればそれぞれ適用してから実行を始めます"
                        "（連続実行のキル→再起動サイクル、①バッチ実行の自動/手動再起動待ち、"
                        "▶開始の「起動してから開始する」オプションのいずれにも適用されます）。"),
         QStringLiteral("Every time the target app is (re)launched and detected, this applies "
                        "whichever of the saved window size/position above are set before the run "
                        "starts (applies alike to 連続実行's kill-then-relaunch cycle, ①'s batch "
                        "auto/manual relaunch wait, and ▶ Start's \"launch before starting\" "
                        "option).")},

        // v0.75: macOSの画面収録権限の表示・案内（操作領域選択オーバーレイが
        // 他アプリを表示できなくなる不具合の原因調査・対応）
        {QStringLiteral("✓ 画面収録の権限は許可されています"),
         QStringLiteral("✓ Screen Recording permission is granted")},
        {QStringLiteral("✗ 画面収録の権限がありません（操作領域を選択する画面が黒くなり、"
                        "他のアプリが見えなくなります。下のボタンから設定を開いてください）"),
         QStringLiteral("✗ Screen Recording permission is missing (the operation-region "
                        "selection screen will appear black, making other apps look invisible. "
                        "Open Settings using the button below)")},
        {QStringLiteral("画面収録の権限設定を開く"), QStringLiteral("Open Screen Recording Settings")},
    };
    return table;
}

Language g_currentLanguage = Language::Japanese;
bool g_loaded = false;

void ensureLoaded()
{
    if (g_loaded)
        return;
    g_loaded = true;
    QSettings settings(QStringLiteral("asobi"), QStringLiteral("EnduranceTestGUI"));
    const QString saved = settings.value(QStringLiteral("language"), QStringLiteral("ja")).toString();
    g_currentLanguage = (saved == QStringLiteral("en")) ? Language::English : Language::Japanese;
}

}  // namespace

Language currentLanguage()
{
    ensureLoaded();
    return g_currentLanguage;
}

void setLanguage(Language lang)
{
    g_currentLanguage = lang;
    g_loaded = true;
    QSettings settings(QStringLiteral("asobi"), QStringLiteral("EnduranceTestGUI"));
    settings.setValue(QStringLiteral("language"), lang == Language::English ? QStringLiteral("en") : QStringLiteral("ja"));
}

QString t(const QString &japaneseText)
{
    if (currentLanguage() == Language::Japanese)
        return japaneseText;
    const QHash<QString, QString> &table = translationTable();
    const auto it = table.constFind(japaneseText);
    if (it != table.constEnd())
        return it.value();
    // Missing table entry -- fall back to the Japanese text rather than
    // showing nothing, so a translation gap degrades to "wrong language"
    // instead of a blank label.
    return japaneseText;
}

}  // namespace I18n
