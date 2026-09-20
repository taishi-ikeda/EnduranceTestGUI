#include "NamedRegionEditorDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "RegionHighlightOverlay.h"
#include "RegionSelectorOverlay.h"
#include "I18n.h"

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

NamedRegionEditorDialog::NamedRegionEditorDialog(const NamedRegion &initial, const QPoint &targetTopLeft,
                                                   bool hasTarget, QWidget *parent)
    : QDialog(parent),
      m_regions(initial.regions),
      m_excludeRegions(initial.excludeRegions),
      m_targetTopLeft(targetTopLeft),
      m_hasTarget(hasTarget),
      m_existingAnchorTopLeft(initial.anchorTopLeft),
      m_hadExistingAnchor(initial.followsTargetWindow)
{
    setWindowTitle(I18n::t(QStringLiteral("操作領域の設定")));

    auto *layout = new QVBoxLayout(this);

    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(I18n::t(QStringLiteral("名前:")), this));
    m_nameEdit = new QLineEdit(initial.name, this);
    m_nameEdit->setPlaceholderText(I18n::t(QStringLiteral("例: メインメニュー")));
    nameRow->addWidget(m_nameEdit, 1);
    layout->addLayout(nameRow);

    auto *regionLabel = new QLabel(I18n::t(QStringLiteral("矩形を画面上で描画してください（複数可）:")), this);
    regionLabel->setWordWrap(true);
    layout->addWidget(regionLabel);
    m_regionListWidget = new QListWidget(this);
    m_regionListWidget->setMaximumHeight(100);
    layout->addWidget(m_regionListWidget);
    auto *regionButtonsRow = new QHBoxLayout;
    m_drawButton = new QPushButton(I18n::t(QStringLiteral("矩形を描画...")), this);
    m_removeRegionButton = new QPushButton(I18n::t(QStringLiteral("選択を削除")), this);
    regionButtonsRow->addWidget(m_drawButton);
    regionButtonsRow->addWidget(m_removeRegionButton);
    layout->addLayout(regionButtonsRow);
    connect(m_drawButton, &QPushButton::clicked, this, &NamedRegionEditorDialog::onDrawRegions);
    connect(m_removeRegionButton, &QPushButton::clicked, this,
            &NamedRegionEditorDialog::onRemoveSelectedRegion);

    auto *excludeLabel = new QLabel(
        I18n::t(QStringLiteral("この操作領域内でクリックしたくない除外(マスク)矩形があれば指定してください（任意、複数可）:")),
        this);
    excludeLabel->setWordWrap(true);
    layout->addWidget(excludeLabel);
    m_excludeListWidget = new QListWidget(this);
    m_excludeListWidget->setMaximumHeight(100);
    layout->addWidget(m_excludeListWidget);
    auto *excludeButtonsRow = new QHBoxLayout;
    m_drawExcludeButton = new QPushButton(I18n::t(QStringLiteral("除外矩形を描画...")), this);
    m_removeExcludeButton = new QPushButton(I18n::t(QStringLiteral("選択を削除")), this);
    excludeButtonsRow->addWidget(m_drawExcludeButton);
    excludeButtonsRow->addWidget(m_removeExcludeButton);
    layout->addLayout(excludeButtonsRow);
    connect(m_drawExcludeButton, &QPushButton::clicked, this,
            &NamedRegionEditorDialog::onDrawExcludeRegions);
    connect(m_removeExcludeButton, &QPushButton::clicked, this,
            &NamedRegionEditorDialog::onRemoveSelectedExcludeRegion);

    // SPEC.md 10: named regions are fixed screen coordinates by default, so
    // they don't follow the target window if it moves. Opting in here
    // records the target's current top-left as this region's anchor;
    // RandomActionEngine translates the rectangles by however far the
    // target has moved from that anchor each time the region is used.
    // QCheckBox has no setWordWrap(); break the long label manually instead
    // (same technique used elsewhere in this app -- SPEC.md 6.9).
    m_followTargetCheck = new QCheckBox(
        I18n::t(QStringLiteral("対象ウィンドウの移動に追従させる\n（保存時の対象ウィンドウ位置を基準に記録）")), this);
    m_followTargetCheck->setChecked(m_hadExistingAnchor);
    m_followTargetCheck->setEnabled(m_hasTarget);
    m_followTargetCheck->setToolTip(
        m_hasTarget ? I18n::t(QStringLiteral("OKを押した時点の対象ウィンドウの位置を基準点として記録します。"))
                    : I18n::t(QStringLiteral("対象ウィンドウが選択されていないため、今は変更できません"
                                     "（既存の設定はそのまま保持されます）。")));
    layout->addWidget(m_followTargetCheck);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &NamedRegionEditorDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    // Visualize the region being built/edited on screen for as long as
    // this dialog stays open (SPEC.md 6.3) -- hidden (not destroyed) when
    // the dialog closes, whether accepted, cancelled, or closed any other
    // way.
    connect(this, &QDialog::finished, this, [this](int) {
        if (m_highlightOverlay)
            m_highlightOverlay->hide();
    });
    // Keep the on-screen highlight's name label in sync while typing, not
    // just after a rectangle add/remove.
    connect(m_nameEdit, &QLineEdit::textChanged, this, &NamedRegionEditorDialog::updateHighlight);

    refreshRegionList();
    refreshExcludeList();
    updateHighlight();
    resize(460, 560);
}

void NamedRegionEditorDialog::updateHighlight()
{
    if (!m_highlightOverlay)
        m_highlightOverlay = new RegionHighlightOverlay(this);
    m_highlightOverlay->showRegion(m_nameEdit->text(), m_regions, m_excludeRegions);
}

void NamedRegionEditorDialog::onDrawRegions()
{
    // Hide the persistent highlight while RegionSelectorOverlay (which
    // already shows the current regions itself while drawing) is up, to
    // avoid the two always-on-top overlays visually fighting each other.
    if (m_highlightOverlay)
        m_highlightOverlay->hide();
    // See StepEditorDialog's equivalent comment: deliberately not
    // hide()/show()-ing this dialog around the overlay.
    const QList<QRect> added =
        RegionSelectorOverlay::run(RegionSelectorOverlay::Mode::Include, m_regions, m_excludeRegions);
    if (!added.isEmpty()) {
        m_regions.append(added);
        refreshRegionList();
    }
    updateHighlight();
}

void NamedRegionEditorDialog::onRemoveSelectedRegion()
{
    const int row = m_regionListWidget->currentRow();
    if (row >= 0 && row < m_regions.size()) {
        m_regions.removeAt(row);
        refreshRegionList();
        updateHighlight();
    }
}

void NamedRegionEditorDialog::onDrawExcludeRegions()
{
    if (m_highlightOverlay)
        m_highlightOverlay->hide();
    const QList<QRect> added =
        RegionSelectorOverlay::run(RegionSelectorOverlay::Mode::Exclude, m_regions, m_excludeRegions);
    if (!added.isEmpty()) {
        m_excludeRegions.append(added);
        refreshExcludeList();
    }
    updateHighlight();
}

void NamedRegionEditorDialog::onRemoveSelectedExcludeRegion()
{
    const int row = m_excludeListWidget->currentRow();
    if (row >= 0 && row < m_excludeRegions.size()) {
        m_excludeRegions.removeAt(row);
        refreshExcludeList();
        updateHighlight();
    }
}

void NamedRegionEditorDialog::refreshRegionList()
{
    m_regionListWidget->clear();
    // "矩形N" (rectangle N), not "領域N" -- this dialog's own "名前" field is
    // the operation region's name (auto-suggested as "操作領域N" by
    // MainWindow::generateDefaultRegionName), so reusing "領域N" for the
    // individual rectangles drawn inside it read as if it were the same
    // name and was confusing (SPEC.md 6.3).
    for (int i = 0; i < m_regions.size(); ++i)
        m_regionListWidget->addItem(labeledRect(I18n::t(QStringLiteral("矩形")), i, m_regions[i]));
}

void NamedRegionEditorDialog::refreshExcludeList()
{
    m_excludeListWidget->clear();
    for (int i = 0; i < m_excludeRegions.size(); ++i)
        m_excludeListWidget->addItem(labeledRect(I18n::t(QStringLiteral("除外")), i, m_excludeRegions[i]));
}

void NamedRegionEditorDialog::onAccept()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")), I18n::t(QStringLiteral("名前を入力してください。")));
        return;
    }
    if (m_regions.isEmpty()) {
        QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                              I18n::t(QStringLiteral("領域を最低1つ描画してください。")));
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
    region.followsTargetWindow = m_followTargetCheck->isChecked();
    if (region.followsTargetWindow) {
        // Rebase to the live target position when one is available (the
        // checkbox is only interactively toggleable in that case anyway);
        // otherwise this is an already-following region being re-saved
        // with no target currently selected, so keep its existing anchor
        // rather than losing track of it.
        region.anchorTopLeft = m_hasTarget ? m_targetTopLeft : m_existingAnchorTopLeft;
    }
    return region;
}
