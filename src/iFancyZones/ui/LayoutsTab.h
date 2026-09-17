#pragma once

#include "core/Layout.h"

#include <QHash>
#include <QUuid>
#include <QWidget>

class QLineEdit;
class QVBoxLayout;

namespace ifz {

class SectionHeader;
class SettingsStore;

// "Layouts" tab: a search field, the template gallery, then every layout the
// user owns, with the one bound to a display called out as active.
class LayoutsTab : public QWidget {
    Q_OBJECT
public:
    explicit LayoutsTab(SettingsStore *store, QWidget *parent = nullptr);

    void reload();

signals:
    void newLayoutRequested();
    void editLayoutRequested(const QUuid &layoutId);
    void duplicateLayoutRequested(const QUuid &layoutId);
    void deleteLayoutRequested(const QUuid &layoutId);
    // A template card was clicked: start a new layout pre-filled with `zones`.
    void templateRequested(const QString &name, const QVector<QRectF> &zones);

private:
    void buildTemplates(QWidget *host, QVBoxLayout *col);
    void rebuildRows();
    // Display names each layout is currently bound to, for the "in use on" line.
    QHash<QUuid, QString> boundDisplayNames() const;

    SettingsStore *m_store;
    QLineEdit *m_search = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    SectionHeader *m_myHeader = nullptr;
    QString m_filter;
};

} // namespace ifz
