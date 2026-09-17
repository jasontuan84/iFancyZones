#include "LayoutEngine.h"

#include <QPoint>

namespace ifz {

QRect LayoutEngine::zoneToPixels(const Zone &zone, const QRect &canvas, int gap)
{
    QRect raw = zoneToPixelsRaw(zone, canvas);
    return raw.adjusted(gap, gap, -gap, -gap);
}

QRect LayoutEngine::zoneToPixelsRaw(const Zone &zone, const QRect &canvas)
{
    const QRectF n = zone.normalized();
    int x = canvas.x() + qRound(n.x()     * canvas.width());
    int y = canvas.y() + qRound(n.y()     * canvas.height());
    int w =              qRound(n.width() * canvas.width());
    int h =              qRound(n.height()* canvas.height());
    return QRect(x, y, w, h);
}

QRectF LayoutEngine::pixelsToNormalized(const QRect &pixelRect, const QRect &canvas)
{
    if (canvas.width() <= 0 || canvas.height() <= 0) {
        return QRectF(0, 0, 1, 1);
    }
    double cw = canvas.width();
    double ch = canvas.height();
    double x = (pixelRect.x() - canvas.x()) / cw;
    double y = (pixelRect.y() - canvas.y()) / ch;
    double w = pixelRect.width()  / cw;
    double h = pixelRect.height() / ch;
    return QRectF(x, y, w, h);
}

int LayoutEngine::hitTest(const Layout &layout, const QRect &canvas, const QPoint &point)
{
    const auto &zones = layout.zones();
    // Iterate in reverse: last zone is "topmost" in z-order.
    for (int i = zones.size() - 1; i >= 0; --i) {
        QRect r = zoneToPixelsRaw(zones[i], canvas);
        if (r.contains(point)) {
            return i;
        }
    }
    return -1;
}

} // namespace ifz
