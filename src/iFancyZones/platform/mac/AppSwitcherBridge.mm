#include "AppSwitcherBridge.h"

#import <AppKit/AppKit.h>

#include <unistd.h>

namespace ifz {

namespace {

// Render an NSImage into a QPixmap at `px` logical points, scaled up by the
// backing scale factor so it stays crisp on Retina displays.
QPixmap PixmapFromNSImage(NSImage *image, int px, qreal dpr)
{
    if (!image || px <= 0) return QPixmap();
    const int side = int(px * dpr);
    if (side <= 0) return QPixmap();

    QImage qimg(side, side, QImage::Format_RGBA8888_Premultiplied);
    qimg.fill(Qt::transparent);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        qimg.bits(), side, side, 8, qimg.bytesPerLine(), cs,
        (CGBitmapInfo)((uint32_t)kCGImageAlphaPremultipliedLast
                       | (uint32_t)kCGBitmapByteOrder32Big));
    CGColorSpaceRelease(cs);
    if (!ctx) return QPixmap();

    NSGraphicsContext *nsctx =
        [NSGraphicsContext graphicsContextWithCGContext:ctx flipped:NO];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:nsctx];
    [image drawInRect:NSMakeRect(0, 0, side, side)
             fromRect:NSZeroRect
            operation:NSCompositingOperationSourceOver
             fraction:1.0];
    [NSGraphicsContext restoreGraphicsState];
    CGContextRelease(ctx);

    QPixmap pm = QPixmap::fromImage(qimg);
    pm.setDevicePixelRatio(dpr);
    return pm;
}

qreal MainScreenScale()
{
    qreal dpr = 1.0;
    if (NSScreen *main = [NSScreen mainScreen]) dpr = main.backingScaleFactor;
    return dpr < 1.0 ? 1.0 : dpr;
}

} // unnamed namespace

QList<RunningAppInfo> AppSwitcherBridge::runningApps(int iconPx)
{
    QList<RunningAppInfo> out;

    NSWorkspace *ws = [NSWorkspace sharedWorkspace];
    NSRunningApplication *frontmost = ws.frontmostApplication;
    const pid_t selfPid = getpid();
    const qreal dpr = MainScreenScale();

    for (NSRunningApplication *app in ws.runningApplications) {
        // Only "regular" apps (those with a Dock tile / menu bar). Skips
        // agents, daemons, and our own accessory process.
        if (app.activationPolicy != NSApplicationActivationPolicyRegular) continue;
        if (app.processIdentifier == selfPid) continue;
        if (app.isTerminated) continue;

        RunningAppInfo info;
        info.pid = app.processIdentifier;
        info.name = app.localizedName ? QString::fromNSString(app.localizedName)
                                       : QStringLiteral("(unknown)");
        info.bundleId = app.bundleIdentifier
                            ? QString::fromNSString(app.bundleIdentifier) : QString();
        info.icon = PixmapFromNSImage(app.icon, iconPx, dpr);
        info.active = (frontmost && app.processIdentifier == frontmost.processIdentifier);

        // Keep the frontmost app at the head so a quick open+release lands on
        // the next-most-recent app (Cmd-Tab style).
        if (info.active) out.prepend(info);
        else             out.append(info);
    }

    return out;
}

bool AppSwitcherBridge::activateApp(qint64 pid)
{
    NSRunningApplication *app =
        [NSRunningApplication runningApplicationWithProcessIdentifier:(pid_t)pid];
    if (!app) return false;
    return [app activateWithOptions:NSApplicationActivateAllWindows];
}

} // namespace ifz
