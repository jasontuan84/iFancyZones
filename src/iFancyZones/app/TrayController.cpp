#include "TrayController.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

namespace ifz {

namespace {

QIcon makeIcon()
{
    QPixmap pm(22, 22);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::black, 1.5);
    p.setPen(pen);
    p.drawRoundedRect(2, 2, 18, 18, 3, 3);
    p.drawLine(11, 2, 11, 20);
    p.drawLine(2, 11, 20, 11);
    QIcon icon(pm);
    icon.setIsMask(true); // adopt menu-bar light/dark colour
    return icon;
}

} // unnamed namespace

TrayController::TrayController(QObject *parent)
    : QObject(parent)
{}

TrayController::~TrayController() = default;

bool TrayController::install()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return false;
    }
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(makeIcon());
    m_tray->setToolTip(QStringLiteral("iFancyZones"));

    // No context menu — a left-click opens the popover directly.
    // Quit / Settings are surfaced in the popover footer.
    connect(m_tray.data(), &QSystemTrayIcon::activated,
            this, [this](QSystemTrayIcon::ActivationReason r) { onActivated(int(r)); });

    m_tray->show();
    return true;
}

void TrayController::onActivated(int reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        emit activated();
    }
}

QRect TrayController::iconGeometry() const
{
    if (!m_tray) return QRect();
    return m_tray->geometry();
}

} // namespace ifz
