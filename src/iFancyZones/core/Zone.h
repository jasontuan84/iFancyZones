#pragma once

#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QUuid>

namespace ifz {

class Zone {
public:
    Zone();
    Zone(const QUuid &id, const QRectF &normalized);

    QUuid id() const { return m_id; }

    QRectF normalized() const { return m_normalized; }
    void setNormalized(const QRectF &r) { m_normalized = clampUnit(r); }

    QJsonObject toJson() const;
    static Zone fromJson(const QJsonObject &obj);

private:
    static QRectF clampUnit(QRectF r);

    QUuid m_id;
    QRectF m_normalized;
};

} // namespace ifz
