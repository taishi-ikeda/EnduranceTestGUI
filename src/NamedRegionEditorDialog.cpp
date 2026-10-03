#include "NamedRegionEditorDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ObjectPickerOverlay.h"
#include "RegionHighlightOverlay.h"
#include "RegionSelectorOverlay.h"
#include "I18n.h"
#include "platform/PlatformAutomation.h"

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
                                                   bool hasTarget, qint64 targetPid, QWidget *parent)
    : QDialog(parent),
      m_regions(initial.regions),
      m_excludeRegions(initial.excludeRegions),
      m_targetTopLeft(targetTopLeft),
      m_hasTarget(hasTarget),
      m_targetPid(targetPid),
      m_existingAnchorTopLeft(initial.anchorTopLeft),
      m_hadExistingAnchor(initial.followsTargetWindow),
      m_objectTarget(initial.objectTarget),
      m_objectPicked(initial.isObjectTarget && !initial.objectTarget.name.isEmpty())
{
    setWindowTitle(I18n::t(QStringLiteral("操作領域の設定")));
    // RegionHighlightOverlay's per-screen windows (shown continuously while
    // this dialog is open, see updateHighlight() below) use
    // Qt::WindowStaysOnTopHint so the highlight stays visible above the
    // target app being tested. Without this dialog being in that same
    // "always on top" layer, several window managers (e.g. a bare openbox
    // session) keep re-asserting the highlight windows above this dialog
    // regardless of raise()/activateWindow() calls, making the dialog
    // itself impossible to see or interact with (SPEC.md 6.3/8).
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    auto *layout = new QVBoxLayout(this);

    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(I18n::t(QStringLiteral("名前:")), this));
    m_nameEdit = new QLineEdit(initial.name, this);
    m_nameEdit->setPlaceholderText(I18n::t(QStringLiteral("例: メインメニュー")));
    nameRow->addWidget(m_nameEdit, 1);
    layout->addLayout(nameRow);

    // SPEC.md 追加実装依頼「名前付きオブジェクト」: how this region's
    // on-screen location is determined -- a fixed rectangle (existing) or
    // an accessibility-tree object resolved by role+name every time it's
    // used (new). Mutually exclusive; onModeChanged() shows/hides the two
    // groups built below accordingly.
    auto *modeRow = new QHBoxLayout;
    m_rectModeRadio = new QRadioButton(I18n::t(QStringLiteral("矩形を描画")), this);
    m_objectModeRadio = new QRadioButton(I18n::t(QStringLiteral("画面上の部品を指定")), this);
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_rectModeRadio);
    modeGroup->addButton(m_objectModeRadio);
    (initial.isObjectTarget ? m_objectModeRadio : m_rectModeRadio)->setChecked(true);
    modeRow->addWidget(m_rectModeRadio);
    modeRow->addWidget(m_objectModeRadio);
    modeRow->addStretch();
    layout->addLayout(modeRow);
    connect(m_rectModeRadio, &QRadioButton::toggled, this, &NamedRegionEditorDialog::onModeChanged);

    m_rectModeGroup = new QWidget(this);
    auto *rectLayout = new QVBoxLayout(m_rectModeGroup);
    rectLayout->setContentsMargins(0, 0, 0, 0);

    auto *regionLabel = new QLabel(I18n::t(QStringLiteral("矩形を画面上で描画してください（複数可）:")), m_rectModeGroup);
    regionLabel->setWordWrap(true);
    rectLayout->addWidget(regionLabel);
    m_regionListWidget = new QListWidget(m_rectModeGroup);
    m_regionListWidget->setMaximumHeight(100);
    rectLayout->addWidget(m_regionListWidget);
    auto *regionButtonsRow = new QHBoxLayout;
    m_drawButton = new QPushButton(I18n::t(QStringLiteral("矩形を描画...")), m_rectModeGroup);
    m_removeRegionButton = new QPushButton(I18n::t(QStringLiteral("選択を削除")), m_rectModeGroup);
    regionButtonsRow->addWidget(m_drawButton);
    regionButtonsRow->addWidget(m_removeRegionButton);
    rectLayout->addLayout(regionButtonsRow);
    connect(m_drawButton, &QPushButton::clicked, this, &NamedRegionEditorDialog::onDrawRegions);
    connect(m_removeRegionButton, &QPushButton::clicked, this,
            &NamedRegionEditorDialog::onRemoveSelectedRegion);

    auto *excludeLabel = new QLabel(
        I18n::t(QStringLiteral("この操作領域内でクリックしたくない除外(マスク)矩形があれば指定してください（任意、複数可）:")),
        m_rectModeGroup);
    excludeLabel->setWordWrap(true);
    rectLayout->addWidget(excludeLabel);
    m_excludeListWidget = new QListWidget(m_rectModeGroup);
    m_excludeListWidget->setMaximumHeight(100);
    rectLayout->addWidget(m_excludeListWidget);
    auto *excludeButtonsRow = new QHBoxLayout;
    m_drawExcludeButton = new QPushButton(I18n::t(QStringLiteral("除外矩形を描画...")), m_rectModeGroup);
    m_removeExcludeButton = new QPushButton(I18n::t(QStringLiteral("選択を削除")), m_rectModeGroup);
    excludeButtonsRow->addWidget(m_drawExcludeButton);
    excludeButtonsRow->addWidget(m_removeExcludeButton);
    rectLayout->addLayout(excludeButtonsRow);
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
        I18n::t(QStringLiteral("対象ウィンドウの移動に追従させる\n（保存時の対象ウィンドウ位置を基準に記録）")),
        m_rectModeGroup);
    m_followTargetCheck->setChecked(m_hadExistingAnchor);
    m_followTargetCheck->setEnabled(m_hasTarget);
    m_followTargetCheck->setToolTip(
        m_hasTarget ? I18n::t(QStringLiteral("OKを押した時点の対象ウィンドウの位置を基準点として記録します。"))
                    : I18n::t(QStringLiteral("対象ウィンドウが選択されていないため、今は変更できません"
                                     "（既存の設定はそのまま保持されます）。")));
    rectLayout->addWidget(m_followTargetCheck);
    layout->addWidget(m_rectModeGroup);

    // SPEC.md 追加実装依頼「名前付きオブジェクト」: the object-mode
    // counterpart to the rectangle group above -- see onPickObject()/
    // result() for how this feeds into NamedRegion::objectTarget.
    m_objectModeGroup = new QWidget(this);
    auto *objectLayout = new QVBoxLayout(m_objectModeGroup);
    objectLayout->setContentsMargins(0, 0, 0, 0);

    auto *objectHint = new QLabel(
        I18n::t(QStringLiteral("「オブジェクトを指定...」を押すと画面が切り替わるので、対象アプリ上で部品（ボタン・"
                                "メニュー項目・チェックボックスなど）にカーソルを合わせ、緑の枠でハイライトされた"
                                "状態でクリックしてください。")),
        m_objectModeGroup);
    objectHint->setWordWrap(true);
    objectLayout->addWidget(objectHint);

    m_pickObjectButton = new QPushButton(I18n::t(QStringLiteral("オブジェクトを指定...")), m_objectModeGroup);
    m_pickObjectButton->setEnabled(m_hasTarget);
    objectLayout->addWidget(m_pickObjectButton);
    connect(m_pickObjectButton, &QPushButton::clicked, this, &NamedRegionEditorDialog::onPickObject);

    m_objectInfoLabel = new QLabel(m_objectModeGroup);
    m_objectInfoLabel->setWordWrap(true);
    objectLayout->addWidget(m_objectInfoLabel);

    m_useDefaultActionCheck = new QCheckBox(
        I18n::t(QStringLiteral("クリック操作では、座標の代わりにこの部品の既定アクションを直接実行する\n"
                                "（ボタンなら押す、チェックボックスなら切り替える、など）")),
        m_objectModeGroup);
    m_useDefaultActionCheck->setChecked(initial.objectTarget.useDefaultAction);
    objectLayout->addWidget(m_useDefaultActionCheck);

    auto *intervalRow = new QHBoxLayout;
    intervalRow->addWidget(
        new QLabel(I18n::t(QStringLiteral("再解決の間隔（この部品の位置を再検索する頻度）:")), m_objectModeGroup));
    m_reresolveIntervalSpin = new QSpinBox(m_objectModeGroup);
    m_reresolveIntervalSpin->setRange(0, 100000);
    m_reresolveIntervalSpin->setSpecialValueText(I18n::t(QStringLiteral("ステップが変わるたびのみ")));
    m_reresolveIntervalSpin->setSuffix(I18n::t(QStringLiteral(" 回ごと")));
    m_reresolveIntervalSpin->setValue(initial.objectTarget.reresolveEveryActions);
    m_reresolveIntervalSpin->setToolTip(
        I18n::t(QStringLiteral("0の場合、このステップの実行が始まった時（または対象が見失われた時）にのみ"
                                "位置を再検索します。1以上にすると、実行中もこの回数ごとに強制的に再検索し、"
                                "レイアウトの動的な変化によく追従しますが、その分だけ低速になります。")));
    intervalRow->addWidget(m_reresolveIntervalSpin);
    intervalRow->addStretch();
    objectLayout->addLayout(intervalRow);

    layout->addWidget(m_objectModeGroup);

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
    if (m_objectPicked)
        updateObjectInfoLabel();
    else
        m_objectInfoLabel->setText(I18n::t(QStringLiteral("（まだ指定されていません）")));
    onModeChanged();  // sets initial group visibility from the radio state seeded above
    updateHighlight();
    resize(460, 620);
}

void NamedRegionEditorDialog::updateHighlight()
{
    if (!m_highlightOverlay)
        m_highlightOverlay = new RegionHighlightOverlay(this);
    if (m_objectModeRadio->isChecked()) {
        // Unlike the rectangle group, this isn't live-tracking (the object
        // could move between now and when the dialog closes) -- it is just
        // a one-time confirmation of where onPickObject() resolved it, so
        // the user can tell at a glance whether they clicked the intended
        // part of the screen. See SPEC.md 追加実装依頼「名前付きオブジェクト」.
        if (m_objectPicked)
            m_highlightOverlay->showRegion(m_nameEdit->text(), {m_lastPickedBounds}, {});
        else
            m_highlightOverlay->hide();
    } else {
        m_highlightOverlay->showRegion(m_nameEdit->text(), m_regions, m_excludeRegions);
    }
    // RegionHighlightOverlay's windows are always-on-top (so the highlight
    // shows above the target app being tested) -- without reasserting this
    // dialog above them every time they're (re)shown, some window managers
    // leave the dialog itself visually hidden behind them instead of on
    // top, effectively making it unusable (SPEC.md 6.3/8).
    raise();
    activateWindow();
}

void NamedRegionEditorDialog::onModeChanged()
{
    const bool objectMode = m_objectModeRadio->isChecked();
    m_rectModeGroup->setVisible(!objectMode);
    m_objectModeGroup->setVisible(objectMode);
    updateHighlight();
}

void NamedRegionEditorDialog::onPickObject()
{
    // Hide the persistent highlight while ObjectPickerOverlay (which draws
    // its own live hover highlight) is up, same rationale as onDrawRegions()
    // hiding it around RegionSelectorOverlay.
    if (m_highlightOverlay)
        m_highlightOverlay->hide();

    PlatformAutomation::AccessibleObjectInfo info;
    const bool picked = ObjectPickerOverlay::run(m_targetPid, info);
    if (picked) {
        // Determine which occurrence (0-based, among objects sharing this
        // exact role+name) the user actually clicked, by re-resolving each
        // candidate in turn via the same lookup RandomActionEngine will use
        // at run time and comparing bounds -- so saving/loading this
        // NamedRegion later lands on the same object even if several share
        // a label (SPEC.md 追加実装依頼「名前付きオブジェクト」). Capped at
        // a generous but finite number of candidates to bound the work; if
        // none match exactly (e.g. the object moved between the hover frame
        // and the click), falls back to occurrence 0 rather than leaving
        // the previous pick in place.
        int occurrenceIndex = 0;
        for (int i = 0; i < 100; ++i) {
            PlatformAutomation::AccessibleObjectHandle handle =
                PlatformAutomation::findAccessibleObject(m_targetPid, info.role, info.name, i);
            if (!handle.isValid())
                break;
            QRect bounds;
            if (handle.currentBounds(bounds) && bounds == info.bounds) {
                occurrenceIndex = i;
                break;
            }
        }

        m_objectTarget.role = info.role;
        m_objectTarget.name = info.name;
        m_objectTarget.occurrenceIndex = occurrenceIndex;
        m_objectPicked = true;
        m_lastPickedBounds = info.bounds;
        updateObjectInfoLabel();
    }
    updateHighlight();
}

void NamedRegionEditorDialog::updateObjectInfoLabel()
{
    m_objectInfoLabel->setText(
        I18n::t(QStringLiteral("指定中: %1 「%2」（%3番目の一致）"))
            .arg(m_objectTarget.role.isEmpty() ? I18n::t(QStringLiteral("(役割不明)")) : m_objectTarget.role)
            .arg(m_objectTarget.name.isEmpty() ? I18n::t(QStringLiteral("(名前なし)")) : m_objectTarget.name)
            .arg(m_objectTarget.occurrenceIndex + 1));
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
        m_regionsChanged = true;
        refreshRegionList();
    }
    updateHighlight();
}

void NamedRegionEditorDialog::onRemoveSelectedRegion()
{
    const int row = m_regionListWidget->currentRow();
    if (row >= 0 && row < m_regions.size()) {
        m_regions.removeAt(row);
        m_regionsChanged = true;
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
        m_regionsChanged = true;
        refreshExcludeList();
    }
    updateHighlight();
}

void NamedRegionEditorDialog::onRemoveSelectedExcludeRegion()
{
    const int row = m_excludeListWidget->currentRow();
    if (row >= 0 && row < m_excludeRegions.size()) {
        m_excludeRegions.removeAt(row);
        m_regionsChanged = true;
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
    if (m_objectModeRadio->isChecked()) {
        if (!m_objectPicked) {
            QMessageBox::warning(this, I18n::t(QStringLiteral("入力エラー")),
                                  I18n::t(QStringLiteral("「オブジェクトを指定...」で部品を選択してください。")));
            return;
        }
    } else if (m_regions.isEmpty()) {
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
    region.isObjectTarget = m_objectModeRadio->isChecked();
    if (region.isObjectTarget) {
        // SPEC.md 追加実装依頼「名前付きオブジェクト」: regions/
        // excludeRegions/followsTargetWindow/anchorTopLeft are left at
        // their just-default-constructed values -- unused for an object-
        // target region (RandomActionEngine::resolveObjectTargetRegion()
        // resolves its bounds dynamically instead).
        region.objectTarget = m_objectTarget;
        region.objectTarget.useDefaultAction = m_useDefaultActionCheck->isChecked();
        region.objectTarget.reresolveEveryActions = m_reresolveIntervalSpin->value();
        return region;
    }

    region.regions = m_regions;
    region.excludeRegions = m_excludeRegions;
    region.followsTargetWindow = m_followTargetCheck->isChecked();
    if (region.followsTargetWindow) {
        // Bug (SPEC.md追加実装及び修正依頼): a follow-enabled region that
        // gets edited and re-saved WITHOUT touching its rectangles (e.g.
        // just renaming it, or editing an unrelated field) used to always
        // rebase the anchor to wherever the target window happens to be
        // *right now* -- even though the still-unchanged rectangles were
        // captured relative to the OLD anchor. That desyncs the two: at
        // run time resolveStepRegion() then translates the (still-old)
        // rectangles by (current position - new anchor), landing them at
        // the wrong place -- badly enough that RandomActionEngine's "about
        // to click outside the target app" safety check would trip and
        // abort the whole test. Only rebase the anchor when the rectangles
        // this anchor applies to were actually redrawn this session, or
        // when this wasn't already a following region (nothing to
        // preserve). Otherwise the existing anchor is still exactly the
        // one those unchanged rectangles were drawn against, so keep it.
        if (m_hadExistingAnchor && !m_regionsChanged)
            region.anchorTopLeft = m_existingAnchorTopLeft;
        else
            region.anchorTopLeft = m_hasTarget ? m_targetTopLeft : m_existingAnchorTopLeft;
    }
    return region;
}
