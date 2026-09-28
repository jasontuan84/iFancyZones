#include "AppSettings.h"

#include <QJsonArray>

namespace ifz {

QJsonObject AppSettings::toJson() const
{
    QJsonObject o;
    o["activation-key-code"] = activationKeyCode;
    o["activation-key-label"] = activationKeyLabel;
    o["cycle-focus-key-code"] = cycleFocusKeyCode;
    o["cycle-focus-modifiers"] = cycleFocusModifiers;
    o["cycle-focus-label"] = cycleFocusLabel;
    o["switcher-enabled"] = switcherEnabled;
    o["switcher-key-code"] = switcherKeyCode;
    o["switcher-modifiers"] = switcherModifiers;
    o["switcher-label"] = switcherLabel;
    o["launch-at-login"] = launchAtLogin;
    o["default-gap"] = defaultGap;
    o["show-zone-numbers"] = showZoneNumbers;
    o["restore-size-on-unsnap"] = restoreSizeOnUnsnap;
    o["zone-color"] = zoneColor.name(QColor::HexRgb);
    QJsonArray bundles;
    for (const QString &b : excludedBundles) bundles.append(b);
    o["excluded-bundles"] = bundles;
    return o;
}

AppSettings AppSettings::fromJson(const QJsonObject &obj)
{
    AppSettings s;
    s.activationKeyCode = obj.value("activation-key-code").toInt(49);
    s.activationKeyLabel = obj.value("activation-key-label").toString(QStringLiteral("Space"));
    s.cycleFocusKeyCode  = obj.value("cycle-focus-key-code").toInt(48);
    s.cycleFocusModifiers = obj.value("cycle-focus-modifiers").toInt(1 << 11);
    s.cycleFocusLabel    = obj.value("cycle-focus-label").toString(QStringLiteral("Opt+Tab"));
    s.switcherEnabled    = obj.value("switcher-enabled").toBool(true);
    s.switcherKeyCode    = obj.value("switcher-key-code").toInt(49);
    s.switcherModifiers  = obj.value("switcher-modifiers").toInt(1 << 11);
    s.switcherLabel      = obj.value("switcher-label").toString(QStringLiteral("Opt+Space"));
    s.launchAtLogin = obj.value("launch-at-login").toBool(false);
    s.defaultGap = obj.value("default-gap").toInt(0);
    s.showZoneNumbers = obj.value("show-zone-numbers").toBool(true);
    s.restoreSizeOnUnsnap = obj.value("restore-size-on-unsnap").toBool(true);
    const QColor zc = QColor::fromString(obj.value("zone-color").toString());
    s.zoneColor = zc.isValid() ? zc : defaultZoneColor();
    const QJsonArray arr = obj.value("excluded-bundles").toArray();
    for (const QJsonValue &v : arr) s.excludedBundles.append(v.toString());
    return s;
}

} // namespace ifz
