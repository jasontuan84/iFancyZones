#pragma once

#include "core/Layout.h"

#include <QWidget>

namespace ifz {

// Click-through overlay shown over one QScreen during a Space-gated drag.
// Inactive zones are drawn with white translucent fill; the zone under
// the cursor (active zone) is drawn green.
class ZoneOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ZoneOverlay(QWidget *parent = nullptr);

    // Set the layout (geometry) and canvas area (= screen.availableGeometry()).
    void setLayoutAndCanvas(const Layout &layout, const QRect &canvas);

    // Index of the zone currently under the cursor. -1 = none.
    void setActiveZone(int index);
    int  activeZoneIndex() const { return m_active; }

    // Whether to show zone numbers in the overlay.
    void setShowNumbers(bool on);

    // Convenience: returns the pixel rect (in global screen coords)
    // of zone `index`, accounting for gap. Use this as the snap target.
    QRect pixelRectOfZone(int index) const;

    // The "outer" pixel rect of a zone — no gap applied. Use this for hit
    // testing so the user doesn't have to land exactly inside the gap-shrunk
    // green area; the whole logical zone is clickable.
    QRect pixelRectOfZoneOuter(int index) const;

    // Make the underlying NSWindow be NSScreenSaverWindowLevel + click-through.
    // Should be called after first show() so the platform window exists.
    void configureNative();

protected:
    void paintEvent(QPaintEvent *e) override;
    void showEvent(QShowEvent *e) override;

private:
    Layout m_layout;
    QRect  m_canvas;
    int    m_active = -1;
    bool   m_showNumbers = true;
    bool   m_nativeConfigured = false;
};

} // namespace ifz
