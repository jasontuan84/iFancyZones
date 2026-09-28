#include "AccessibilityBridge.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

#include <dlfcn.h>
#include <string.h>
#include <unistd.h>

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

struct WindowHit {
    pid_t pid = 0;
    CGWindowID wid = 0;
    CGRect frame = CGRectZero;
};

// The topmost normal-layer window under `point`. Skips our own windows (the
// zone overlays) and fully transparent helper windows, so it returns the
// window the user sees under the cursor.
WindowHit WindowAtPoint(CGPoint point) {
    WindowHit hit;
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!list) return hit;

    const pid_t selfPid = getpid();
    CFIndex count = CFArrayGetCount(list);
    for (CFIndex i = 0; i < count; ++i) {
        CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(list, i);
        CFNumberRef layerRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowLayer);
        int layer = 0;
        if (layerRef) CFNumberGetValue(layerRef, kCFNumberIntType, &layer);
        if (layer != 0) continue; // skip menu bar, dock, etc.

        CFNumberRef alphaRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowAlpha);
        double alpha = 1.0;
        if (alphaRef) CFNumberGetValue(alphaRef, kCFNumberDoubleType, &alpha);
        if (alpha <= 0.0) continue;

        CFNumberRef pidRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowOwnerPID);
        int pid = 0;
        if (pidRef) CFNumberGetValue(pidRef, kCFNumberIntType, &pid);
        if (pid == 0 || pid == selfPid) continue;

        CFDictionaryRef bounds = (CFDictionaryRef)CFDictionaryGetValue(info, kCGWindowBounds);
        if (!bounds) continue;
        CGRect r;
        if (!CGRectMakeWithDictionaryRepresentation(bounds, &r)) continue;
        if (!CGRectContainsPoint(r, point)) continue;

        CFNumberRef widRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowNumber);
        int wid = 0;
        if (widRef) CFNumberGetValue(widRef, kCFNumberIntType, &wid);
        hit.pid = (pid_t)pid;
        hit.wid = (CGWindowID)wid;
        hit.frame = r;
        break;
    }
    CFRelease(list);
    return hit;
}

pid_t PidForWindowAtPoint(CGPoint point) {
    return WindowAtPoint(point).pid;
}

// The AX element of window `wid` in app `pid`, retained. Matches by
// CGWindowID when the private symbol exists. Otherwise, or when the app does
// not map ids (some Java windows), it matches by frame. Returns nullptr when
// nothing matches, so the caller never gets a sibling window by mistake.
AXUIElementRef CopyWindowById(pid_t pid, CGWindowID wid, CGRect frame) {
    AXUIElementRef app = CopyAppElementForPid(pid);
    if (!app) return nullptr;
    CFArrayRef windows = nullptr;
    AXError err = AXUIElementCopyAttributeValue(app, kAXWindowsAttribute,
                                                (CFTypeRef *)&windows);
    CFRelease(app);
    if (err != kAXErrorSuccess || !windows) {
        if (windows) CFRelease(windows);
        return nullptr;
    }

    AXUIElementRef target = nullptr;
    const CFIndex n = CFArrayGetCount(windows);
    if (&_AXUIElementGetWindow != nullptr && wid != 0) {
        for (CFIndex i = 0; i < n && !target; ++i) {
            AXUIElementRef w = (AXUIElementRef)CFArrayGetValueAtIndex(windows, i);
            CGWindowID id = 0;
            if (_AXUIElementGetWindow(w, &id) == kAXErrorSuccess && id == wid) target = w;
        }
    }
    if (!target && !CGRectIsEmpty(frame)) {
        const QRect want(int(frame.origin.x), int(frame.origin.y),
                         int(frame.size.width), int(frame.size.height));
        for (CFIndex i = 0; i < n && !target; ++i) {
            AXUIElementRef w = (AXUIElementRef)CFArrayGetValueAtIndex(windows, i);
            if (FrameOfWindow(w) == want) target = w;
        }
    }
    if (target) CFRetain(target);
    CFRelease(windows);
    return target;
}

// Private SkyLight calls that front a process with ONE window and make that
// window key. yabai and AltTab use the same pair. Resolved at runtime so the
// binary still loads if SkyLight ever drops them.
using SLPSSetFrontProcessWithOptionsFn =
    CGError (*)(ProcessSerialNumber *, CGWindowID, uint32_t);
using SLPSPostEventRecordToFn = CGError (*)(ProcessSerialNumber *, uint8_t *);

constexpr uint32_t kCPSUserGenerated = 0x200;

bool FrontProcessWithWindow(pid_t pid, CGWindowID wid)
{
    static void *skylight = dlopen(
        "/System/Library/PrivateFrameworks/SkyLight.framework/SkyLight", RTLD_LAZY);
    static auto setFront = skylight
        ? (SLPSSetFrontProcessWithOptionsFn)dlsym(skylight, "_SLPSSetFrontProcessWithOptions")
        : nullptr;
    static auto postEvent = skylight
        ? (SLPSPostEventRecordToFn)dlsym(skylight, "SLPSPostEventRecordTo")
        : nullptr;
    if (!setFront || !postEvent || wid == 0) return false;

    ProcessSerialNumber psn = {0, 0};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (GetProcessForPID(pid, &psn) != noErr) return false;
#pragma clang diagnostic pop

    if (setFront(&psn, wid, kCPSUserGenerated) != kCGErrorSuccess) return false;

    // Make the window key: one event record with a "focus" byte, sent twice.
    uint8_t bytes[0xf8] = {0};
    bytes[0x04] = 0xf8;
    bytes[0x3a] = 0x10;
    memcpy(&bytes[0x3c], &wid, sizeof(wid));
    memset(&bytes[0x20], 0xff, 0x10);
    bytes[0x08] = 0x01;
    postEvent(&psn, bytes);
    bytes[0x08] = 0x02;
    postEvent(&psn, bytes);
    return true;
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

AccessibilityBridge::WindowHandle AccessibilityBridge::captureWindowAtPoint(const QPoint &screenPoint)
{
    const WindowHit hit = WindowAtPoint(CGPointMake(screenPoint.x(), screenPoint.y()));
    if (hit.pid == 0) return nullptr;
    return (WindowHandle)CopyWindowById(hit.pid, hit.wid, hit.frame);
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
        // Make the target the app's main window BEFORE activation. Activation
        // brings the app's main and key windows forward, so a stale main window
        // (for example a second window of the same app in another zone) would
        // pop up there instead.
        AXUIElementSetAttributeValue(target, kAXMainAttribute, kCFBooleanTrue);

        // Bring the process forward with only this window. Without this,
        // activateWithOptions: also raises the app's previous key window,
        // which can sit in a different zone.
        const bool fronted = FrontProcessWithWindow((pid_t)pid, cgWindowID);
        if (!fronted) {
            NSRunningApplication *appNs =
                [NSRunningApplication runningApplicationWithProcessIdentifier:(pid_t)pid];
            if (appNs) [appNs activateWithOptions:0];
        }

        // Raise again after activation, which may have reordered the app's
        // windows.
        AXUIElementPerformAction(target, kAXRaiseAction);
        AXUIElementSetAttributeValue(target, kAXFocusedAttribute, kCFBooleanTrue);
        CFRelease(target);
        ok = true;
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
