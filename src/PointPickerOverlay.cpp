#include "PointPickerOverlay.h"
#include "I18n.h"
#include "OverlayGeometry.h"

#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

using OverlayGeometry::globalPosOf;
using OverlayGeometry::grabVirtualDesktopSnapshot;
using OverlayGeometry::virtualDesktopGeometry;

PointPickerOverlay::PointPickerOverlay(QWidget *parent)
    : QWidget(parent)
    // Must happen before this (still invisible) widget is shown -- see
    // grabVirtualDesktopSnapshot()'s comment.
    , m_backgroundSnapshot(grabVirtualDesktopSnapshot())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    // Same rationale as RegionSelectorOverlay: keep this the active modal
    // surface instead of fighting a still-running modal dialog underneath
    // (e.g. SetupActionEditorDialog) while it's up.
    setWindowModality(Qt::ApplicationModal);
    // Deliberately NOT Qt::WA_TranslucentBackground -- see
    // RegionSelectorOverlay's constructor for why (paintEvent() draws
    // m_backgroundSnapshot as an opaque background instead).
    setAttribute(Qt::WA_DeleteOnClose, false);
    setCursor(Qt::CrossCursor);
    setGeometry(virtualDesktopGeometry());
}

bool PointPickerOverlay::run(QPoint &outPoint)
{
    PointPickerOverlay overlay;
    // Deliberately NOT showFullScreen() -- see RegionSelectorOverlay::run()
    // for why (native fullscreen hides the target window on macOS).
    overlay.show();
    overlay.setGeometry(virtualDesktopGeometry());
    overlay.activateWindow();
    overlay.raise();

    QEventLoop loop;
    QObject::connect(&overlay, &PointPickerOverlay::finishedPicking, &loop, &QEventLoop::quit);
    loop.exec();

    if (overlay.m_accepted) {
        outPoint = overlay.m_pickedPoint;
        return true;
    }
    return false;
}

void PointPickerOverlay::finish(bool accepted, const QPoint &pt)
{
    // See RegionSelectorOverlay::finish()'s identical guard: without it,
    // a queued second mousePressEvent/keyPressEvent for this same overlay
    // (e.g. a click and an Escape arriving in quick succession) could run
    // finish() twice, re-emitting finishedPicking() after run()'s local
    // QEventLoop/overlay has already gone away.
    if (m_finished)
        return;
    m_finished = true;
    m_accepted = accepted;
    m_pickedPoint = pt;
    close();
    emit finishedPicking();
}

void PointPickerOverlay::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.drawPixmap(0, 0, m_backgroundSnapshot);
    p.fillRect(rect(), QColor(0, 0, 0, 70));

    p.setPen(Qt::white);
    p.setFont(QFont(font().family(), 14, QFont::Bold));
    const QString hint =
        I18n::t(QStringLiteral("クリックした位置を座標として使用します  ―  Esc でキャンセル"));
    p.drawText(QRect(20, 16, width() - 40, 30), Qt::AlignLeft | Qt::AlignVCenter, hint);
}

void PointPickerOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        finish(true, globalPosOf(event));
}

void PointPickerOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        finish(false, QPoint());
}
