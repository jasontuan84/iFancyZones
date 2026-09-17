#pragma once

#include <QColor>
#include <QString>

class QPainter;
class QRectF;
class QWidget;

namespace ifz {

class Layout;

// Shared look for the menu-bar popover: one place for the palette and for the
// bits of chrome both tabs draw (cards, section rules, layout previews).
namespace theme {

// Surfaces, drawn on top of the window's vibrancy layer.
inline QColor card()          { return QColor(255, 255, 255, 14); }
inline QColor cardHover()     { return QColor(255, 255, 255, 26); }
inline QColor cardBorder()    { return QColor(255, 255, 255, 28); }
inline QColor rule()          { return QColor(255, 255, 255, 46); }

// Text.
inline QColor text()          { return QColor(0xEC, 0xEF, 0xF4); }
inline QColor textDim()       { return QColor(0x9A, 0xA2, 0xB2); }
inline QColor textFaint()     { return QColor(0x76, 0x7E, 0x8E); }

// Accents.
inline QColor accent()        { return QColor(0x3B, 0x82, 0xF6); } // selection blue
inline QColor accentFill()    { return QColor(0x3B, 0x82, 0xF6, 40); }
inline QColor running()       { return QColor(0x30, 0xD1, 0x58); } // status dot
inline QColor warn()          { return QColor(0xF5, 0xA5, 0x24); } // AX banner

inline int cardRadius()       { return 10; }

// Paint a layout's zones into `target`, scaled to fit and centred. Used for
// both the display cards and the layout rows, so a layout always reads the
// same wherever it appears. `zoneColor` distinguishes the active layout.
void paintLayoutPreview(QPainter *p, const QRectF &target, const Layout &l,
                        const QColor &zoneColor);

// A dashed placeholder, for a layout that has no zones configured yet.
void paintEmptyPreview(QPainter *p, const QRectF &target);

// Stylesheet for the whole popover. Applied once on the root widget.
QString styleSheet();

} // namespace theme
} // namespace ifz
