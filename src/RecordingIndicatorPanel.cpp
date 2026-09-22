#include "RecordingIndicatorPanel.h"
#include "I18n.h"

#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

RecordingIndicatorPanel::RecordingIndicatorPanel(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto *layout = new QVBoxLayout(this);
    auto *titleLabel = new QLabel(I18n::t(QStringLiteral("● 記録中... (Escで終了)")), this);
    titleLabel->setStyleSheet("color: white; font-weight: bold;");
    m_countLabel = new QLabel(I18n::t(QStringLiteral("記録件数: 0")), this);
    m_countLabel->setStyleSheet("color: white;");

    auto *stopButton = new QPushButton(I18n::t(QStringLiteral("■ 記録終了")), this);
    stopButton->setStyleSheet(
        "QPushButton { background-color: #d32f2f; color: white; font-weight: bold; "
        "font-size: 16px; padding: 10px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #b71c1c; }");
    connect(stopButton, &QPushButton::clicked, this, &RecordingIndicatorPanel::stopRequested);

    layout->addWidget(titleLabel);
    layout->addWidget(m_countLabel);
    layout->addWidget(stopButton);

    setStyleSheet("background-color: rgba(30, 30, 30, 235); border-radius: 8px;");
    resize(240, 120);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.right() - width() - 24, avail.top() + 24);
    }
}

void RecordingIndicatorPanel::setRecordedActionCount(int count)
{
    m_countLabel->setText(I18n::t(QStringLiteral("記録件数: %1")).arg(count));
}
