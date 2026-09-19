#include "StepEditorDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

StepEditorDialog::StepEditorDialog(const RegionStep &initial, const QList<NamedRegion> &availableRegions,
                                    QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("ステップの操作領域を選択"));

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(
        new QLabel(QStringLiteral("このステップで操作する領域を選択してください。\n"
                                    "操作の種類・重み・回数や詳細パラメータは、追加後に③操作パラメータ"
                                    "パネルでこのステップを選択して設定します。"),
                    this));
    m_wholeWindowRadio = new QRadioButton(QStringLiteral("対象GUIの全領域（自動追従）"), this);
    m_namedRegionRadio = new QRadioButton(QStringLiteral("登録済みの操作領域から選択:"), this);
    layout->addWidget(m_wholeWindowRadio);
    auto *namedRegionRow = new QHBoxLayout;
    namedRegionRow->addWidget(m_namedRegionRadio);
    m_namedRegionCombo = new QComboBox(this);
    for (const NamedRegion &region : availableRegions)
        m_namedRegionCombo->addItem(region.name);
    namedRegionRow->addWidget(m_namedRegionCombo, 1);
    layout->addLayout(namedRegionRow);
    connect(m_wholeWindowRadio, &QRadioButton::toggled, this, &StepEditorDialog::onModeChanged);

    if (availableRegions.isEmpty()) {
        m_namedRegionRadio->setEnabled(false);
        m_namedRegionCombo->setEnabled(false);
        m_namedRegionCombo->addItem(QStringLiteral("（①対象選択パネルで操作領域を追加してください）"));
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    // Populate from `initial`.
    if (initial.useWholeWindow || availableRegions.isEmpty()) {
        m_wholeWindowRadio->setChecked(true);
    } else {
        m_namedRegionRadio->setChecked(true);
        const int idx = m_namedRegionCombo->findText(initial.regionName);
        m_namedRegionCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    onModeChanged();
    resize(420, 200);
}

void StepEditorDialog::onModeChanged()
{
    const bool named = m_namedRegionRadio->isChecked();
    m_namedRegionCombo->setEnabled(named && m_namedRegionCombo->count() > 0 && m_namedRegionRadio->isEnabled());
}

bool StepEditorDialog::useWholeWindow() const
{
    return m_wholeWindowRadio->isChecked();
}

QString StepEditorDialog::regionName() const
{
    return m_namedRegionRadio->isChecked() ? m_namedRegionCombo->currentText() : QString();
}
