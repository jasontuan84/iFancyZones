#pragma once

#include <QObject>
#include <QPointer>

namespace ifz {

class SettingsStore;
class HotkeyManager;
class SwitcherInput;
class CarouselOverlay;

// Coordinates the 3D app-switcher feature:
//   trigger chord (HotkeyManager)  -> open the carousel
//   arrow keys / modifier release  (SwitcherInput) -> navigate / commit
//   commit -> raise the selected app (AppSwitcherBridge)
//
// The active event tap (SwitcherInput) exists only while the carousel is
// open, so there is no input-path cost when the feature is idle.
class AppSwitcher : public QObject {
    Q_OBJECT
public:
    AppSwitcher(SettingsStore *store, QObject *parent = nullptr);
    ~AppSwitcher() override;

    void start();   // register the trigger hotkey from current settings

private slots:
    void onTriggered();
    void onNavigate(int delta);
    void onCommit();
    void onCancel();
    void onSettingsChanged();

private:
    void open();
    void close(bool activateSelection);

    SettingsStore  *m_store;
    HotkeyManager  *m_hotkey = nullptr;
    SwitcherInput  *m_input = nullptr;
    QPointer<CarouselOverlay> m_overlay;
    bool m_open = false;
};

} // namespace ifz
