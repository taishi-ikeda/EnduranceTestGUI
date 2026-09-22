#include "RegionSelectorOverlay.h"
#include "I18n.h"
#include "OverlayGeometry.h"

#include <QApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

using OverlayGeometry::globalPosOf;
using OverlayGeometry::grabVirtualDesktopSnapshot;
using OverlayGeometry::virtualDesktopGeometry;

RegionSelectorOverlay::RegionSelectorOverlay(Mode mode, const QList<QRect> &existingIncludes,
                                              const QList<QRect> &existingExcludes,
                                              QWidget *parent)
    : QWidget(parent)
    , m_mode(mode)
    , m_existingIncludes(existingIncludes)
    , m_existingExcludes(existingExcludes)
    // Must happen before this (still invisible) widget is shown -- see
    // grabVirtualDesktopSnapshot()'s comment.
    , m_backgroundSnapshot(grabVirtualDesktopSnapshot())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    // This overlay is typically launched from inside a modal QDialog
    // (StepEditorDialog) that stays running (hidden or not) for the
    // duration -- on macOS a QDialog::exec() modal session doesn't end
    // just because the dialog is hidden. Making this overlay itself
    // application-modal makes it the active modal surface while it's up,
    // instead of being a separate, unrelated top-level window fighting
    // the still-active modal session underneath (which was left the app
    // unresponsive after the overlay closed).
    setWindowModality(Qt::ApplicationModal);
    // Deliberately NOT Qt::WA_TranslucentBackground: this window paints
    // m_backgroundSnapshot as an opaque background instead (see paintEvent()
    // and grabVirtualDesktopSnapshot()'s comment for why -- real
    // window-level translucency renders as solid black on several Linux
    // window managers with no compositor running).
    setAttribute(Qt::WA_DeleteOnClose, false);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    setGeometry(virtualDesktopGeometry());
}

QList<QRect> RegionSelectorOverlay::run(Mode mode, const QList<QRect> &existingIncludes,
                                         const QList<QRect> &existingExcludes)
{
    RegionSelectorOverlay overlay(mode, existingIncludes, existingExcludes);
    // Deliberately NOT showFullScreen(): on macOS that enters native
    // fullscreen (a separate Space), which hides every other window --
    // including the target app the user needs to see to draw regions
    // over. A plain show() at the explicit virtual-desktop geometry set in
    // the constructor covers the same area as a borderless always-on-top
    // window without leaving the normal desktop/Space, so the background
    // snapshot painted in paintEvent() (captured in the constructor, before
    // this show()) still matches what the user actually sees underneath.
    overlay.show();
    overlay.setGeometry(virtualDesktopGeometry());
    overlay.activateWindow();
    overlay.raise();

    QEventLoop loop;
    QObject::connect(&overlay, &RegionSelectorOverlay::finishedSelecting, &loop, &QEventLoop::quit);
    loop.exec();

    return overlay.m_accepted ? overlay.m_newRects : QList<QRect>();
}

void RegionSelectorOverlay::finish(bool accepted)
{
    m_accepted = accepted;
    m_finished = true;
    close();
    emit finishedSelecting();
}

void RegionSelectorOverlay::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.drawPixmap(0, 0, m_backgroundSnapshot);
    p.fillRect(rect(), QColor(0, 0, 0, 70));

    const QPoint origin = geometry().topLeft();

    auto drawRectList = [&](const QList<QRect> &rects, const QColor &fill, const QColor &border) {
        for (const QRect &r : rects) {
            const QRect local = r.translated(-origin);
            p.setPen(QPen(border, 2));
            p.setBrush(fill);
            p.drawRect(local);
        }
    };

    drawRectList(m_existingIncludes, QColor(0, 200, 0, 40), QColor(0, 200, 0, 160));
    drawRectList(m_existingExcludes, QColor(220, 0, 0, 60), QColor(220, 0, 0, 180));

    const QColor newFill = (m_mode == Mode::Include) ? QColor(0, 220, 0, 90)
                                                       : QColor(230, 30, 30, 110);
    const QColor newBorder = (m_mode == Mode::Include) ? QColor(0, 255, 0) : QColor(255, 60, 60);
    drawRectList(m_newRects, newFill, newBorder);

    if (m_dragging) {
        const QRect local = m_currentRect.translated(-origin);
        p.setPen(QPen(newBorder, 2, Qt::DashLine));
        p.setBrush(newFill);
        p.drawRect(local);
    }

    p.setPen(Qt::white);
    p.setFont(QFont(font().family(), 14, QFont::Bold));
    const QString modeLabel =
        (m_mode == Mode::Include) ? I18n::t(QStringLiteral("含める領域を選択中")) : I18n::t(QStringLiteral("除外(マスク)領域を選択中"));
    const QString hint = I18n::t(QStringLiteral("%1  ―  ドラッグで矩形を追加（複数可） / Enter または右クリックで確定 / Esc でキャンセル"))
                              .arg(modeLabel);
    p.drawText(QRect(20, 16, width() - 40, 30), Qt::AlignLeft | Qt::AlignVCenter, hint);
}

void RegionSelectorOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = globalPosOf(event);
        m_currentRect = QRect(m_dragStart, m_dragStart);
        update();
    } else if (event->button() == Qt::RightButton) {
        finish(true);
    }
}

void RegionSelectorOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_currentRect = QRect(m_dragStart, globalPosOf(event)).normalized();
        update();
    }
}

void RegionSelectorOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        if (m_currentRect.width() >= 4 && m_currentRect.height() >= 4)
            m_newRects.append(m_currentRect);
        m_currentRect = QRect();
        update();
    }
}

void RegionSelectorOverlay::mouseDoubleClickEvent(QMouseEvent * /*event*/)
{
    finish(true);
}

void RegionSelectorOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        finish(false);
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finish(true);
    }
}
