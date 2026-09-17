#include "TrayPopover.h"

#include "LayoutsTab.h"
#include "ScreenLayoutTab.h"
#include "services/SettingsStore.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QVBoxLayout>

namespace ifz {

TrayPopover::TrayPopover(SettingsStore *store, AccessibilityBridge *ax, QWidget *parent)
    : QFrame(parent), m_store(store), m_ax(ax)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFixedSize(540, 440);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_tabs = new QTabWidget(this);
    m_screenTab = new ScreenLayoutTab(m_store, this);
    m_screenTab->setAccessibilityBridge(m_ax);
    m_layoutsTab = new LayoutsTab(m_store, this);
    m_tabs->addTab(m_screenTab, tr("Screen && Layout"));
    m_tabs->addTab(m_layoutsTab, tr("Layouts"));
    layout->addWidget(m_tabs, 1);

    auto *footer = new QHBoxLayout();
    auto *quitBtn = new QPushButton(tr("Quit"), this);
    quitBtn->setFlat(true);
    connect(quitBtn, &QPushButton::clicked, this, [this]() {
        emit quitRequested();
        close();
    });
    footer->addWidget(quitBtn);
    footer->addStretch();
    auto *settingsBtn = new QPushButton(tr("⚙  Settings…"), this);
    settingsBtn->setFlat(true);
    connect(settingsBtn, &QPushButton::clicked, this, [this]() {
        emit settingsRequested();
        close();
    });
    footer->addWidget(settingsBtn);
    layout->addLayout(footer);

    connect(m_screenTab, &ScreenLayoutTab::settingsRequested,
            this, &TrayPopover::settingsRequested);

    connect(m_layoutsTab, &LayoutsTab::newLayoutRequested, this, [this]() {
        emit newLayoutRequested();
        close();
    });
    connect(m_layoutsTab, &LayoutsTab::editLayoutRequested, this, [this](const QUuid &id) {
        emit editLayoutRequested(id);
        close();
    });
    connect(m_layoutsTab, &LayoutsTab::duplicateLayoutRequested,
            this, &TrayPopover::duplicateLayoutRequested);
    connect(m_layoutsTab, &LayoutsTab::deleteLayoutRequested,
            this, &TrayPopover::deleteLayoutRequested);
}

void TrayPopover::showAtTray(const QRect &trayIconGeometry)
{
    if (m_screenTab) m_screenTab->reload();
    if (m_layoutsTab) m_layoutsTab->reload();

    QPoint targetTopLeft;
    if (!trayIconGeometry.isEmpty()) {
        // Center horizontally under the tray icon, with a small gap.
        targetTopLeft = QPoint(trayIconGeometry.center().x() - width() / 2,
                               trayIconGeometry.bottom() + 6);
    } else {
        // Fallback: center on the primary screen, slightly below the top.
        QRect ag = QGuiApplication::primaryScreen()->availableGeometry();
        targetTopLeft = QPoint(ag.center().x() - width() / 2, ag.top() + 32);
    }
    // Clamp to screen bounds.
    QScreen *s = QGuiApplication::screenAt(targetTopLeft);
    if (!s) s = QGuiApplication::primaryScreen();
    QRect ag = s->availableGeometry();
    int x = qBound(ag.left() + 8, targetTopLeft.x(), ag.right() - width() - 8);
    int y = qBound(ag.top() + 8, targetTopLeft.y(), ag.bottom() - height() - 8);

    move(x, y);
    show();
    raise();
    activateWindow();
}

void TrayPopover::focusOutEvent(QFocusEvent *e)
{
    QFrame::focusOutEvent(e);
    // Qt::Popup auto-hides on outside click; we don't need manual handling.
}

void TrayPopover::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor bg = palette().color(QPalette::Window);
    bg.setAlpha(245);
    p.setBrush(bg);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 10, 10);
}

} // namespace ifz
