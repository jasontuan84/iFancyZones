#pragma once

#include "AppSettings.h"
#include "core/Layout.h"
#include "core/Monitor.h"

#include <QList>

#include <QHash>
#include <QObject>
#include <QString>
#include <QUuid>
#include <QVector>

namespace ifz {

// Single source of truth for in-memory app state. Loads/saves
// ~/Library/Application Support/iFancyZones/data.json atomically.
class SettingsStore : public QObject {
    Q_OBJECT
public:
    explicit SettingsStore(QObject *parent = nullptr);

    void load();          // call once on app start
    void save();          // call after every mutation; debounced internally

    const AppSettings &settings() const { return m_settings; }
    void setSettings(const AppSettings &s);

    const QVector<Layout> &layouts() const { return m_layouts; }

    Layout layoutById(const QUuid &id) const;
    void upsertLayout(const Layout &l);
    void deleteLayout(const QUuid &id);

    // bindings: Monitor::key -> layoutId. layoutId.isNull() == "None".
    QUuid bindingFor(const QString &displayKey) const;
    void setBinding(const QString &displayKey, const QUuid &layoutId);

    // Files from 1.7.0 and earlier keyed bindings by CGDisplayUnitNumber,
    // which is not stable between sessions. Carry those over to the
    // persistent key of the displays connected now, then drop them.
    // Call once after load() with MacScreenInfo::currentMonitors().
    void migrateLegacyBindings(const QList<Monitor> &monitors);

signals:
    void layoutsChanged();
    void bindingsChanged();
    void settingsChanged();

private:
    QString filePath() const;
    void ensureBuiltIns();
    void scheduleSave();

    AppSettings m_settings;
    QVector<Layout> m_layouts;
    QHash<QString, QUuid> m_bindings;
    QHash<quint32, QUuid> m_legacyBindings; // read from an old file, see migrateLegacyBindings()

    bool m_loaded = false;
    bool m_savePending = false;
};

} // namespace ifz
