#pragma once

#include "Zone.h"

#include <QJsonObject>
#include <QString>
#include <QUuid>
#include <QVector>

namespace ifz {

enum class LayoutKind { BuiltIn, Custom };

class Layout {
public:
    Layout();
    Layout(const QUuid &id, const QString &name, LayoutKind kind);

    QUuid id() const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    LayoutKind kind() const { return m_kind; }
    void setKind(LayoutKind k) { m_kind = k; }

    int gap() const { return m_gap; }
    void setGap(int g) { m_gap = g < 0 ? 0 : g; }

    const QVector<Zone> &zones() const { return m_zones; }
    QVector<Zone> &zonesMutable() { return m_zones; }

    void addZone(const Zone &z) { m_zones.append(z); }
    void removeZoneAt(int i) { if (i >= 0 && i < m_zones.size()) m_zones.removeAt(i); }
    void clearZones() { m_zones.clear(); }

    bool isBuiltIn() const { return m_kind == LayoutKind::BuiltIn; }

    QJsonObject toJson() const;
    static Layout fromJson(const QJsonObject &obj);

    Layout duplicate(const QString &newName) const;

private:
    QUuid m_id;
    QString m_name;
    LayoutKind m_kind = LayoutKind::Custom;
    int m_gap = 0;
    QVector<Zone> m_zones;
};

} // namespace ifz
