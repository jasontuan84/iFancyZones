#include "AccessibilityBridge.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

// Private symbol: returns the CGWindowID for a given AX window. Used by
// yabai/Rectangle/Hammerspoon and everyone else doing cross-process window
// management. Weak-imported so the binary still loads if it ever disappears.
extern "C" AXError _AXUIElementGetWindow(AXUIElementRef element,
                                         CGWindowID *identifier)
    __attribute__((weak_import));

namespace ifz {

struct AccessibilityBridge::Impl {
    AXUIElementRef systemWide = nullptr;
};

namespace {

AXUIElementRef CopyAppElementForPid(pid_t pid) {
    return AXUIElementCreateApplication(pid);
}

AXUIElementRef CopyFocusedWindow(AXUIElementRef app) {
    AXUIElementRef win = nullptr;
    AXError err = AXUIElementCopyAttributeValue(app,
        kAXFocusedWindowAttribute, (CFTypeRef *)&win);
    if (err != kAXErrorSuccess) return nullptr;
    return win;
}

AXError SetWindowFrame(AXUIElementRef win, const QRect &target) {
    CGPoint p = CGPointMake(target.x(), target.y());
    CGSize  s = CGSizeMake(target.width(), target.height());
    AXValueRef pos  = AXValueCreate((AXValueType)kAXValueCGPointType, &p);
    AXValueRef size = AXValueCreate((AXValueType)kAXValueCGSizeType,  &s);

    AXError err1 = kAXErrorSuccess;
    AXError err2 = kAXErrorSuccess;
    if (pos) {
        err1 = AXUIElementSetAttributeValue(win, kAXPositionAttribute, pos);
        CFRelease(pos);
    }
    if (size) {
        err2 = AXUIElementSetAttributeValue(win, kAXSizeAttribute, size);
        CFRelease(size);
    }

    // Re-read the actual size and centre the window horizontally over the
    // zone. When the app honoured our size request this is a no-op (actual
    // width == zone width, so the offset is 0). But many apps clamp to a
    // minimum width and ignore the shrink: without this they'd stay pinned to
    // the zone's left edge and overflow to the right. Centring makes a clamped
    // window straddle the zone's centre line instead.
    CGFloat finalX = target.x();
    CFTypeRef sizeRef = nullptr;
    if (AXUIElementCopyAttributeValue(win, kAXSizeAttribute, &sizeRef) == kAXErrorSuccess) {
        CGSize actual = CGSizeZero;
        AXValueGetValue((AXValueRef)sizeRef, (AXValueType)kAXValueCGSizeType, &actual);
        CFRelease(sizeRef);
        finalX = target.x() + (target.width() - actual.width) / 2.0;
    }

    // Re-apply position (centred X, zone Y): some apps clamp size and shift
    // the origin during the size write, so this must come last.
    p = CGPointMake(finalX, target.y());
    pos = AXValueCreate((AXValueType)kAXValueCGPointType, &p);
    if (pos) {
        AXUIElementSetAttributeValue(win, kAXPositionAttribute, pos);
        CFRelease(pos);
    }
    if (err1 != kAXErrorSuccess) return err1;
    return err2;
}

QRect FrameOfWindow(AXUIElementRef win) {
    CGPoint p = CGPointZero;
    CGSize  s = CGSizeZero;
    CFTypeRef posRef = nullptr;
    CFTypeRef sizeRef = nullptr;
    if (AXUIElementCopyAttributeValue(win, kAXPositionAttribute, &posRef) != kAXErrorSuccess) {
        return QRect();
    }
    if (AXUIElementCopyAttributeValue(win, kAXSizeAttribute, &sizeRef) != kAXErrorSuccess) {
        CFRelease(posRef);
        return QRect();
    }
    AXValueGetValue((AXValueRef)posRef,  (AXValueType)kAXValueCGPointType, &p);
    AXValueGetValue((AXValueRef)sizeRef, (AXValueType)kAXValueCGSizeType,  &s);
    CFRelease(posRef);
    CFRelease(sizeRef);
    return QRect(int(p.x), int(p.y), int(s.width), int(s.height));
}

pid_t PidForWindowAtPoint(CGPoint point) {
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!list) return 0;

    pid_t result = 0;
    CFIndex count = CFArrayGetCount(list);
    for (CFIndex i = 0; i < count; ++i) {
        CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(list, i);
        CFNumberRef layerRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowLayer);
        int layer = 0;
        if (layerRef) CFNumberGetValue(layerRef, kCFNumberIntType, &layer);
        if (layer != 0) continue; // skip menu bar, dock, etc.

        CFDictionaryRef bounds = (CFDictionaryRef)CFDictionaryGetValue(info, kCGWindowBounds);
        if (!bounds) continue;
        CGRect r;
        if (!CGRectMakeWithDictionaryRepresentation(bounds, &r)) continue;
        if (CGRectContainsPoint(r, point)) {
            CFNumberRef pidRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowOwnerPID);
            if (pidRef) {
                int pid = 0;
                CFNumberGetValue(pidRef, kCFNumberIntType, &pid);
                result = (pid_t)pid;
            }
            break;
        }
    }
    CFRelease(list);
    return result;
}

} // unnamed namespace

AccessibilityBridge::AccessibilityBridge(QObject *parent)
    : QObject(parent), d(new Impl)
{
    d->systemWide = AXUIElementCreateSystemWide();
}

AccessibilityBridge::~AccessibilityBridge()
{
    if (d->systemWide) CFRelease(d->systemWide);
    delete d;
}

bool AccessibilityBridge::isTrusted() const
{
    return AXIsProcessTrusted();
}

bool AccessibilityBridge::requestTrust()
{
    NSDictionary *opts = @{
        (__bridge id)kAXTrustedCheckOptionPrompt : @YES
    };
    return AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)opts);
}

bool AccessibilityBridge::moveFrontmostWindow(const QRect &targetGlobal)
{
    NSRunningApplication *front = NSWorkspace.sharedWorkspace.frontmostApplication;
    if (!front) return false;

    AXUIElementRef app = CopyAppElementForPid(front.processIdentifier);
    AXUIElementRef win = CopyFocusedWindow(app);
    if (!win) {
        if (app) CFRelease(app);
        return false;
    }
    AXError e = SetWindowFrame(win, targetGlobal);
    CFRelease(win);
    if (app) CFRelease(app);
    return e == kAXErrorSuccess;
}

bool AccessibilityBridge::moveWindowAtPoint(const QPoint &screenPoint, const QRect &targetGlobal)
{
    CGPoint cgp = CGPointMake(screenPoint.x(), screenPoint.y());
    pid_t pid = PidForWindowAtPoint(cgp);
    if (pid == 0) return moveFrontmostWindow(targetGlobal);

    AXUIElementRef app = CopyAppElementForPid(pid);
    AXUIElementRef win = CopyFocusedWindow(app);
    if (!win) {
        if (app) CFRelease(app);
        return false;
    }
    AXError e = SetWindowFrame(win, targetGlobal);
    CFRelease(win);
    if (app) CFRelease(app);
    return e == kAXErrorSuccess;
}

AccessibilityBridge::WindowHandle AccessibilityBridge::captureFocusedWindow()
{
    NSRunningApplication *front = NSWorkspace.sharedWorkspace.frontmostApplication;
    if (!front) return nullptr;

    AXUIElementRef app = CopyAppElementForPid(front.processIdentifier);
    if (!app) return nullptr;
    AXUIElementRef win = CopyFocusedWindow(app);
    CFRelease(app);
    if (!win) return nullptr;

    return (WindowHandle)win; // ownership transferred to the caller
}

bool AccessibilityBridge::moveCapturedWindow(WindowHandle h, const QRect &targetGlobal)
{
    if (!h) return false;
    AXError e = SetWindowFrame((AXUIElementRef)h, targetGlobal);
    if (e != kAXErrorSuccess) {
        NSLog(@"[iFancyZones] moveCapturedWindow AX error %d", (int)e);
    }
    return e == kAXErrorSuccess;
}

void AccessibilityBridge::releaseWindowHandle(WindowHandle h)
{
    if (h) CFRelease((CFTypeRef)h);
}

QRect AccessibilityBridge::frameOfFocusedWindow(qint64 *outPid)
{
    if (outPid) *outPid = 0;
    NSRunningApplication *front = NSWorkspace.sharedWorkspace.frontmostApplication;
    if (!front) return QRect();
    if (outPid) *outPid = (qint64)front.processIdentifier;

    AXUIElementRef app = CopyAppElementForPid(front.processIdentifier);
    if (!app) return QRect();
    AXUIElementRef win = CopyFocusedWindow(app);
    CFRelease(app);
    if (!win) return QRect();

    QRect r = FrameOfWindow(win);
    CFRelease(win);
    return r;
}

bool AccessibilityBridge::raiseWindow(qint64 pid, quint32 cgWindowID)
{
    if (pid <= 0) return false;
    AXUIElementRef app = CopyAppElementForPid((pid_t)pid);
    if (!app) return false;

    CFArrayRef windows = nullptr;
    AXError err = AXUIElementCopyAttributeValue(app, kAXWindowsAttribute,
                                                (CFTypeRef *)&windows);
    if (err != kAXErrorSuccess || !windows) {
        if (windows) CFRelease(windows);
        CFRelease(app);
        return false;
    }

    AXUIElementRef target = nullptr;
    if (&_AXUIElementGetWindow != nullptr) {
        for (CFIndex i = 0; i < CFArrayGetCount(windows); ++i) {
            AXUIElementRef w = (AXUIElementRef)CFArrayGetValueAtIndex(windows, i);
            CGWindowID wid = 0;
            if (_AXUIElementGetWindow(w, &wid) == kAXErrorSuccess && wid == cgWindowID) {
                target = w;
                CFRetain(target);
                break;
            }
        }
    }
    if (!target && CFArrayGetCount(windows) > 0) {
        // Fallback: no private symbol or no match — use the first window.
        target = (AXUIElementRef)CFArrayGetValueAtIndex(windows, 0);
        CFRetain(target);
    }
    CFRelease(windows);

    bool ok = false;
    if (target) {
        AXUIElementPerformAction(target, kAXRaiseAction);
        AXUIElementSetAttributeValue(target, kAXMainAttribute, kCFBooleanTrue);
        AXUIElementSetAttributeValue(target, kAXFocusedAttribute, kCFBooleanTrue);
        CFRelease(target);

        NSRunningApplication *appNs =
            [NSRunningApplication runningApplicationWithProcessIdentifier:(pid_t)pid];
        if (appNs) {
            [appNs activateWithOptions:NSApplicationActivateIgnoringOtherApps];
            ok = true;
        }
    }
    CFRelease(app);
    return ok;
}

QRect AccessibilityBridge::frameOfWindowAtPoint(const QPoint &screenPoint)
{
    CGPoint cgp = CGPointMake(screenPoint.x(), screenPoint.y());
    pid_t pid = PidForWindowAtPoint(cgp);
    if (pid == 0) return QRect();

    AXUIElementRef app = CopyAppElementForPid(pid);
    AXUIElementRef win = CopyFocusedWindow(app);
    if (!win) {
        if (app) CFRelease(app);
        return QRect();
    }
    QRect r = FrameOfWindow(win);
    CFRelease(win);
    if (app) CFRelease(app);
    return r;
}

} // namespace ifz
