#include "SettingsStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>

namespace ifz {

namespace {

Layout makeFull(const QString &name)
{
    Layout l(QUuid::createUuid(), name, LayoutKind::BuiltIn);
    l.addZone(Zone(QUuid::createUuid(), QRectF(0.0, 0.0, 1.0, 1.0)));
    return l;
}

Layout makeTwoColumns(const QString &name)
{
    Layout l(QUuid::createUuid(), name, LayoutKind::BuiltIn);
    l.addZone(Zone(QUuid::createUuid(), QRectF(0.0, 0.0, 0.5, 1.0)));
    l.addZone(Zone(QUuid::createUuid(), QRectF(0.5, 0.0, 0.5, 1.0)));
    return l;
}

Layout makeThreeColumns(const QString &name)
{
    Layout l(QUuid::createUuid(), name, LayoutKind::BuiltIn);
    const double third = 1.0 / 3.0;
    l.addZone(Zone(QUuid::createUuid(), QRectF(0.0,       0.0, third, 1.0)));
    l.addZone(Zone(QUuid::createUuid(), QRectF(third,     0.0, third, 1.0)));
    l.addZone(Zone(QUuid::createUuid(), QRectF(2 * third, 0.0, third, 1.0)));
    return l;
}

Layout makeQuadrants(const QString &name)
{
    Layout l(QUuid::createUuid(), name, LayoutKind::BuiltIn);
    l.addZone(Zone(QUuid::createUuid(), QRectF(0.0, 0.0, 0.5, 0.5)));
    l.addZone(Zone(QUuid::createUuid(), QRectF(0.5, 0.0, 0.5, 0.5)));
    l.addZone(Zone(QUuid::createUuid(), QRectF(0.0, 0.5, 0.5, 0.5)));
    l.addZone(Zone(QUuid::createUuid(), QRectF(0.5, 0.5, 0.5, 0.5)));
    return l;
}

} // unnamed namespace

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
{}

QString SettingsStore::filePath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/data.json");
}

void SettingsStore::load()
{
    QFile f(filePath());
    if (!f.exists()) {
        ensureBuiltIns();
        m_loaded = true;
        save();
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        ensureBuiltIns();
        m_loaded = true;
        return;
    }
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        ensureBuiltIns();
        m_loaded = true;
        save();
        return;
    }

    QJsonObject root = doc.object();
    m_settings = AppSettings::fromJson(root.value("settings").toObject());

    m_layouts.clear();
    const QJsonArray arr = root.value("layouts").toArray();
    for (const QJsonValue &v : arr) {
        m_layouts.append(Layout::fromJson(v.toObject()));
    }
    ensureBuiltIns();

    m_bindings.clear();
    m_legacyBindings.clear();
    const QJsonArray bind = root.value("bindings").toArray();
    for (const QJsonValue &v : bind) {
        QJsonObject o = v.toObject();
        QString sid = o.value("layout-id").toString();
        QUuid id = sid.isEmpty() ? QUuid() : QUuid::fromString(sid);
        if (id.isNull()) continue;
        const QString key = o.value("display-key").toString();
        if (!key.isEmpty()) {
            m_bindings.insert(key, id);
            continue;
        }
        // Old format: the key was a CGDisplayUnitNumber saved as an int
        // (negative when the namespace bit was set).
        const quint32 unit = static_cast<quint32>(o.value("display-unit-number").toInt(0));
        if (unit != 0) m_legacyBindings.insert(unit, id);
    }
    m_loaded = true;
}

void SettingsStore::ensureBuiltIns()
{
    bool hasBuiltIn = false;
    for (const Layout &l : m_layouts) {
        if (l.isBuiltIn()) { hasBuiltIn = true; break; }
    }
    if (hasBuiltIn) return;

    m_layouts.append(makeFull(QStringLiteral("Single")));
    m_layouts.append(makeTwoColumns(QStringLiteral("Two columns")));
    m_layouts.append(makeThreeColumns(QStringLiteral("Three columns")));
    m_layouts.append(makeQuadrants(QStringLiteral("Quadrants")));
}

void SettingsStore::save()
{
    QJsonObject root;
    root["schema-version"] = 2;
    root["settings"] = m_settings.toJson();

    QJsonArray arr;
    for (const Layout &l : m_layouts) arr.append(l.toJson());
    root["layouts"] = arr;

    QJsonArray bind;
    for (auto it = m_bindings.constBegin(); it != m_bindings.constEnd(); ++it) {
        QJsonObject o;
        o["display-key"] = it.key();
        o["layout-id"] = it.value().isNull() ? QString() : it.value().toString(QUuid::WithoutBraces);
        bind.append(o);
    }
    root["bindings"] = bind;

    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    m_savePending = false;
}

void SettingsStore::scheduleSave()
{
    if (!m_loaded) return;
    if (m_savePending) return;
    m_savePending = true;
    QTimer::singleShot(150, this, [this]() { save(); });
}

void SettingsStore::setSettings(const AppSettings &s)
{
    m_settings = s;
    emit settingsChanged();
    scheduleSave();
}

Layout SettingsStore::layoutById(const QUuid &id) const
{
    for (const Layout &l : m_layouts) {
        if (l.id() == id) return l;
    }
    return Layout();
}

void SettingsStore::upsertLayout(const Layout &l)
{
    for (int i = 0; i < m_layouts.size(); ++i) {
        if (m_layouts[i].id() == l.id()) {
            m_layouts[i] = l;
            emit layoutsChanged();
            scheduleSave();
            return;
        }
    }
    m_layouts.append(l);
    emit layoutsChanged();
    scheduleSave();
}

void SettingsStore::deleteLayout(const QUuid &id)
{
    for (int i = 0; i < m_layouts.size(); ++i) {
        if (m_layouts[i].id() == id) {
            if (m_layouts[i].isBuiltIn()) return; // protect built-ins
            m_layouts.removeAt(i);
            // Remove any binding that pointed to this layout.
            for (auto it = m_bindings.begin(); it != m_bindings.end(); ) {
                if (it.value() == id) it = m_bindings.erase(it);
                else ++it;
            }
            emit layoutsChanged();
            emit bindingsChanged();
            scheduleSave();
            return;
        }
    }
}

QUuid SettingsStore::bindingFor(const QString &displayKey) const
{
    if (displayKey.isEmpty()) return QUuid();
    return m_bindings.value(displayKey, QUuid());
}

void SettingsStore::setBinding(const QString &displayKey, const QUuid &layoutId)
{
    if (displayKey.isEmpty()) return;
    if (layoutId.isNull()) {
        m_bindings.remove(displayKey);
    } else {
        m_bindings.insert(displayKey, layoutId);
    }
    emit bindingsChanged();
    scheduleSave();
}

void SettingsStore::migrateLegacyBindings(const QList<Monitor> &monitors)
{
    if (m_legacyBindings.isEmpty()) return;
    for (const Monitor &m : monitors) {
        if (m.key.isEmpty() || m_bindings.contains(m.key)) continue;
        const QUuid id = m_legacyBindings.value(m.legacyKey());
        if (!id.isNull() && !layoutById(id).id().isNull()) m_bindings.insert(m.key, id);
    }
    // Unit numbers of displays not connected now mean nothing in a later
    // session, so they are not kept.
    m_legacyBindings.clear();
    emit bindingsChanged();
    scheduleSave();
}

} // namespace ifz
