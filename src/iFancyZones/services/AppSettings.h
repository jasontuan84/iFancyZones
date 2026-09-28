#pragma once

#include <QColor>
#include <QJsonObject>
#include <QStringList>

namespace ifz {

// Per the activation-key picker in product spec §2.7.
// "Space" is the default; users may pick a single modifier (Shift/Ctrl/Opt/Cmd)
// or a regular key. nativeKeyCode is the macOS virtual keycode (kVK_*).
struct AppSettings {
    int activationKeyCode = 49;        // kVK_Space
    QString activationKeyLabel = QStringLiteral("Space");

    // Cycle focused window through other windows in the same zone.
    // Carbon modifier mask: cmdKey=1<<8, shiftKey=1<<9, optionKey=1<<11, controlKey=1<<12.
    // Defaults to Option+Tab (optionKey | kVK_Tab=48).
    int cycleFocusKeyCode = 48;             // kVK_Tab
    int cycleFocusModifiers = 1 << 11;      // optionKey
    QString cycleFocusLabel = QStringLiteral("Opt+Tab");

    // 3D app-switcher carousel. Hold the modifier, tap the base key to open;
    // arrow keys move the selection; releasing the modifier raises the picked
    // app. Defaults to Option+Space (optionKey | kVK_Space=49).
    bool switcherEnabled = true;
    int switcherKeyCode = 49;               // kVK_Space
    int switcherModifiers = 1 << 11;        // optionKey
    QString switcherLabel = QStringLiteral("Opt+Space");

    bool launchAtLogin = false;
    int defaultGap = 0;
    bool showZoneNumbers = true;
    bool restoreSizeOnUnsnap = true;

    // Color of the zones in the snap preview and the layout editor, and of
    // the screen-number badges. Stored as "#RRGGBB"; each surface applies
    // its own alpha.
    static QColor defaultZoneColor() { return QColor(60, 200, 120); }
    QColor zoneColor = defaultZoneColor();
    QStringList excludedBundles;

    QJsonObject toJson() const;
    static AppSettings fromJson(const QJsonObject &obj);
};

} // namespace ifz
