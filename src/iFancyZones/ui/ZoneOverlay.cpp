#include "ZoneOverlay.h"

#include "core/LayoutEngine.h"
#include "platform/mac/AppKitBridge.h"
#include "ui/PopoverTheme.h"

#include <QPainter>
#include <QWindow>

namespace ifz {

ZoneOverlay::ZoneOverlay(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint
                 | Qt::Tool
                 | Qt::WindowDoesNotAcceptFocus
                 | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
}

void ZoneOverlay::setLayoutAndCanvas(const Layout &layout, const QRect &canvas)
{
    m_layout = layout;
    m_canvas = canvas;
    setGeometry(canvas);
    update();
}

void ZoneOverlay::setActiveZone(int index)
{
    if (m_active == index) return;
    m_active = index;
    update();
}

void ZoneOverlay::setShowNumbers(bool on)
{
    m_showNumbers = on;
    update();
}

QRect ZoneOverlay::pixelRectOfZone(int index) const
{
    const auto &zones = m_layout.zones();
    if (index < 0 || index >= zones.size()) return QRect();
    return LayoutEngine::zoneToPixels(zones[index], m_canvas, m_layout.gap());
}

QRect ZoneOverlay::pixelRectOfZoneOuter(int index) const
{
    const auto &zones = m_layout.zones();
    if (index < 0 || index >= zones.size()) return QRect();
    return LayoutEngine::zoneToPixelsRaw(zones[index], m_canvas);
}

void ZoneOverlay::configureNative()
{
    if (m_nativeConfigured) return;
    if (QWindow *w = windowHandle()) {
        AppKitBridge::configureAsOverlay(w);
        m_nativeConfigured = true;
    }
}

void ZoneOverlay::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    configureNative();
}

void ZoneOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto &zones = m_layout.zones();
    // Translate global canvas pixels into widget-local coords.
    const QPoint offset = -m_canvas.topLeft();

    for (int i = 0; i < zones.size(); ++i) {
        QRect r = LayoutEngine::zoneToPixels(zones[i], m_canvas, m_layout.gap()).translated(offset);

        QColor fill, border;
        if (i == m_active) {
            fill   = theme::zoneColor(110);
            border = theme::zoneColor(240);
        } else {
            fill   = QColor(255, 255, 255, 90);
            border = QColor(255, 255, 255, 200);
        }

        p.setBrush(fill);
        p.setPen(QPen(border, i == m_active ? 3 : 2));
        p.drawRoundedRect(r, 10, 10);

        if (m_showNumbers) {
            QRect badge(r.left() + 12, r.top() + 12, 32, 32);
            p.setBrush(QColor(0, 0, 0, 140));
            p.setPen(Qt::NoPen);
            p.drawEllipse(badge);
            p.setPen(Qt::white);
            QFont f = p.font();
            f.setBold(true);
            f.setPointSize(14);
            p.setFont(f);
            p.drawText(badge, Qt::AlignCenter, QString::number(i + 1));
        }
    }
}

} // namespace ifz
