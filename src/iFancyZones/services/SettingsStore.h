#pragma once

#include "AppSettings.h"
#include "core/Layout.h"

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

    // bindings: displayUnitNumber -> layoutId. layoutId.isNull() == "None".
    QUuid bindingFor(quint32 displayUnitNumber) const;
    void setBinding(quint32 displayUnitNumber, const QUuid &layoutId);
    const QHash<quint32, QUuid> &allBindings() const { return m_bindings; }

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
    QHash<quint32, QUuid> m_bindings;

    bool m_loaded = false;
    bool m_savePending = false;
};

} // namespace ifz
