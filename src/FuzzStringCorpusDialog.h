#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;
class QPushButton;

// SPEC.md 追加実装依頼「異常入力ファジング」: editor for the app-wide fuzz-
// string corpus (TestConfig::fuzzStringCorpus / MainWindow's own
// QSettings-backed master list), opened via "表示" menu's "ファズ文字列
// リストを編集..." action. A plain add/edit/remove/reset-to-defaults list
// editor -- deliberately not tied to any particular step, since the corpus
// itself is a single shared resource every step's ActionParams::
// fuzzStringUseAllEntries/fuzzStringSelectedEntries draws from (see
// ActionParamsEditor's own fuzz-string section).
class FuzzStringCorpusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FuzzStringCorpusDialog(const QStringList &initial, QWidget *parent = nullptr);

    // The edited corpus -- only meaningful if the dialog was accepted
    // (OK), same convention as the rest of this app's editor dialogs.
    QStringList result() const;

private slots:
    void onAdd();
    void onEdit();
    void onRemove();
    void onResetToDefaults();

private:
    void refreshList();

    QListWidget *m_listWidget = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QStringList m_entries;
};
