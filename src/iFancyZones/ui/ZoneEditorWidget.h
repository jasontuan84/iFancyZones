#pragma once

#include <QPoint>
#include <QWidget>

namespace ifz {

// One zone inside the layout editor canvas. Translucent green rectangle,
// supports drag (interior) + resize (edges/corners) + delete button +
// auto-alignment preset icons.
class ZoneEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit ZoneEditorWidget(int index, const QRect &canvas, QWidget *parent = nullptr);

    void setIndex(int i);
    int index() const { return m_index; }

    void setCanvas(const QRect &canvas);
    QRect canvas() const { return m_canvas; }

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

    // The zone rectangle in canvas-local pixels (canvas.x()/y() == 0,0 in its frame).
    QRect zoneRect() const;
    void setZoneRect(const QRect &r);

    // The "gap" inset applied at paint time. The widget's geometry stays
    // the OUTER zone rectangle (so drag/resize edits the logical zone);
    // the rendered fill/border/badge is shrunk by `gap` on every side.
    // Visual spacing between adjacent zones is therefore 2 * gap.
    void setGap(int gap);
    int  gap() const { return m_gap; }

signals:
    void deleteRequested();
    void selected();
    void changed();

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    enum class DragMode { None, Move, ResizeL, ResizeR, ResizeT, ResizeB,
                          ResizeTL, ResizeTR, ResizeBL, ResizeBR };

    DragMode hitTest(const QPoint &localPos) const;
    void updateCursor(const QPoint &localPos);
    void clampToCanvas();
    QRect deleteButtonRect() const;
    QRect presetIconRect(int i) const;
    void applyPreset(int i);

    int m_index;
    QRect m_canvas;       // size of the editor canvas (origin (0,0))
    int  m_gap = 0;
    bool m_selected = false;
    bool m_hovered = false;

    DragMode m_drag = DragMode::None;
    QPoint m_dragOrigin;
    QRect m_startGeom;

    static constexpr int kEdge = 8;       // resize hit-test width
    static constexpr int kMinSize = 40;
};

} // namespace ifz
