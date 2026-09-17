#include "MacScreenInfo.h"

#include <QGuiApplication>
#include <QScreen>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

namespace ifz {

namespace {

CGDirectDisplayID DisplayIdForNSScreen(NSScreen *screen)
{
    if (!screen) return 0;
    NSNumber *num = screen.deviceDescription[@"NSScreenNumber"];
    if (!num) return 0;
    return (CGDirectDisplayID)num.unsignedIntValue;
}

// Convert an NSScreen.frame (Cocoa bottom-left primary origin) to Qt's
// top-left primary-origin QRect.
QRect CocoaFrameToQt(CGRect frame, CGFloat primaryHeight)
{
    int topY = (int)qRound(primaryHeight - (frame.origin.y + frame.size.height));
    return QRect((int)qRound(frame.origin.x), topY,
                 (int)qRound(frame.size.width), (int)qRound(frame.size.height));
}

} // unnamed namespace

QList<Monitor> MacScreenInfo::currentMonitors()
{
    QList<Monitor> out;
    NSScreen *primary = NSScreen.screens.firstObject;
    if (!primary) {
        NSLog(@"[iFancyZones] MacScreenInfo: NSScreen.screens is EMPTY");
        return out;
    }
    const CGFloat primaryHeight = primary.frame.size.height;

    // Diagnostic dump so the user can verify what AppKit reports.
    NSLog(@"╔══════════════════════════════════════════════════════════╗");
    NSLog(@"║ iFancyZones screen detection diagnostic                  ║");
    NSLog(@"╚══════════════════════════════════════════════════════════╝");
    NSLog(@"[iFancyZones] NSScreen.screens.count = %lu",
          (unsigned long)NSScreen.screens.count);

    // Also enumerate Quartz's full online display list — this includes
    // mirror sets that AppKit would collapse into a single NSScreen.
    {
        uint32_t maxDisplays = 16;
        CGDirectDisplayID ids[16];
        uint32_t actual = 0;
        CGGetOnlineDisplayList(maxDisplays, ids, &actual);
        NSLog(@"[iFancyZones] CGGetOnlineDisplayList -> %u display(s)", actual);
        for (uint32_t k = 0; k < actual; ++k) {
            CGDirectDisplayID d = ids[k];
            CGRect b = CGDisplayBounds(d);
            NSLog(@"[iFancyZones]   CG[%u]: displayId=%u unitNumber=%u "
                  @"isActive=%d isAsleep=%d isMain=%d isOnline=%d "
                  @"mirrorSource=%u bounds=(%.0f,%.0f,%.0fx%.0f)",
                  k, d, CGDisplayUnitNumber(d),
                  CGDisplayIsActive(d), CGDisplayIsAsleep(d),
                  CGDisplayIsMain(d), CGDisplayIsOnline(d),
                  CGDisplayMirrorsDisplay(d),
                  b.origin.x, b.origin.y, b.size.width, b.size.height);
        }
    }

    // Iterate NSScreen.screens directly: every physical display has its own
    // entry, regardless of whether two displays share a model name. This
    // bypasses any QScreen → NSScreen mapping problems.
    int idx = 1;
    int i = -1;
    for (NSScreen *ns in NSScreen.screens) {
        ++i;
        CGDirectDisplayID did = DisplayIdForNSScreen(ns);
        const quint32 unit = did != 0 ? CGDisplayUnitNumber(did) : 0;
        NSLog(@"[iFancyZones]   NSScreen[%d]: name='%@' displayId=%u unitNumber=%u "
              @"frame=(%.0f,%.0f,%.0fx%.0f) backingScale=%.1f",
              i, ns.localizedName, did, unit,
              ns.frame.origin.x, ns.frame.origin.y,
              ns.frame.size.width, ns.frame.size.height,
              ns.backingScaleFactor);

        if (did == 0) {
            NSLog(@"[iFancyZones]     -> SKIPPED (displayId == 0)");
            continue;
        }

        Monitor m;
        m.index = idx++;
        m.displayId = (quint32)did;
        m.unitNumber = unit;
        m.name = QString::fromNSString(ns.localizedName);
        m.geometry          = CocoaFrameToQt(ns.frame,        primaryHeight);
        m.availableGeometry = CocoaFrameToQt(ns.visibleFrame, primaryHeight);
        out.append(m);
    }
    NSLog(@"[iFancyZones] MacScreenInfo: produced %d Monitor entries", (int)out.size());
    return out;
}

quint32 MacScreenInfo::unitNumberFor(const QScreen *screen)
{
    if (!screen) return 0;

    NSScreen *primary = NSScreen.screens.firstObject;
    if (!primary) return 0;
    const CGFloat primaryHeight = primary.frame.size.height;

    // Match by center-point containment: a screen's centre lies inside
    // exactly one NSScreen's bounds (screens never overlap). Robust against
    // sub-point geometry differences between Qt and Cocoa.
    const QPoint qCenter = screen->geometry().center();

    auto stableKeyFor = [](CGDirectDisplayID did) -> quint32 {
        if (did == 0) return 0;
        quint32 unit = CGDisplayUnitNumber(did);
        if (unit != 0) return unit;
        // Some main displays report unitNumber=0; fall back to displayId but
        // namespace-separate it (high bit set) so we never collide with a
        // real unit number on another monitor. Mirrors Monitor::stableKey().
        return Monitor::kDisplayIdNamespaceBit | (quint32)did;
    };

    for (NSScreen *ns in NSScreen.screens) {
        const QRect nsg = CocoaFrameToQt(ns.frame, primaryHeight);
        if (nsg.contains(qCenter)) {
            return stableKeyFor(DisplayIdForNSScreen(ns));
        }
    }

    // Last-resort fallback: name match.
    NSString *qname = screen->name().toNSString();
    for (NSScreen *ns in NSScreen.screens) {
        if ([ns.localizedName isEqualToString:qname]) {
            return stableKeyFor(DisplayIdForNSScreen(ns));
        }
    }
    return 0;
}

} // namespace ifz
