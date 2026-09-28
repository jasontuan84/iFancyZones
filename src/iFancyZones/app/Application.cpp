#include "Application.h"

#include "app/AppSwitcher.h"
#include "app/TrayController.h"
#include "platform/mac/AccessibilityBridge.h"
#include "platform/mac/AppKitBridge.h"
#include "services/SettingsStore.h"
#include "services/WindowManager.h"
#include "ui/LayoutEditor.h"
#include "ui/PopoverTheme.h"
#include "ui/SettingsWindow.h"
#include "ui/TrayPopover.h"

#include <QDir>
#include <QStandardPaths>
#include <QUuid>

namespace ifz {

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setApplicationName(QStringLiteral("iFancyZones"));
    setApplicationVersion(QStringLiteral(IFZ_VERSION));
    setOrganizationName(QStringLiteral("Jason"));
    setOrganizationDomain(QStringLiteral("tuanquynh.com"));
    setQuitOnLastWindowClosed(false);

    // App windows sit on a dark background, so make all QLabel text white for
    // legibility. The carousel app name is painted directly (QPainter, black
    // on a white card) and is unaffected by this widget stylesheet.
    setStyleSheet(QStringLiteral("QLabel { color: #FFFFFF; }"));
}

Application::~Application() = default;

void Application::bootstrap()
{
    // Be an accessory app at runtime even if the bundle plist doesn't load.
    AppKitBridge::becomeAccessoryApp();

    m_settings = new SettingsStore(this);
    m_settings->load();

    // Every zone surface paints with theme::zoneColor(). Keep it in sync with
    // the user's setting.
    theme::setZoneColor(m_settings->settings().zoneColor);
    connect(m_settings, &SettingsStore::settingsChanged, this, [this]() {
        theme::setZoneColor(m_settings->settings().zoneColor);
    });

    m_accessibility = new AccessibilityBridge(this);

    // Surface the macOS Accessibility prompt on first launch.
    // The user must toggle it on themselves in System Settings; we then
    // re-install the CGEventTap via the WindowManager's permission watchdog.
    if (!m_accessibility->isTrusted()) {
        m_accessibility->requestTrust();
    }

    m_windowMgr = new WindowManager(m_settings, m_accessibility, this);
    m_windowMgr->start();

    // 3D app switcher (Alt+Space carousel). Independent of the zone manager.
    m_switcher = new AppSwitcher(m_settings, this);
    m_switcher->start();

    m_tray = new TrayController(this);
    m_tray->install();
    connect(m_tray, &TrayController::activated, this, &Application::showPopover);
    connect(m_tray, &TrayController::settingsRequested, this, &Application::showSettingsWindow);
    connect(m_tray, &TrayController::quitRequested, this, &QCoreApplication::quit);
}

void Application::showPopover()
{
    if (!m_popover) {
        m_popover = new TrayPopover(m_settings, m_accessibility);
        connect(m_popover.data(), &TrayPopover::settingsRequested,
                this, &Application::showSettingsWindow);
        connect(m_popover.data(), &TrayPopover::quitRequested,
                this, &QCoreApplication::quit);
        connect(m_popover.data(), &TrayPopover::editLayoutRequested,
                this, &Application::openEditorForLayout);
        connect(m_popover.data(), &TrayPopover::duplicateLayoutRequested,
                this, &Application::duplicateLayout);
        connect(m_popover.data(), &TrayPopover::deleteLayoutRequested,
                this, &Application::deleteLayout);
        connect(m_popover.data(), &TrayPopover::newLayoutRequested,
                this, &Application::openEditorForNewLayout);
        connect(m_popover.data(), &TrayPopover::templateRequested,
                this, &Application::openEditorForTemplate);
    }
    m_popover->toggleAtTray(m_tray->iconGeometry());
}

void Application::showSettingsWindow()
{
    if (!m_settingsWindow) {
        m_settingsWindow = new SettingsWindow(m_settings);
    }
    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->activateWindow();
}

void Application::openEditorForLayout(const QUuid &layoutId)
{
    Layout l = m_settings->layoutById(layoutId);
    if (l.id().isNull()) return;
    if (l.isBuiltIn()) return; // built-ins are read-only; spec §3.3 #6

    if (m_editor) {
        m_editor->close();
        m_editor->deleteLater();
        m_editor = nullptr;
    }
    m_editor = new LayoutEditor(l, /*createNew=*/false);
    connect(m_editor.data(), &LayoutEditor::saved, this, [this](const Layout &saved) {
        m_settings->upsertLayout(saved);
    });
    m_editor->open();
}

void Application::openEditorForNewLayout()
{
    if (m_editor) {
        m_editor->close();
        m_editor->deleteLater();
        m_editor = nullptr;
    }
    Layout fresh(QUuid::createUuid(), QStringLiteral("New layout"), LayoutKind::Custom);
    fresh.setGap(m_settings->settings().defaultGap);
    m_editor = new LayoutEditor(fresh, /*createNew=*/true);
    connect(m_editor.data(), &LayoutEditor::saved, this, [this](const Layout &saved) {
        m_settings->upsertLayout(saved);
    });
    m_editor->open();
}

void Application::openEditorForTemplate(const QString &name,
                                        const QVector<QRectF> &zones)
{
    if (m_editor) {
        m_editor->close();
        m_editor->deleteLater();
        m_editor = nullptr;
    }
    // Templates are starting points, so the layout is created unsaved and the
    // user confirms it in the editor like any other new layout.
    Layout fresh(QUuid::createUuid(), name, LayoutKind::Custom);
    fresh.setGap(m_settings->settings().defaultGap);
    for (const QRectF &r : zones) fresh.addZone(Zone(QUuid::createUuid(), r));
    m_editor = new LayoutEditor(fresh, /*createNew=*/true);
    connect(m_editor.data(), &LayoutEditor::saved, this, [this](const Layout &saved) {
        m_settings->upsertLayout(saved);
    });
    m_editor->open();
}

void Application::duplicateLayout(const QUuid &layoutId)
{
    Layout l = m_settings->layoutById(layoutId);
    if (l.id().isNull()) return;
    QString newName = l.name() + QStringLiteral(" copy");
    m_settings->upsertLayout(l.duplicate(newName));
}

void Application::deleteLayout(const QUuid &layoutId)
{
    m_settings->deleteLayout(layoutId);
}

} // namespace ifz
