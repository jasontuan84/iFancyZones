#pragma once

#include "core/Monitor.h"

#include <QList>

class QScreen;

namespace ifz {

// Builds Monitor records for the connected displays.
class MacScreenInfo {
public:
    // Ordered left to right (then top to bottom), numbered from 1 in that
    // order, so "Detect" and the Displays tab match the physical arrangement.
    static QList<Monitor> currentMonitors();

    // The persistent Monitor::key of the display showing `screen`.
    // Empty if not found.
    static QString displayKeyFor(const QScreen *screen);
};

} // namespace ifz
