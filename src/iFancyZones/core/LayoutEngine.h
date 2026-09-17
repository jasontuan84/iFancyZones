#pragma once

#include "Layout.h"

#include <QRect>
#include <QRectF>
#include <QSize>

namespace ifz {

// Pure functions: convert between normalized [0,1] zone rects and pixel rects
// on a particular canvas (= screen available area).
class LayoutEngine {
public:
    // Convert a normalized zone rect to a pixel rect on `canvas`.
    // `gap` is subtracted from all four sides so windows don't touch.
    static QRect zoneToPixels(const Zone &zone, const QRect &canvas, int gap);

    // Convert a normalized zone rect to a pixel rect on `canvas` WITHOUT gap.
    // Used for editor rendering and overlay highlight.
    static QRect zoneToPixelsRaw(const Zone &zone, const QRect &canvas);

    // Convert a pixel rect (in canvas-local pixels) to a normalized zone rect.
    // Used in the editor when the user drags/resizes a ZoneEditorWidget.
    static QRectF pixelsToNormalized(const QRect &pixelRect, const QRect &canvas);

    // Find the topmost zone (last in z-order) whose pixel rect contains `point`.
    // Returns -1 if `point` is outside all zones.
    static int hitTest(const Layout &layout, const QRect &canvas, const QPoint &point);
};

} // namespace ifz
