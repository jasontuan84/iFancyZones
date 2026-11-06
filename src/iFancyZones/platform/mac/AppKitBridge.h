#pragma once

#include <QColor>
#include <QRectF>

class QPainterPath;
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

    // Vibrancy behind the whole window, masked to `outline`. The outline is
    // in window-local logical points with a top-left origin, the same path
    // the Qt side paints, so the blur and the painted edge always coincide.
    // Safe to call on every show.
    static void setShapedBlurBackdrop(QWindow *qwin, const QPainterPath &outline);

    // True when the system renders Liquid Glass (macOS 26 or later).
    static bool hasLiquidGlass();

    // How one glass surface looks.
    struct Glass {
        double cornerRadius = 0;
        QColor tint;          // invalid: no tint
        bool clear = false;   // the more transparent "clear" glass style
    };

    // Put a glass surface behind the Qt content at `rect` (window-local
    // logical points, top-left origin). `key` names the surface: a second
    // call with the same key moves and restyles it. Before macOS 26 the
    // surface is a rounded vibrancy view instead. Call after show().
    static void setGlassBackdrop(QWindow *qwin, const char *key,
                                 const QRectF &rect, const Glass &glass);

    // Render the window in Dark Mode whatever the system setting. The app
    // paints light text on its surfaces, so glass and vibrancy must stay dark.
    static void useDarkAppearance(QWindow *qwin);

    // Switch the running app's activation policy to "accessory" — the
    // LSUIElement-equivalent at runtime. Keeps Cmd-Tab clean even if
    // a window was already shown.
    static void becomeAccessoryApp();
};

} // namespace ifz
