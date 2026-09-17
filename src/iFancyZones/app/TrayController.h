#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>

class QSystemTrayIcon;
class QMenu;

namespace ifz {

class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(QObject *parent = nullptr);
    ~TrayController() override;

    bool install();

    // Best-effort geometry of the menu-bar icon in global screen coords.
    // Used by TrayPopover for anchoring.
    QRect iconGeometry() const;

signals:
    void activated();              // user clicked the icon
    void settingsRequested();
    void quitRequested();

private slots:
    void onActivated(int reason);

private:
    QPointer<QSystemTrayIcon> m_tray;
    QPointer<QMenu> m_menu;
};

} // namespace ifz
