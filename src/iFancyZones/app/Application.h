#pragma once

#include <QApplication>
#include <QPointer>
#include <QRectF>
#include <QVector>

namespace ifz {

class SettingsStore;
class WindowManager;
class AppSwitcher;
class TrayController;
class TrayPopover;
class SettingsWindow;
class LayoutEditor;
class AccessibilityBridge;

class Application : public QApplication {
    Q_OBJECT
public:
    Application(int &argc, char **argv);
    ~Application() override;

    void bootstrap();

    SettingsStore *settingsStore() const { return m_settings; }
    AccessibilityBridge *accessibility() const { return m_accessibility; }

public slots:
    void showPopover();
    void showSettingsWindow();
    void openEditorForLayout(const class QUuid &layoutId);   // edit existing
    void openEditorForNewLayout();                            // new
    // Start a new layout pre-filled from one of the Layouts-tab templates.
    void openEditorForTemplate(const QString &name, const QVector<QRectF> &zones);
    void duplicateLayout(const class QUuid &layoutId);
    void deleteLayout(const class QUuid &layoutId);

private:
    SettingsStore       *m_settings = nullptr;
    AccessibilityBridge *m_accessibility = nullptr;
    WindowManager       *m_windowMgr = nullptr;
    AppSwitcher         *m_switcher = nullptr;
    TrayController      *m_tray = nullptr;
    QPointer<TrayPopover>    m_popover;
    QPointer<SettingsWindow> m_settingsWindow;
    QPointer<LayoutEditor>   m_editor;
};

} // namespace ifz
