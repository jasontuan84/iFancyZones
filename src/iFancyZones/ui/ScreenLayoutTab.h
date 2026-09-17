#pragma once

#include <QPointer>
#include <QUuid>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QVBoxLayout;

namespace ifz {

class AccessibilityBridge;
class SettingsStore;

class ScreenLayoutTab : public QWidget {
    Q_OBJECT
public:
    explicit ScreenLayoutTab(SettingsStore *store, QWidget *parent = nullptr);

    // The popover injects this so the tab can show an AX-permission banner.
    void setAccessibilityBridge(AccessibilityBridge *ax);

    // Re-read screens + layouts and rebuild rows.
    void reload();

signals:
    void settingsRequested();

private:
    struct Row {
        quint32 unitNumber = 0;
        QString name;
        QPointer<QComboBox> combo;
    };

    void rebuildRows();
    void onComboChanged(int index, quint32 unitNumber);

    SettingsStore *m_store;
    AccessibilityBridge *m_ax = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QLabel *m_axBanner = nullptr;
    QVector<Row> m_rows;
};

} // namespace ifz
