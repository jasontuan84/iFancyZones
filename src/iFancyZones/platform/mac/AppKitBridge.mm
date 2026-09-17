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

void AppKitBridge::addBlurBackdrop(QWindow *qwin)
{
    NSWindow *nswin = NativeWindow(qwin);
    if (!nswin) return;
    NSView *content = nswin.contentView;
    if (!content) return;

    // Avoid inserting twice.
    for (NSView *sub in content.subviews) {
        if ([sub isKindOfClass:[NSVisualEffectView class]]) return;
    }

    NSVisualEffectView *blur =
        [[NSVisualEffectView alloc] initWithFrame:content.bounds];
    blur.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    blur.material = NSVisualEffectMaterialHUDWindow;
    blur.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    blur.state = NSVisualEffectStateActive;
    [content addSubview:blur positioned:NSWindowBelow relativeTo:nil];

    nswin.opaque = NO;
    nswin.backgroundColor = NSColor.clearColor;
}

void AppKitBridge::becomeAccessoryApp()
{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

} // namespace ifz
