#pragma once

#include <QRect>
#include <QSizeF>
#include <QString>

#include <cmath>

namespace ifz {

// Stable identity for a physical display. On macOS we use CGDisplayUnitNumber;
// it survives reconnects of the same physical screen.
struct Monitor {
    int index = 0;                // 1-based, assigned by MacScreenInfo
    quint32 displayId = 0;        // CGDirectDisplayID for live screens
    quint32 unitNumber = 0;       // CGDisplayUnitNumber (stable per-physical-display when non-zero)
    QString name;                 // human-readable, e.g. "Built-in Retina Display"
    QRect geometry;               // in Qt's top-left coordinate system, points
    QRect availableGeometry;      // minus menu bar + Dock
    bool isMain = false;          // hosts the menu bar
    QSizeF physicalSizeMm;        // CGDisplayScreenSize; empty when unknown

    // Panel diagonal in inches, rounded. 0 when the display reports no
    // physical size (happens for some virtual and mirrored displays).
    int diagonalInches() const {
        if (physicalSizeMm.isEmpty()) return 0;
        const double mm = std::hypot(physicalSizeMm.width(), physicalSizeMm.height());
        return int(std::lround(mm / 25.4));
    }

    // 21:9 and wider. Used only to label the display in the UI.
    bool isUltrawide() const {
        if (geometry.height() <= 0) return false;
        return double(geometry.width()) / double(geometry.height()) >= 2.1;
    }

    // The main display sometimes reports unitNumber=0 even though it is
    // online and usable. We treat displayId as the truth: any CGDirectDisplayID
    // assigned by Quartz means the display exists this session.
    bool isValid() const { return displayId != 0; }

    // Stable per-physical-display key for persistence.
    //
    // CGDisplayUnitNumber is the preferred ID (it persists across reconnects).
    // When it is 0 (happens for the main display on some macOS versions) we
    // fall back to displayId — but we **set the high bit** so the fallback
    // lives in a separate numeric namespace from real unit numbers. Otherwise
    // displayId=1 would collide with unitNumber=1 of a different monitor.
    static constexpr quint32 kDisplayIdNamespaceBit = 0x80000000u;
    quint32 stableKey() const {
        if (unitNumber != 0) return unitNumber;
        if (displayId == 0) return 0;
        return kDisplayIdNamespaceBit | displayId;
    }
};

} // namespace ifz
