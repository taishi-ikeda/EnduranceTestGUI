#include "NamedRegionEditorDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "RegionSelectorOverlay.h"

namespace
{
QString labeledRect(const QString &prefix, int index, const QRect &r)
{
    return QStringLiteral("%1%2: (%3, %4)  %5 x %6")
        .arg(prefix)
        .arg(index + 1)
        .arg(r.x())
        .arg(r.y())
        .arg(r.width())
        .arg(r.height());
}
}  // namespace

NamedRegionEditorDialog::NamedRegionEditorDialog(const NamedRegion &initial, QWidget *parent)
    : QDialog(parent), m_regions(initial.regions), m_excludeRegions(initial.excludeRegions)
{
    setWindowTitle(QStringLiteral("操作領域の設定"));

    auto *layout = new QVBoxLayout(this);

    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(QStringLiteral("名前:"), this));
    m_nameEdit = new QLineEdit(initial.name, this);
    m_nameEdit->setPlaceholderText(QStringLiteral("例: メインメニュー"));
    nameRow->addWidget(m_nameEdit, 1);
    layout->addLayout(nameRow);

    auto *regionLabel = new QLabel(QStringLiteral("領域を画面上で描画してください（複数可）:"), this);
    regionLabel->setWordWrap(true);
    layout->addWidget(regionLabel);
    m_regionListWidget = new QListWidget(this);
    m_regionListWidget->setMaximumHeight(100);
    layout->addWidget(m_regionListWidget);
    auto *regionButtonsRow = new QHBoxLayout;
    m_drawButton = new QPushButton(QStringLiteral("領域を描画..."), this);
    m_removeRegionButton = new QPushButton(QStringLiteral("選択を削除"), this);
    regionButtonsRow->addWidget(m_drawButton);
    regionButtonsRow->addWidget(m_removeRegionButton);
    layout->addLayout(regionButtonsRow);
    connect(m_drawButton, &QPushButton::clicked, this, &NamedRegionEditorDialog::onDrawRegions);
    connect(m_removeRegionButton, &QPushButton::clicked, this,
            &NamedRegionEditorDialog::onRemoveSelectedRegion);

    auto *excludeLabel = new QLabel(
        QStringLiteral("この領域内でクリックしたくない除外(マスク)領域があれば指定してください（任意、複数可）:"),
        this);
    excludeLabel->setWordWrap(true);
    layout->addWidget(excludeLabel);
    m_excludeListWidget = new QListWidget(this);
    m_excludeListWidget->setMaximumHeight(100);
    layout->addWidget(m_excludeListWidget);
    auto *excludeButtonsRow = new QHBoxLayout;
    m_drawExcludeButton = new QPushButton(QStringLiteral("除外領域を描画..."), this);
    m_removeExcludeButton = new QPushButton(QStringLiteral("選択を削除"), this);
    excludeButtonsRow->addWidget(m_drawExcludeButton);
    excludeButtonsRow->addWidget(m_removeExcludeButton);
    layout->addLayout(excludeButtonsRow);
    connect(m_drawExcludeButton, &QPushButton::clicked, this,
            &NamedRegionEditorDialog::onDrawExcludeRegions);
    connect(m_removeExcludeButton, &QPushButton::clicked, this,
            &NamedRegionEditorDialog::onRemoveSelectedExcludeRegion);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &NamedRegionEditorDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    refreshRegionList();
    refreshExcludeList();
    resize(460, 560);
}

void NamedRegionEditorDialog::onDrawRegions()
{
    // See StepEditorDialog's equivalent comment: deliberately not
    // hide()/show()-ing this dialog around the overlay.
    const QList<QRect> added =
        RegionSelectorOverlay::run(RegionSelectorOverlay::Mode::Include, m_regions, m_excludeRegions);
    if (!added.isEmpty()) {
        m_regions.append(added);
        refreshRegionList();
    }
}

void NamedRegionEditorDialog::onRemoveSelectedRegion()
{
    const int row = m_regionListWidget->currentRow();
    if (row >= 0 && row < m_regions.size()) {
        m_regions.removeAt(row);
        refreshRegionList();
    }
}

void NamedRegionEditorDialog::onDrawExcludeRegions()
{
    const QList<QRect> added =
        RegionSelectorOverlay::run(RegionSelectorOverlay::Mode::Exclude, m_regions, m_excludeRegions);
    if (!added.isEmpty()) {
        m_excludeRegions.append(added);
        refreshExcludeList();
    }
}

void NamedRegionEditorDialog::onRemoveSelectedExcludeRegion()
{
    const int row = m_excludeListWidget->currentRow();
    if (row >= 0 && row < m_excludeRegions.size()) {
        m_excludeRegions.removeAt(row);
        refreshExcludeList();
    }
}

void NamedRegionEditorDialog::refreshRegionList()
{
    m_regionListWidget->clear();
    for (int i = 0; i < m_regions.size(); ++i)
        m_regionListWidget->addItem(labeledRect(QStringLiteral("領域"), i, m_regions[i]));
}

void NamedRegionEditorDialog::refreshExcludeList()
{
    m_excludeListWidget->clear();
    for (int i = 0; i < m_excludeRegions.size(); ++i)
        m_excludeListWidget->addItem(labeledRect(QStringLiteral("除外"), i, m_excludeRegions[i]));
}

void NamedRegionEditorDialog::onAccept()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("入力エラー"), QStringLiteral("名前を入力してください。"));
        return;
    }
    if (m_regions.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("入力エラー"),
                              QStringLiteral("領域を最低1つ描画してください。"));
        return;
    }
    accept();
}

NamedRegion NamedRegionEditorDialog::result() const
{
    NamedRegion region;
    region.name = m_nameEdit->text().trimmed();
    region.regions = m_regions;
    region.excludeRegions = m_excludeRegions;
    return region;
}
