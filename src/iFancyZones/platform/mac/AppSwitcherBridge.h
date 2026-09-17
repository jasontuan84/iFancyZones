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

    // Bring the app owning `pid` to the front (raise + activate all windows).
    // Returns true on success.
    static bool activateApp(qint64 pid);
};

} // namespace ifz
