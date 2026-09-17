#include "ScreenNumberBadge.h"

#include "platform/mac/AppKitBridge.h"

#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QWindow>

namespace ifz {

// ─── ScreenNumberBadge ──────────────────────────────────────────────────

ScreenNumberBadge::ScreenNumberBadge(int number, int diameter, QWidget *parent)
    : QWidget(parent), m_number(number), m_diameter(diameter)
{
    setFixedSize(diameter, diameter);
    setAttribute(Qt::WA_TranslucentBackground);
}

void ScreenNumberBadge::setNumber(int n)
{
    m_number = n;
    update();
}

void ScreenNumberBadge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r(1, 1, m_diameter - 2, m_diameter - 2);
    p.setBrush(QColor(60, 200, 120, 240));
    p.setPen(Qt::NoPen);
    p.drawEllipse(r);

    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(qMax(10, m_diameter / 2 - 1));
    p.setFont(f);
    p.drawText(r, Qt::AlignCenter, QString::number(m_number));
}

// ─── ScreenIdentifierOverlay ────────────────────────────────────────────

ScreenIdentifierOverlay::ScreenIdentifierOverlay(int number, const QRect &screenGeometry)
    : QWidget(nullptr), m_number(number)
{
    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint
                 | Qt::Tool
                 | Qt::WindowDoesNotAcceptFocus
                 | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);
    setGeometry(screenGeometry);
}

void ScreenIdentifierOverlay::flash(int durationMs)
{
    show();
    raise();
    QTimer::singleShot(durationMs, this, &QWidget::close);
}

void ScreenIdentifierOverlay::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (!m_native) {
        if (QWindow *w = windowHandle()) {
            AppKitBridge::configureAsOverlay(w);
            m_native = true;
        }
    }
}

void ScreenIdentifierOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Large green circle in the centre of the screen.
    const int diameter = qMin(width(), height()) / 3;
    const QPoint c(width() / 2, height() / 2);
    QRect circle(c.x() - diameter / 2, c.y() - diameter / 2, diameter, diameter);

    // Drop shadow effect by stacked semi-transparent ellipses.
    for (int i = 16; i > 0; i -= 4) {
        p.setBrush(QColor(0, 0, 0, 30));
        p.setPen(Qt::NoPen);
        p.drawEllipse(circle.adjusted(-i, -i, i, i));
    }

    p.setBrush(QColor(60, 200, 120, 245));
    p.setPen(QPen(QColor(255, 255, 255, 200), 6));
    p.drawEllipse(circle);

    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(diameter / 2);
    p.setFont(f);
    p.drawText(circle, Qt::AlignCenter, QString::number(m_number));
}

} // namespace ifz
