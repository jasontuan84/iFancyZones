#pragma once

#include "core/Monitor.h"

#include <QList>

class QScreen;

namespace ifz {

// Builds Monitor records (with stable CGDisplayUnitNumber) from Qt's QScreens.
class MacScreenInfo {
public:
    static QList<Monitor> currentMonitors();

    // Best-effort: map a QScreen back to its CGDisplayUnitNumber.
    // Returns 0 if not found.
    static quint32 unitNumberFor(const QScreen *screen);
};

} // namespace ifz
