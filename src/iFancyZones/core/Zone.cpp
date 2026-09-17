#include "Zone.h"

#include <algorithm>

namespace ifz {

Zone::Zone()
    : m_id(QUuid::createUuid())
    , m_normalized(0.0, 0.0, 1.0, 1.0)
{}

Zone::Zone(const QUuid &id, const QRectF &normalized)
    : m_id(id.isNull() ? QUuid::createUuid() : id)
    , m_normalized(clampUnit(normalized))
{}

QRectF Zone::clampUnit(QRectF r)
{
    double x = std::clamp(r.x(),      0.0, 1.0);
    double y = std::clamp(r.y(),      0.0, 1.0);
    double w = std::clamp(r.width(),  0.0, 1.0 - x);
    double h = std::clamp(r.height(), 0.0, 1.0 - y);
    return QRectF(x, y, w, h);
}

QJsonObject Zone::toJson() const
{
    QJsonObject o;
    o["id"] = m_id.toString(QUuid::WithoutBraces);
    o["x"] = m_normalized.x();
    o["y"] = m_normalized.y();
    o["w"] = m_normalized.width();
    o["h"] = m_normalized.height();
    return o;
}

Zone Zone::fromJson(const QJsonObject &obj)
{
    QUuid id = QUuid::fromString(obj.value("id").toString());
    QRectF r(obj.value("x").toDouble(),
             obj.value("y").toDouble(),
             obj.value("w").toDouble(1.0),
             obj.value("h").toDouble(1.0));
    return Zone(id, r);
}

} // namespace ifz
