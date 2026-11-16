#pragma once

#include <QRect>
#include <QSizeF>
#include <QString>

#include <cmath>

namespace ifz {

// Identity of a physical display.
//
// `key` is what bindings persist under. It is the display's ColorSync UUID
// (CGDisplayCreateUUIDFromDisplayID), which macOS itself uses to remember the
// display arrangement. It is derived from the EDID vendor, model and serial
// number, so two identical monitors still get different keys and keep them
// across reboots, sleep and reconnects.
//
// CGDisplayUnitNumber and CGDirectDisplayID are NOT stable: with two monitors
// of the same model they are handed out in enumeration order and can swap
// between sessions, which used to swap the layouts of the two displays.
struct Monitor {
    int index = 0;                // 1-based, left-to-right, assigned by MacScreenInfo
    quint32 displayId = 0;        // CGDirectDisplayID for live screens (per session)
    quint32 unitNumber = 0;       // CGDisplayUnitNumber (per session, only for migration)
    QString key;                  // persistent per-physical-display key, see above
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

    // The key bindings were saved under up to 1.7.0. Used only to carry old
    // bindings over to `key` for the displays connected at first launch.
    static constexpr quint32 kDisplayIdNamespaceBit = 0x80000000u;
    quint32 legacyKey() const {
        if (unitNumber != 0) return unitNumber;
        if (displayId == 0) return 0;
        return kDisplayIdNamespaceBit | displayId;
    }
};

} // namespace ifz
