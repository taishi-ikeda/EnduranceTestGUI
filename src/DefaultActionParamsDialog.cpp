#include "DefaultActionParamsDialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include "ActionKindEditor.h"
#include "ActionParamsEditor.h"
#include "I18n.h"

DefaultActionParamsDialog::DefaultActionParamsDialog(const RegionStep &initialKinds,
                                                       const ActionParams &initialParams, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::t(QStringLiteral("デフォルトの操作設定")));

    auto *layout = new QVBoxLayout(this);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *scrollContent = new QWidget;
    scroll->setWidget(scrollContent);
    auto *scrollLayout = new QVBoxLayout(scrollContent);

    auto *kindGroup = new QGroupBox(I18n::t(QStringLiteral("デフォルトの操作種別・重み・回数")), scrollContent);
    auto *kindLayout = new QVBoxLayout(kindGroup);
    auto *kindHintLabel = new QLabel(
        I18n::t(QStringLiteral("②で新しくステップを追加したときの初期値です。既存のステップには"
                        "影響しません。")),
        kindGroup);
    kindHintLabel->setWordWrap(true);
    kindLayout->addWidget(kindHintLabel);
    m_kindEditor = new ActionKindEditor(kindGroup);
    kindLayout->addWidget(m_kindEditor);
    scrollLayout->addWidget(kindGroup);

    auto *paramsGroup = new QGroupBox(I18n::t(QStringLiteral("デフォルトの操作の詳細設定")), scrollContent);
    auto *paramsLayout = new QVBoxLayout(paramsGroup);
    auto *paramsHintLabel = new QLabel(
        I18n::t(QStringLiteral("②で「デフォルトを使う」になっているすべてのステップに適用されます。")),
        paramsGroup);
    paramsHintLabel->setWordWrap(true);
    paramsLayout->addWidget(paramsHintLabel);
    m_paramsEditor = new ActionParamsEditor(paramsGroup);
    paramsLayout->addWidget(m_paramsEditor);
    scrollLayout->addWidget(paramsGroup, 1);

    layout->addWidget(scroll, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    m_kindEditor->setKinds(initialKinds);
    m_paramsEditor->setParams(initialParams);
    resize(560, 760);
}

RegionStep DefaultActionParamsDialog::resultKinds() const
{
    RegionStep step;
    m_kindEditor->applyKindsTo(step);
    return step;
}

ActionParams DefaultActionParamsDialog::resultParams() const
{
    return m_paramsEditor->params();
}
