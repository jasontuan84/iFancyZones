#include "MacScreenInfo.h"

#include <QGuiApplication>
#include <QScreen>

#import <AppKit/AppKit.h>
#import <ColorSync/ColorSync.h>
#import <CoreGraphics/CoreGraphics.h>

#include <algorithm>

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

// Persistent key of a display, see Monitor::key.
QString DisplayKey(CGDirectDisplayID did)
{
    if (did == 0) return QString();
    if (CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(did)) {
        CFStringRef str = CFUUIDCreateString(kCFAllocatorDefault, uuid);
        CFRelease(uuid);
        if (str) {
            const QString key = QString::fromCFString(str);
            CFRelease(str);
            if (!key.isEmpty()) return key;
        }
    }
    // No ColorSync UUID (should not happen for a real display): fall back to
    // the EDID identity, which still tells two monitors with serials apart.
    return QStringLiteral("edid:%1-%2-%3")
        .arg(CGDisplayVendorNumber(did))
        .arg(CGDisplayModelNumber(did))
        .arg(CGDisplaySerialNumber(did));
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
    int i = -1;
    for (NSScreen *ns in NSScreen.screens) {
        ++i;
        CGDirectDisplayID did = DisplayIdForNSScreen(ns);
        const quint32 unit = did != 0 ? CGDisplayUnitNumber(did) : 0;
        NSLog(@"[iFancyZones]   NSScreen[%d]: name='%@' displayId=%u unitNumber=%u "
              @"key=%@ frame=(%.0f,%.0f,%.0fx%.0f) backingScale=%.1f",
              i, ns.localizedName, did, unit, DisplayKey(did).toNSString(),
              ns.frame.origin.x, ns.frame.origin.y,
              ns.frame.size.width, ns.frame.size.height,
              ns.backingScaleFactor);

        if (did == 0) {
            NSLog(@"[iFancyZones]     -> SKIPPED (displayId == 0)");
            continue;
        }

        Monitor m;
        m.displayId = (quint32)did;
        m.unitNumber = unit;
        m.key = DisplayKey(did);
        m.name = QString::fromNSString(ns.localizedName);
        m.geometry          = CocoaFrameToQt(ns.frame,        primaryHeight);
        m.availableGeometry = CocoaFrameToQt(ns.visibleFrame, primaryHeight);
        m.isMain = (did == CGMainDisplayID());
        const CGSize mm = CGDisplayScreenSize(did);
        m.physicalSizeMm = QSizeF(mm.width, mm.height);
        out.append(m);
    }

    // NSScreen.screens lists the menu-bar display first and the rest in no
    // useful order. Number the displays by where they sit instead.
    std::stable_sort(out.begin(), out.end(), [](const Monitor &a, const Monitor &b) {
        if (a.geometry.x() != b.geometry.x()) return a.geometry.x() < b.geometry.x();
        return a.geometry.y() < b.geometry.y();
    });
    for (int k = 0; k < out.size(); ++k) out[k].index = k + 1;

    NSLog(@"[iFancyZones] MacScreenInfo: produced %d Monitor entries", (int)out.size());
    return out;
}

QString MacScreenInfo::displayKeyFor(const QScreen *screen)
{
    if (!screen) return QString();

    NSScreen *primary = NSScreen.screens.firstObject;
    if (!primary) return QString();
    const CGFloat primaryHeight = primary.frame.size.height;

    // Match by centre-point containment: a screen's centre lies inside
    // exactly one NSScreen's bounds (screens never overlap). Never by name:
    // two monitors of the same model share it.
    const QPoint qCenter = screen->geometry().center();
    for (NSScreen *ns in NSScreen.screens) {
        const QRect nsg = CocoaFrameToQt(ns.frame, primaryHeight);
        if (nsg.contains(qCenter)) return DisplayKey(DisplayIdForNSScreen(ns));
    }
    return QString();
}

} // namespace ifz
