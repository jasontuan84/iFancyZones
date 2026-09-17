#pragma once

#include <QUuid>
#include <QWidget>

class QVBoxLayout;

namespace ifz {

class SettingsStore;

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

private:
    void rebuildRows();

    SettingsStore *m_store;
    QVBoxLayout *m_rowsLayout = nullptr;
};

} // namespace ifz
