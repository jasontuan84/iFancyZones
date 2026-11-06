#pragma once

#include <QList>
#include <QPixmap>
#include <QString>

namespace ifz {

// One entry in the app-switcher list: a running, user-facing application.
struct RunningAppInfo {
    qint64  pid = 0;
    QString name;
    QString bundleId;
    QPixmap icon;             // app icon, pre-rendered at the requested size
    bool    active = false;   // true if this is the currently frontmost app
};

// Pure-C++ facade over NSWorkspace / NSRunningApplication for the 3D app
// switcher. All Cocoa types stay inside the .mm.
class AppSwitcherBridge {
public:
    // Snapshot of "regular" (Dock-visible) running apps, most-recently-active
    // first. iFancyZones itself is excluded. `iconPx` is the logical pixel
    // size each icon is rendered at (scaled by the main screen's device pixel
    // ratio for crispness on Retina).
    static QList<RunningAppInfo> runningApps(int iconPx = 160);

    // The CGWindowID of the app's topmost on-screen normal window, or 0 when
    // the app has no window on the current space (hidden, minimized, or on
    // another space). The topmost window is the one the user used last.
    static quint32 frontmostWindowOf(qint64 pid);

    // Unhide and activate the app owning `pid`. Activation brings forward
    // only the app's main and key windows, not every window. Use this when
    // frontmostWindowOf() finds no window. Returns true on success.
    static bool activateApp(qint64 pid);
};

} // namespace ifz
