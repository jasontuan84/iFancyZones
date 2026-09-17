#include "AppSwitcher.h"

#include "platform/mac/AppSwitcherBridge.h"
#include "platform/mac/HotkeyManager.h"
#include "platform/mac/SwitcherInput.h"
#include "services/SettingsStore.h"
#include "ui/CarouselOverlay.h"

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>

namespace ifz {

AppSwitcher::AppSwitcher(SettingsStore *store, QObject *parent)
    : QObject(parent), m_store(store)
{
    m_hotkey = new HotkeyManager(this);
    connect(m_hotkey, &HotkeyManager::triggered, this, &AppSwitcher::onTriggered);

    m_input = new SwitcherInput(this);
    connect(m_input, &SwitcherInput::navigate,        this, &AppSwitcher::onNavigate);
    connect(m_input, &SwitcherInput::commitRequested, this, &AppSwitcher::onCommit);
    connect(m_input, &SwitcherInput::cancelRequested, this, &AppSwitcher::onCancel);

    connect(store, &SettingsStore::settingsChanged, this, &AppSwitcher::onSettingsChanged);
}

AppSwitcher::~AppSwitcher() = default;

void AppSwitcher::start()
{
    onSettingsChanged();
}

void AppSwitcher::onSettingsChanged()
{
    const AppSettings &s = m_store->settings();
    m_input->setTrigger(s.switcherKeyCode, s.switcherModifiers);

    m_hotkey->unregisterHotkey();
    if (s.switcherEnabled && s.switcherKeyCode > 0) {
        if (m_hotkey->registerHotkey((unsigned)s.switcherKeyCode,
                                     (unsigned)s.switcherModifiers)) {
            qInfo() << "[iFancyZones] app-switcher hotkey registered:" << s.switcherLabel;
        } else {
            qWarning() << "[iFancyZones] failed to register app-switcher hotkey"
                       << s.switcherLabel;
        }
    }
}

void AppSwitcher::onTriggered()
{
    // While open, a repeated trigger advances the selection (Cmd-Tab feel).
    if (m_open) {
        onNavigate(+1);
        return;
    }
    open();
}

void AppSwitcher::open()
{
    QList<RunningAppInfo> apps = AppSwitcherBridge::runningApps(/*iconPx=*/256);
    if (apps.size() < 2) {
        qInfo() << "[iFancyZones] app-switcher: not enough apps to switch";
        return;
    }

    if (!m_overlay) m_overlay = new CarouselOverlay();
    m_overlay->setApps(apps);

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    m_overlay->showOnScreen(screen);
    // Start on the next-most-recent app so a quick tap+release flips to it.
    m_overlay->setSelectedIndex(1);

    m_open = true;

    // Begin capturing keys + the modifier release. If the active tap can't be
    // created (Accessibility not granted), fall back to committing the default
    // selection immediately so the gesture still does something useful.
    if (!m_input->begin()) {
        qWarning() << "[iFancyZones] app-switcher: event tap unavailable"
                      " (grant Accessibility); committing default selection";
        close(/*activateSelection=*/true);
    }
}

void AppSwitcher::onNavigate(int delta)
{
    if (!m_open || !m_overlay) return;
    m_overlay->setSelectedIndex(m_overlay->selectedIndex() + delta);
}

void AppSwitcher::onCommit() { close(/*activateSelection=*/true); }
void AppSwitcher::onCancel() { close(/*activateSelection=*/false); }

void AppSwitcher::close(bool activateSelection)
{
    if (!m_open) return;
    m_open = false;
    m_input->end();

    qint64 pid = -1;
    if (m_overlay) {
        pid = m_overlay->selectedPid();
        m_overlay->hideOverlay();
    }

    if (activateSelection && pid > 0) {
        AppSwitcherBridge::activateApp(pid);
    }
}

} // namespace ifz
