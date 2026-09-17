#include "AppKitBridge.h"

#include <QWindow>

#import <AppKit/AppKit.h>

namespace ifz {

namespace {

NSWindow *NativeWindow(QWindow *qwin)
{
    if (!qwin) return nil;
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(qwin->winId());
    if (!view) return nil;
    return view.window;
}

} // unnamed namespace

void AppKitBridge::configureAsOverlay(QWindow *qwin)
{
    NSWindow *nswin = NativeWindow(qwin);
    if (!nswin) return;

    nswin.level = NSScreenSaverWindowLevel;
    nswin.collectionBehavior =
        NSWindowCollectionBehaviorCanJoinAllSpaces |
        NSWindowCollectionBehaviorStationary       |
        NSWindowCollectionBehaviorFullScreenAuxiliary |
        NSWindowCollectionBehaviorIgnoresCycle;
    nswin.ignoresMouseEvents = YES;
    nswin.opaque = NO;
    nswin.backgroundColor = NSColor.clearColor;
    nswin.hasShadow = NO;
}

void AppKitBridge::configureAsEditor(QWindow *qwin)
{
    NSWindow *nswin = NativeWindow(qwin);
    if (!nswin) return;

    nswin.level = NSPopUpMenuWindowLevel;
    nswin.collectionBehavior =
        NSWindowCollectionBehaviorCanJoinAllSpaces |
        NSWindowCollectionBehaviorFullScreenAuxiliary |
        NSWindowCollectionBehaviorIgnoresCycle;
    nswin.ignoresMouseEvents = NO;
    nswin.opaque = NO;
    nswin.backgroundColor = NSColor.clearColor;
    nswin.hasShadow = NO;

    // Lock the editor backdrop: not movable, not resizable. Users can only
    // drag/resize zones inside, never the whole canvas.
    nswin.movable = NO;
    nswin.movableByWindowBackground = NO;
    nswin.styleMask &= ~NSWindowStyleMaskResizable;
}

void AppKitBridge::configureAsPopover(QWindow *qwin)
{
    NSWindow *nswin = NativeWindow(qwin);
    if (!nswin) return;

    // Above normal and floating windows, below the menu bar's own menus.
    nswin.level = NSStatusWindowLevel;
    nswin.collectionBehavior =
        NSWindowCollectionBehaviorCanJoinAllSpaces |
        NSWindowCollectionBehaviorFullScreenAuxiliary |
        NSWindowCollectionBehaviorIgnoresCycle;
    nswin.ignoresMouseEvents = NO;
    nswin.opaque = NO;
    nswin.backgroundColor = NSColor.clearColor;
    nswin.hasShadow = YES;
    nswin.movable = NO;
    nswin.movableByWindowBackground = NO;

    // We dismiss the popover ourselves (see TrayPopover::event) so that the
    // stray mouse-up trailing the status-item click cannot close it.
    nswin.hidesOnDeactivate = NO;
}

void AppKitBridge::activateApp()
{
    if (@available(macOS 14.0, *)) {
        [NSApp activate];
    } else {
        [NSApp activateIgnoringOtherApps:YES];
    }
}

namespace {

// Find (or build) the vibrancy view for `nswin` and place it BEHIND the Qt
// content. It has to be a sibling of Qt's NSView, not a child of it: a
// subview is always composited on top of whatever its superview draws into
// its own layer, so parenting the blur under the Qt view hides the whole UI.
NSVisualEffectView *BlurSiblingOf(NSWindow *nswin, NSVisualEffectMaterial material)
{
    NSView *qtView = nswin.contentView;
    if (!qtView) return nil;
    NSView *host = qtView.superview;
    if (!host) return nil; // no frame view to share: better no blur than no UI

    for (NSView *sub in host.subviews) {
        if ([sub isKindOfClass:[NSVisualEffectView class]]) {
            NSVisualEffectView *existing = (NSVisualEffectView *)sub;
            existing.frame = qtView.frame;
            return existing;
        }
    }

    NSVisualEffectView *blur =
        [[NSVisualEffectView alloc] initWithFrame:qtView.frame];
    blur.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    blur.material = material;
    blur.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    blur.state = NSVisualEffectStateActive;
    [host addSubview:blur positioned:NSWindowBelow relativeTo:qtView];

    nswin.opaque = NO;
    nswin.backgroundColor = NSColor.clearColor;
    return blur;
}

} // unnamed namespace

void AppKitBridge::addBlurBackdrop(QWindow *qwin)
{
    NSWindow *nswin = NativeWindow(qwin);
    if (!nswin) return;
    (void)BlurSiblingOf(nswin, NSVisualEffectMaterialHUDWindow);
}

void AppKitBridge::addPopoverBlur(QWindow *qwin,
                                  double cornerRadius,
                                  double beakHeight,
                                  double beakWidth,
                                  double beakCenterX)
{
    NSWindow *nswin = NativeWindow(qwin);
    if (!nswin) return;
    NSVisualEffectView *blur = BlurSiblingOf(nswin, NSVisualEffectMaterialPopover);
    if (!blur) return;

    const CGFloat w = blur.bounds.size.width;
    const CGFloat h = blur.bounds.size.height;
    if (w <= 0 || h <= 0) return;

    // AppKit's origin is bottom-left, so the body's top edge sits at h - beak.
    const CGFloat bodyTop = h - beakHeight;
    const CGFloat cx = beakCenterX;
    const CGFloat halfBeak = beakWidth / 2.0;

    // maskImage (rather than a CALayer mask) is the documented way to shape an
    // NSVisualEffectView; it keeps the vibrancy and the window shadow correct.
    NSImage *mask = [NSImage imageWithSize:NSMakeSize(w, h)
                                   flipped:NO
                            drawingHandler:^BOOL(NSRect) {
        NSBezierPath *path =
            [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(0, 0, w, bodyTop)
                                            xRadius:cornerRadius
                                            yRadius:cornerRadius];
        NSBezierPath *beak = [NSBezierPath bezierPath];
        [beak moveToPoint:NSMakePoint(cx - halfBeak, bodyTop - 1.0)];
        [beak curveToPoint:NSMakePoint(cx, h)
             controlPoint1:NSMakePoint(cx - halfBeak * 0.35, h - beakHeight * 0.15)
             controlPoint2:NSMakePoint(cx - halfBeak * 0.18, h)];
        [beak curveToPoint:NSMakePoint(cx + halfBeak, bodyTop - 1.0)
             controlPoint1:NSMakePoint(cx + halfBeak * 0.18, h)
             controlPoint2:NSMakePoint(cx + halfBeak * 0.35, h - beakHeight * 0.15)];
        [beak closePath];
        [path appendBezierPath:beak];
        [path setWindingRule:NSWindingRuleNonZero];
        [[NSColor blackColor] set];
        [path fill];
        return YES;
    }];
    blur.maskImage = mask;

    [nswin invalidateShadow];
}

void AppKitBridge::becomeAccessoryApp()
{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

} // namespace ifz
