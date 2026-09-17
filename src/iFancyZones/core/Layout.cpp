#include "Layout.h"

#include <QJsonArray>

namespace ifz {

Layout::Layout()
    : m_id(QUuid::createUuid())
    , m_name(QStringLiteral("Untitled"))
{}

Layout::Layout(const QUuid &id, const QString &name, LayoutKind kind)
    : m_id(id.isNull() ? QUuid::createUuid() : id)
    , m_name(name)
    , m_kind(kind)
{}

QJsonObject Layout::toJson() const
{
    QJsonObject o;
    o["id"] = m_id.toString(QUuid::WithoutBraces);
    o["name"] = m_name;
    o["kind"] = m_kind == LayoutKind::BuiltIn ? "built-in" : "custom";
    o["gap"] = m_gap;

    QJsonArray arr;
    for (const Zone &z : m_zones) {
        arr.append(z.toJson());
    }
    o["zones"] = arr;
    return o;
}

Layout Layout::fromJson(const QJsonObject &obj)
{
    Layout l;
    l.m_id = QUuid::fromString(obj.value("id").toString());
    if (l.m_id.isNull()) {
        l.m_id = QUuid::createUuid();
    }
    l.m_name = obj.value("name").toString(QStringLiteral("Untitled"));
    l.m_kind = obj.value("kind").toString() == QStringLiteral("built-in")
                   ? LayoutKind::BuiltIn
                   : LayoutKind::Custom;
    l.m_gap = obj.value("gap").toInt(0);

    const QJsonArray arr = obj.value("zones").toArray();
    for (const QJsonValue &v : arr) {
        l.m_zones.append(Zone::fromJson(v.toObject()));
    }
    return l;
}

Layout Layout::duplicate(const QString &newName) const
{
    Layout copy(QUuid::createUuid(), newName, LayoutKind::Custom);
    copy.m_gap = m_gap;
    copy.m_zones.reserve(m_zones.size());
    for (const Zone &z : m_zones) {
        copy.m_zones.append(Zone(QUuid::createUuid(), z.normalized()));
    }
    return copy;
}

} // namespace ifz
