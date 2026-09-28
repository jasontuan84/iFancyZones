#include "PopoverTheme.h"

#include "core/Layout.h"
#include "core/Zone.h"

#include <QPainter>
#include <QRectF>

namespace ifz {
namespace theme {

namespace {
QColor g_zoneColor(60, 200, 120);
} // unnamed namespace

QColor zoneColor(int alpha)
{
    QColor c = g_zoneColor;
    c.setAlpha(alpha);
    return c;
}

void setZoneColor(const QColor &c)
{
    if (c.isValid()) g_zoneColor = c;
}

void paintLayoutPreview(QPainter *p, const QRectF &target, const Layout &l,
                        const QColor &zoneColor)
{
    if (l.zones().isEmpty()) {
        paintEmptyPreview(p, target);
        return;
    }

    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(zoneColor);
    for (const Zone &z : l.zones()) {
        const QRectF n = z.normalized();
        QRectF r(target.left() + n.x() * target.width(),
                 target.top() + n.y() * target.height(),
                 n.width() * target.width(),
                 n.height() * target.height());
        // A uniform inset is what reads as a gap between zones at this size;
        // the layout's own gap is in screen pixels and vanishes when scaled.
        r = r.adjusted(1, 1, -1, -1);
        if (r.width() <= 0 || r.height() <= 0) continue;
        p->drawRoundedRect(r, 1.5, 1.5);
    }
    p->restore();
}

void paintEmptyPreview(QPainter *p, const QRectF &target)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    QPen pen(textFaint(), 1, Qt::DashLine);
    pen.setDashPattern({3, 3});
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(target.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    p->restore();
}

QString styleSheet()
{
    return QStringLiteral(R"(
        QWidget { color: #ECEFF4; font-size: 13px; }
        QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical { background: transparent; width: 9px; margin: 2px; }
        QScrollBar::handle:vertical {
            background: rgba(255,255,255,55); border-radius: 4px; min-height: 28px;
        }
        QScrollBar::handle:vertical:hover { background: rgba(255,255,255,85); }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

        QPushButton {
            background: rgba(255,255,255,22);
            border: 1px solid rgba(255,255,255,26);
            border-radius: 7px;
            padding: 5px 12px;
            font-size: 12px;
        }
        QPushButton:hover  { background: rgba(255,255,255,38); }
        QPushButton:pressed{ background: rgba(255,255,255,55); }
        QPushButton:disabled { color: rgba(236,239,244,90); }
        QPushButton#primaryWarn {
            background: #F5A524; color: #1A1206; border: 0; font-weight: 600;
        }
        QPushButton#primaryWarn:hover { background: #FFB93C; }

        QComboBox {
            background: rgba(255,255,255,26);
            border: 1px solid rgba(255,255,255,26);
            border-radius: 7px;
            padding: 4px 8px;
            font-size: 12px;
            min-width: 76px;
        }
        QComboBox:hover { background: rgba(255,255,255,40); }
        QComboBox::drop-down { border: 0; width: 18px; }

        QLineEdit {
            background: rgba(255,255,255,18);
            border: 1px solid rgba(255,255,255,26);
            border-radius: 8px;
            padding: 6px 10px 6px 28px;
            selection-background-color: #3B82F6;
        }
        QLineEdit:focus { border: 1px solid rgba(59,130,246,160); }
    )");
}

} // namespace theme
} // namespace ifz
