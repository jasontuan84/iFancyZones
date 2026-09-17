#pragma once

#include <QFrame>
#include <QPointer>
#include <QUuid>

class QTabWidget;

namespace ifz {

class AccessibilityBridge;
class SettingsStore;
class ScreenLayoutTab;
class LayoutsTab;

// Frameless popover anchored to the menu-bar icon.
class TrayPopover : public QFrame {
    Q_OBJECT
public:
    explicit TrayPopover(SettingsStore *store,
                         AccessibilityBridge *ax,
                         QWidget *parent = nullptr);

    // `trayIconGeometry` is in global screen coords (top-left).
    void showAtTray(const QRect &trayIconGeometry);

signals:
    void settingsRequested();
    void quitRequested();
    void newLayoutRequested();
    void editLayoutRequested(const QUuid &layoutId);
    void duplicateLayoutRequested(const QUuid &layoutId);
    void deleteLayoutRequested(const QUuid &layoutId);

protected:
    void focusOutEvent(QFocusEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private:
    SettingsStore *m_store;
    AccessibilityBridge *m_ax;
    QTabWidget *m_tabs = nullptr;
    QPointer<ScreenLayoutTab> m_screenTab;
    QPointer<LayoutsTab> m_layoutsTab;
};

} // namespace ifz
