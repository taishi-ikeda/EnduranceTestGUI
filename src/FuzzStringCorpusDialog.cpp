#include "FuzzStringCorpusDialog.h"
#include "FuzzStringDefaults.h"
#include "I18n.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
// Long entries (deliberately, e.g. the built-in 64KB stress-test string)
// would otherwise make a single row unreadable/unreasonably wide -- show
// just the start plus a character count, same idea as eliding a long
// filename. The full text is still what's actually stored/used; this only
// affects how a row is displayed in this list.
constexpr int kMaxDisplayChars = 80;

QString displayTextFor(const QString &entry)
{
    if (entry.size() <= kMaxDisplayChars)
        return entry;
    return I18n::t(QStringLiteral("%1...(全%2文字)")).arg(entry.left(kMaxDisplayChars)).arg(entry.size());
}
}  // namespace

FuzzStringCorpusDialog::FuzzStringCorpusDialog(const QStringList &initial, QWidget *parent)
    : QDialog(parent), m_entries(initial)
{
    setWindowTitle(I18n::t(QStringLiteral("ファズ文字列リストを編集")));
    resize(560, 480);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        I18n::t(QStringLiteral("「ファズ文字列入力」操作が送信先として使う文字列の一覧です（アプリ全体で共有されます）。")),
        this));

    m_listWidget = new QListWidget(this);
    layout->addWidget(m_listWidget, 1);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &FuzzStringCorpusDialog::onEdit);

    auto *buttonRow = new QHBoxLayout;
    auto *addButton = new QPushButton(I18n::t(QStringLiteral("追加...")), this);
    m_editButton = new QPushButton(I18n::t(QStringLiteral("編集...")), this);
    m_removeButton = new QPushButton(I18n::t(QStringLiteral("削除")), this);
    auto *resetButton = new QPushButton(I18n::t(QStringLiteral("デフォルトに戻す...")), this);
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(m_editButton);
    buttonRow->addWidget(m_removeButton);
    buttonRow->addStretch();
    buttonRow->addWidget(resetButton);
    layout->addLayout(buttonRow);

    connect(addButton, &QPushButton::clicked, this, &FuzzStringCorpusDialog::onAdd);
    connect(m_editButton, &QPushButton::clicked, this, &FuzzStringCorpusDialog::onEdit);
    connect(m_removeButton, &QPushButton::clicked, this, &FuzzStringCorpusDialog::onRemove);
    connect(resetButton, &QPushButton::clicked, this, &FuzzStringCorpusDialog::onResetToDefaults);

    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, [this] {
        const bool hasSelection = !m_listWidget->selectedItems().isEmpty();
        m_editButton->setEnabled(hasSelection);
        m_removeButton->setEnabled(hasSelection);
    });

    auto *dialogButtons = new QHBoxLayout;
    dialogButtons->addStretch();
    auto *okButton = new QPushButton(I18n::t(QStringLiteral("OK")), this);
    auto *cancelButton = new QPushButton(I18n::t(QStringLiteral("キャンセル")), this);
    okButton->setDefault(true);
    dialogButtons->addWidget(okButton);
    dialogButtons->addWidget(cancelButton);
    layout->addLayout(dialogButtons);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    refreshList();
}

QStringList FuzzStringCorpusDialog::result() const
{
    return m_entries;
}

void FuzzStringCorpusDialog::refreshList()
{
    m_listWidget->clear();
    for (const QString &entry : m_entries)
        m_listWidget->addItem(displayTextFor(entry));
    const bool hasSelection = !m_listWidget->selectedItems().isEmpty();
    m_editButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
}

void FuzzStringCorpusDialog::onAdd()
{
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, I18n::t(QStringLiteral("ファズ文字列を追加")),
        I18n::t(QStringLiteral("対象に送信する文字列を入力してください（制御文字や改行もそのまま使えます）:")),
        QString(), &ok);
    if (!ok || text.isEmpty())
        return;
    m_entries.append(text);
    refreshList();
    m_listWidget->setCurrentRow(m_listWidget->count() - 1);
}

void FuzzStringCorpusDialog::onEdit()
{
    const int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_entries.size())
        return;
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, I18n::t(QStringLiteral("ファズ文字列を編集")),
        I18n::t(QStringLiteral("対象に送信する文字列を入力してください（制御文字や改行もそのまま使えます）:")),
        m_entries[row], &ok);
    if (!ok || text.isEmpty())
        return;
    m_entries[row] = text;
    refreshList();
    m_listWidget->setCurrentRow(row);
}

void FuzzStringCorpusDialog::onRemove()
{
    const int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_entries.size())
        return;
    m_entries.removeAt(row);
    refreshList();
}

void FuzzStringCorpusDialog::onResetToDefaults()
{
    const auto choice = QMessageBox::question(
        this, I18n::t(QStringLiteral("デフォルトに戻しますか？")),
        I18n::t(QStringLiteral("現在の一覧をすべて削除し、組み込みのデフォルト一覧に置き換えます。"
                                "この操作は元に戻せません。よろしいですか？")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes)
        return;
    m_entries = FuzzStringDefaults::builtInCorpus();
    refreshList();
}
