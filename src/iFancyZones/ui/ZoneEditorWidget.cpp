#include "ZoneEditorWidget.h"

#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace ifz {

namespace {
constexpr int kBadgeDiameter = 28;
constexpr int kPresetIconSize = 52;
constexpr int kPresetIconSpacing = 10;
constexpr int kPresetCols = 4;
constexpr int kPresetRows = 3;
constexpr int kDeleteSize = 22;

// Each preset icon is a tiny "screen thumbnail" showing the filled rectangle
// (as a fraction of the icon's inner area) that the preset will place a zone
// into. Indexes match the order in applyPreset().
struct PresetFill {
    double x, y, w, h;
};
constexpr PresetFill kPresetFills[12] = {
    { 0.0,     0.0, 0.5,     1.0 },     // 0  left 1/2
    { 0.5,     0.0, 0.5,     1.0 },     // 1  right 1/2
    { 0.0,     0.0, 1.0,     0.5 },     // 2  top 1/2
    { 0.0,     0.5, 1.0,     0.5 },     // 3  bottom 1/2
    { 0.0,     0.0, 0.5,     0.5 },     // 4  TL quad
    { 0.5,     0.0, 0.5,     0.5 },     // 5  TR quad
    { 0.0,     0.5, 0.5,     0.5 },     // 6  BL quad
    { 0.5,     0.5, 0.5,     0.5 },     // 7  BR quad
    { 0.0,         0.0, 1.0/3.0, 1.0 }, // 8  left 1/3
    { 1.0/3.0,     0.0, 1.0/3.0, 1.0 }, // 9  middle 1/3
    { 2.0/3.0,     0.0, 1.0/3.0, 1.0 }, // 10 right 1/3
    { 0.0,         0.0, 1.0,     1.0 }, // 11 full
};
} // unnamed namespace

ZoneEditorWidget::ZoneEditorWidget(int index, const QRect &canvas, QWidget *parent)
    : QWidget(parent), m_index(index), m_canvas(canvas)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
}

void ZoneEditorWidget::setIndex(int i) { m_index = i; update(); }
void ZoneEditorWidget::setCanvas(const QRect &c) { m_canvas = c; clampToCanvas(); }

void ZoneEditorWidget::setGap(int g)
{
    if (g < 0) g = 0;
    if (g > 100) g = 100;
    if (m_gap == g) return;
    m_gap = g;
    update();
}

void ZoneEditorWidget::setSelected(bool s)
{
    if (m_selected == s) return;
    m_selected = s;
    update();
}

QRect ZoneEditorWidget::zoneRect() const { return geometry(); }

void ZoneEditorWidget::setZoneRect(const QRect &r)
{
    setGeometry(r);
    clampToCanvas();
}

ZoneEditorWidget::DragMode ZoneEditorWidget::hitTest(const QPoint &p) const
{
    const QRect r = rect();
    bool l = p.x() <= kEdge;
    bool t = p.y() <= kEdge;
    bool right = p.x() >= r.width() - kEdge;
    bool bot   = p.y() >= r.height() - kEdge;
    if (l && t)       return DragMode::ResizeTL;
    if (right && t)   return DragMode::ResizeTR;
    if (l && bot)     return DragMode::ResizeBL;
    if (right && bot) return DragMode::ResizeBR;
    if (l)            return DragMode::ResizeL;
    if (right)        return DragMode::ResizeR;
    if (t)            return DragMode::ResizeT;
    if (bot)          return DragMode::ResizeB;
    return DragMode::Move;
}

void ZoneEditorWidget::updateCursor(const QPoint &p)
{
    switch (hitTest(p)) {
        case DragMode::ResizeL:
        case DragMode::ResizeR:  setCursor(Qt::SizeHorCursor); break;
        case DragMode::ResizeT:
        case DragMode::ResizeB:  setCursor(Qt::SizeVerCursor); break;
        case DragMode::ResizeTL:
        case DragMode::ResizeBR: setCursor(Qt::SizeFDiagCursor); break;
        case DragMode::ResizeTR:
        case DragMode::ResizeBL: setCursor(Qt::SizeBDiagCursor); break;
        default:                 setCursor(Qt::SizeAllCursor); break;
    }
}

void ZoneEditorWidget::clampToCanvas()
{
    QRect g = geometry();
    g.setWidth(std::max(g.width(), kMinSize));
    g.setHeight(std::max(g.height(), kMinSize));
    if (g.left() < m_canvas.left())   g.moveLeft(m_canvas.left());
    if (g.top() < m_canvas.top())     g.moveTop(m_canvas.top());
    if (g.right() > m_canvas.right()) g.moveRight(m_canvas.right());
    if (g.bottom() > m_canvas.bottom()) g.moveBottom(m_canvas.bottom());
    setGeometry(g);
}

QRect ZoneEditorWidget::deleteButtonRect() const
{
    QRect r = rect().adjusted(m_gap, m_gap, -m_gap, -m_gap);
    return QRect(r.right() - kDeleteSize - 6, r.top() + 6, kDeleteSize, kDeleteSize);
}

QRect ZoneEditorWidget::presetIconRect(int i) const
{
    // 4 columns × 3 rows centered in the zone:
    //   row 0 (halves):     L, R, T, B
    //   row 1 (quadrants):  TL, TR, BL, BR
    //   row 2 (thirds+full): L⅓, M⅓, R⅓, Full
    const QRect r = rect().adjusted(m_gap, m_gap, -m_gap, -m_gap);
    const int totalW = kPresetCols * kPresetIconSize + (kPresetCols - 1) * kPresetIconSpacing;
    const int totalH = kPresetRows * kPresetIconSize + (kPresetRows - 1) * kPresetIconSpacing;
    const int x0 = r.left() + (r.width()  - totalW) / 2;
    const int y0 = r.top()  + (r.height() - totalH) / 2;
    const int col = i % kPresetCols;
    const int row = i / kPresetCols;
    return QRect(x0 + col * (kPresetIconSize + kPresetIconSpacing),
                 y0 + row * (kPresetIconSize + kPresetIconSpacing),
                 kPresetIconSize, kPresetIconSize);
}

void ZoneEditorWidget::applyPreset(int i)
{
    QRect r = m_canvas;
    int third = r.width() / 3;
    int halfW = r.width() / 2;
    int halfH = r.height() / 2;

    switch (i) {
        case 0: r = QRect(r.left(),          r.top(), halfW, r.height()); break;            // left 1/2
        case 1: r = QRect(r.left() + halfW,  r.top(), r.width() - halfW, r.height()); break;// right 1/2
        case 2: r = QRect(r.left(), r.top(),          r.width(), halfH); break;             // top 1/2
        case 3: r = QRect(r.left(), r.top() + halfH,  r.width(), r.height() - halfH); break;// bottom 1/2
        case 4: r = QRect(r.left(),         r.top(),         halfW, halfH); break;          // TL quad
        case 5: r = QRect(r.left() + halfW, r.top(),         r.width() - halfW, halfH); break; // TR quad
        case 6: r = QRect(r.left(),         r.top() + halfH, halfW, r.height() - halfH); break; // BL
        case 7: r = QRect(r.left() + halfW, r.top() + halfH, r.width() - halfW, r.height() - halfH); break; // BR
        case 8: r = QRect(r.left(),                  r.top(), third, r.height()); break;
        case 9: r = QRect(r.left() + third,          r.top(), third, r.height()); break;
        case 10:r = QRect(r.left() + 2 * third,      r.top(), r.width() - 2 * third, r.height()); break;
        case 11:r = m_canvas; break;
        default: return;
    }
    setGeometry(r);
    clampToCanvas();
    emit changed();
    update();
}

void ZoneEditorWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    emit selected();

    const QPoint p = e->position().toPoint();

    if (m_selected && deleteButtonRect().contains(p)) {
        emit deleteRequested();
        return;
    }

    if (m_selected) {
        for (int i = 0; i < 12; ++i) {
            if (presetIconRect(i).contains(p)) {
                applyPreset(i);
                return;
            }
        }
    }

    m_drag = hitTest(p);
    m_dragOrigin = e->globalPosition().toPoint();
    m_startGeom = geometry();
}

void ZoneEditorWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_drag == DragMode::None) {
        updateCursor(e->position().toPoint());
        return;
    }
    const QPoint cur = e->globalPosition().toPoint();
    const int dx = cur.x() - m_dragOrigin.x();
    const int dy = cur.y() - m_dragOrigin.y();
    QRect g = m_startGeom;
    switch (m_drag) {
        case DragMode::Move:     g.translate(dx, dy); break;
        case DragMode::ResizeL:  g.setLeft(g.left() + dx); break;
        case DragMode::ResizeR:  g.setRight(g.right() + dx); break;
        case DragMode::ResizeT:  g.setTop(g.top() + dy); break;
        case DragMode::ResizeB:  g.setBottom(g.bottom() + dy); break;
        case DragMode::ResizeTL: g.setTopLeft(g.topLeft() + QPoint(dx, dy)); break;
        case DragMode::ResizeTR: g.setTopRight(g.topRight() + QPoint(dx, dy)); break;
        case DragMode::ResizeBL: g.setBottomLeft(g.bottomLeft() + QPoint(dx, dy)); break;
        case DragMode::ResizeBR: g.setBottomRight(g.bottomRight() + QPoint(dx, dy)); break;
        default: break;
    }
    if (g.width() < kMinSize)  g.setWidth(kMinSize);
    if (g.height() < kMinSize) g.setHeight(kMinSize);
    setGeometry(g);
    clampToCanvas();
    emit changed();
}

void ZoneEditorWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = DragMode::None;
}

void ZoneEditorWidget::enterEvent(QEnterEvent *)
{
    m_hovered = true;
    update();
}

void ZoneEditorWidget::leaveEvent(QEvent *)
{
    m_hovered = false;
    setCursor(Qt::ArrowCursor);
    update();
}

void ZoneEditorWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor fill(60, 200, 120, 96);
    QColor border(60, 200, 120, 220);
    if (m_selected) {
        border.setAlpha(255);
    }

    // Render the zone shrunk by `gap` on each side. Two adjacent zones
    // therefore display 2*gap pixels of empty space between them.
    QRect r = rect().adjusted(m_gap + 1, m_gap + 1, -m_gap - 1, -m_gap - 1);
    if (r.width() <= 0 || r.height() <= 0) return;

    p.setBrush(fill);
    p.setPen(QPen(border, m_selected ? 1.5 : 1.0));
    p.drawRoundedRect(r, 8, 8);

    // Number badge
    QRect badge(r.left() + 8, r.top() + 8, kBadgeDiameter, kBadgeDiameter);
    p.setBrush(QColor(255, 255, 255, 230));
    p.setPen(Qt::NoPen);
    p.drawEllipse(badge);
    p.setPen(QColor(30, 30, 30));
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(13);
    p.setFont(f);
    p.drawText(badge, Qt::AlignCenter, QString::number(m_index + 1));

    if (m_selected || m_hovered) {
        // Delete button
        QRect del = deleteButtonRect();
        p.setBrush(QColor(220, 60, 60, 240));
        p.setPen(Qt::NoPen);
        p.drawEllipse(del);
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(del.topLeft() + QPoint(7, 7), del.bottomRight() - QPoint(7, 7));
        p.drawLine(del.topRight() + QPoint(-7, 7), del.bottomLeft() + QPoint(7, -7));
    }

    const int gridW = kPresetCols * kPresetIconSize + (kPresetCols - 1) * kPresetIconSpacing;
    const int gridH = kPresetRows * kPresetIconSize + (kPresetRows - 1) * kPresetIconSpacing;
    if (m_selected && r.width() > gridW + 32 && r.height() > gridH + 80) {
        // Bottom-center strip of 12 thumbnail icons. Each icon = a tiny screen
        // frame with the preset's target rectangle filled, so the user can
        // visually pick the layout at a glance.
        for (int i = 0; i < 12; ++i) {
            const QRect ir = presetIconRect(i);

            // Dark rounded chip background with subtle border.
            p.setBrush(QColor(0, 0, 0, 165));
            p.setPen(QPen(QColor(255, 255, 255, 60), 1));
            p.drawRoundedRect(ir, 8, 8);

            // Inner "screen" area with a thin outline.
            const QRect inner = ir.adjusted(8, 8, -8, -8);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 255, 255, 130), 1));
            p.drawRoundedRect(inner, 3, 3);

            // Filled rectangle representing the zone the preset would create.
            const PresetFill &pf = kPresetFills[i];
            QRect fill(
                inner.left() + qRound(pf.x * inner.width()),
                inner.top()  + qRound(pf.y * inner.height()),
                qRound(pf.w * inner.width()),
                qRound(pf.h * inner.height())
            );
            // Shrink slightly so the user can see the boundary lines for
            // half/third/quarter layouts.
            fill.adjust(1, 1, -1, -1);
            if (fill.width() < 1)  fill.setWidth(1);
            if (fill.height() < 1) fill.setHeight(1);

            p.setBrush(QColor(255, 255, 255, 235));
            p.setPen(Qt::NoPen);
            p.drawRect(fill);
        }
    }
}

} // namespace ifz
