#pragma once

#include <QtCore/QtGlobal>

class QWindow;

namespace ifz {

// Helpers that need AppKit to configure native windows the way the spec
// requires (e.g. overlay at NSScreenSaverWindowLevel + can-join-all-spaces).
class AppKitBridge {
public:
    // Make the underlying NSWindow of `qwin` behave as an overlay:
    //  - level = NSScreenSaverWindowLevel (above full-screen apps)
    //  - collectionBehavior: join all spaces, stationary, full-screen aux
    //  - ignoresMouseEvents = true (click-through)
    // Call AFTER `qwin->create()` (winId() has been forced).
    static void configureAsOverlay(QWindow *qwin);

    // As above but interactive (does NOT ignore mouse events).
    // Used by the layout editor overlay.
    static void configureAsEditor(QWindow *qwin);

    // Configure `qwin` as a menu-bar popover: floats above ordinary windows,
    // follows the user onto other spaces and full-screen apps, and can take
    // keyboard focus. Call AFTER the platform window exists.
    static void configureAsPopover(QWindow *qwin);

    // Bring this (accessory) app forward so a just-shown window actually
    // becomes key. Without this an LSUIElement app never takes focus.
    static void activateApp();

    // Insert a behind-window NSVisualEffectView under the Qt content so the
    // window gets a frosted-glass blur of whatever is behind it. Call after
    // the platform window exists. Safe to call once per window.
    static void addBlurBackdrop(QWindow *qwin);

    // Menu-bar popover vibrancy, masked to the panel outline: a rounded
    // rectangle plus the beak that points at the status item. All values are
    // Qt logical points with a top-left origin; `beakCenterX` is the beak tip
    // measured from the window's left edge. Safe to call on every show, which
    // is required since the beak moves with the icon.
    static void addPopoverBlur(QWindow *qwin,
                               double cornerRadius,
                               double beakHeight,
                               double beakWidth,
                               double beakCenterX);

    // Switch the running app's activation policy to "accessory" — the
    // LSUIElement-equivalent at runtime. Keeps Cmd-Tab clean even if
    // a window was already shown.
    static void becomeAccessoryApp();
};

} // namespace ifz
