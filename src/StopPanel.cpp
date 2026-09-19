#include "StopPanel.h"

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
    m_countLabel = new QLabel(QStringLiteral("実行回数: 0"), this);
    m_countLabel->setStyleSheet("color: white; font-weight: bold;");

    auto *stopButton = new QPushButton(QStringLiteral("■ 停止 (STOP)"), this);
    stopButton->setStyleSheet(
        "QPushButton { background-color: #d32f2f; color: white; font-weight: bold; "
        "font-size: 16px; padding: 10px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #b71c1c; }");
    connect(stopButton, &QPushButton::clicked, this, &StopPanel::stopRequested);

    layout->addWidget(m_countLabel);
    layout->addWidget(stopButton);

    setStyleSheet("background-color: rgba(30, 30, 30, 235); border-radius: 8px;");
    resize(220, 90);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.right() - width() - 24, avail.top() + 24);
    }
}

void StopPanel::setIterationCount(qint64 count)
{
    m_countLabel->setText(QStringLiteral("実行回数: %1").arg(count));
}
