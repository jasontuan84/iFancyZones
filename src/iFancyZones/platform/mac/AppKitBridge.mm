#include "AppKitBridge.h"

#include <QPainterPath>
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

namespace {

// `path` (top-left origin) as an AppKit path in a view of height `h`
// (bottom-left origin).
NSBezierPath *BezierFromPath(const QPainterPath &path, CGFloat h)
{
    auto pt = [h](const QPainterPath::Element &e) { return NSMakePoint(e.x, h - e.y); };
    NSBezierPath *b = [NSBezierPath bezierPath];
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        switch (e.type) {
        case QPainterPath::MoveToElement:
            if (i > 0) [b closePath];
            [b moveToPoint:pt(e)];
            break;
        case QPainterPath::LineToElement:
            [b lineToPoint:pt(e)];
            break;
        case QPainterPath::CurveToElement:
            if (i + 2 < path.elementCount()) {
                [b curveToPoint:pt(path.elementAt(i + 2))
                  controlPoint1:pt(e)
                  controlPoint2:pt(path.elementAt(i + 1))];
                i += 2;
            }
            break;
        default:
            break;
        }
    }
    [b closePath];
    return b;
}

// A stretchable rounded-rectangle mask for a vibrancy view.
NSImage *RoundedMask(CGFloat radius)
{
    const CGFloat side = radius * 2 + 1;
    NSImage *mask = [NSImage imageWithSize:NSMakeSize(side, side)
                                   flipped:NO
                            drawingHandler:^BOOL(NSRect r) {
        [[NSColor blackColor] set];
        [[NSBezierPath bezierPathWithRoundedRect:r xRadius:radius yRadius:radius] fill];
        return YES;
    }];
    mask.capInsets = NSEdgeInsetsMake(radius, radius, radius, radius);
    mask.resizingMode = NSImageResizingModeStretch;
    return mask;
}

// `r` (window-local, top-left origin) in the coordinates of the view that
// hosts Qt's content view.
NSRect HostRect(NSView *host, NSView *qtView, const QRectF &r)
{
    const NSRect q = qtView.frame;
    const CGFloat y = host.isFlipped ? q.origin.y + r.y()
                                     : q.origin.y + q.size.height - r.y() - r.height();
    return NSMakeRect(q.origin.x + r.x(), y, r.width(), r.height());
}

NSView *FindBackdrop(NSView *host, NSString *identifier)
{
    for (NSView *sub in host.subviews) {
        if ([sub.identifier isEqualToString:identifier]) return sub;
    }
    return nil;
}

} // unnamed namespace

void AppKitBridge::setShapedBlurBackdrop(QWindow *qwin, const QPainterPath &outline)
{
    NSWindow *nswin = NativeWindow(qwin);
    if (!nswin) return;
    NSVisualEffectView *blur = BlurSiblingOf(nswin, NSVisualEffectMaterialPopover);
    if (!blur) return;

    const CGFloat w = blur.bounds.size.width;
    const CGFloat h = blur.bounds.size.height;
    if (w <= 0 || h <= 0) return;

    // maskImage (rather than a CALayer mask) is the documented way to shape an
    // NSVisualEffectView. It keeps the vibrancy and the window shadow correct.
    NSBezierPath *shape = BezierFromPath(outline, h);
    blur.maskImage = [NSImage imageWithSize:NSMakeSize(w, h)
                                    flipped:NO
                             drawingHandler:^BOOL(NSRect) {
        [[NSColor blackColor] set];
        [shape fill];
        return YES;
    }];
    [nswin invalidateShadow];
}

bool AppKitBridge::hasLiquidGlass()
{
    if (@available(macOS 26.0, *)) return true;
    return false;
}

void AppKitBridge::setGlassBackdrop(QWindow *qwin, const char *key,
                                    const QRectF &rect, const Glass &g)
{
    NSWindow *nswin = NativeWindow(qwin);
    if (!nswin || rect.isEmpty()) return;
    NSView *qtView = nswin.contentView;
    NSView *host = qtView.superview;
    if (!qtView || !host) return; // no frame view to share: better no glass than no UI

    NSString *identifier = [@"ifz.glass." stringByAppendingString:@(key)];
    const NSRect frame = HostRect(host, qtView, rect);
    NSView *existing = FindBackdrop(host, identifier);

    if (@available(macOS 26.0, *)) {
        NSGlassEffectView *glass = [existing isKindOfClass:[NSGlassEffectView class]]
                                       ? (NSGlassEffectView *)existing : nil;
        if (!glass) {
            glass = [[NSGlassEffectView alloc] initWithFrame:frame];
            glass.identifier = identifier;
            // A sibling BELOW Qt's view: the glass refracts the desktop and Qt
            // paints the controls on top of it.
            [host addSubview:glass positioned:NSWindowBelow relativeTo:qtView];
        }
        glass.frame = frame;
        glass.cornerRadius = g.cornerRadius;
        glass.style = g.clear ? NSGlassEffectViewStyleClear : NSGlassEffectViewStyleRegular;
        glass.tintColor = g.tint.isValid()
            ? [NSColor colorWithSRGBRed:g.tint.redF() green:g.tint.greenF()
                                   blue:g.tint.blueF() alpha:g.tint.alphaF()]
            : nil;
    } else {
        NSVisualEffectView *blur = [existing isKindOfClass:[NSVisualEffectView class]]
                                       ? (NSVisualEffectView *)existing : nil;
        if (!blur) {
            blur = [[NSVisualEffectView alloc] initWithFrame:frame];
            blur.identifier = identifier;
            blur.material = NSVisualEffectMaterialHUDWindow;
            blur.blendingMode = NSVisualEffectBlendingModeBehindWindow;
            blur.state = NSVisualEffectStateActive;
            [host addSubview:blur positioned:NSWindowBelow relativeTo:qtView];
        }
        blur.frame = frame;
        blur.maskImage = g.cornerRadius > 0 ? RoundedMask(g.cornerRadius) : nil;
    }

    nswin.opaque = NO;
    nswin.backgroundColor = NSColor.clearColor;
    [nswin invalidateShadow];
}

void AppKitBridge::useDarkAppearance(QWindow *qwin)
{
    NSWindow *nswin = NativeWindow(qwin);
    if (!nswin) return;
    nswin.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
}

void AppKitBridge::becomeAccessoryApp()
{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

} // namespace ifz
