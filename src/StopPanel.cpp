#include "StopPanel.h"
#include "I18n.h"

#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

StopPanel::StopPanel(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto *layout = new QVBoxLayout(this);
    m_countLabel = new QLabel(I18n::t(QStringLiteral("実行回数: 0")), this);
    m_countLabel->setStyleSheet("color: white; font-weight: bold;");

    m_pauseResumeButton = new QPushButton(I18n::t(QStringLiteral("‖ 一時停止")), this);
    m_pauseResumeButton->setStyleSheet(
        "QPushButton { background-color: #455a64; color: white; font-weight: bold; "
        "font-size: 14px; padding: 8px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #37474f; }");
    connect(m_pauseResumeButton, &QPushButton::clicked, this, &StopPanel::pauseResumeRequested);

    auto *stopButton = new QPushButton(I18n::t(QStringLiteral("■ 停止 (STOP)")), this);
    stopButton->setStyleSheet(
        "QPushButton { background-color: #d32f2f; color: white; font-weight: bold; "
        "font-size: 16px; padding: 10px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #b71c1c; }");
    connect(stopButton, &QPushButton::clicked, this, &StopPanel::stopRequested);

    layout->addWidget(m_countLabel);
    layout->addWidget(m_pauseResumeButton);
    layout->addWidget(stopButton);

    setStyleSheet("background-color: rgba(30, 30, 30, 235); border-radius: 8px;");
    resize(220, 140);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.right() - width() - 24, avail.top() + 24);
    }
}

void StopPanel::setIterationCount(qint64 count)
{
    m_countLabel->setText(I18n::t(QStringLiteral("実行回数: %1")).arg(count));
}

void StopPanel::setPaused(bool paused)
{
    m_pauseResumeButton->setText(paused ? I18n::t(QStringLiteral("▶ 再開")) : I18n::t(QStringLiteral("‖ 一時停止")));
}
