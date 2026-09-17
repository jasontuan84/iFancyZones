#include "TrayPopover.h"

#include "LayoutsTab.h"
#include "PopoverTheme.h"
#include "PopoverWidgets.h"
#include "ScreenLayoutTab.h"
#include "platform/mac/AppKitBridge.h"
#include "services/SettingsStore.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWindow>

#include <functional>

namespace ifz {

namespace {

// Panel geometry. The beak sits above the body, so the widget is that much
// taller than the content and every layout is inset by kBeakH at the top.
constexpr int kBeakH = 11;
constexpr int kBeakW = 26;
constexpr int kRadius = 12;
constexpr int kBodyW = 348;
constexpr int kBodyH = 548;

// The status-item click that opens the popover keeps generating events for a
// few moments afterwards (mouse-up, key-window shuffling). Ignore any
// deactivation inside this window or the popover closes the instant it opens.
constexpr qint64 kOpenGraceMs = 400;
// A click on the icon while the popover is open first deactivates it, then
// delivers `activated`. Treat a show request this soon after a hide as the
// second half of that same click, i.e. a toggle-closed.
constexpr qint64 kToggleGraceMs = 400;

// The rounded blue app mark shown in the header.
class AppMark : public QWidget {
public:
    explicit AppMark(QWidget *parent) : QWidget(parent) { setFixedSize(28, 28); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect());
        p.setPen(Qt::NoPen);
        p.setBrush(theme::accent());
        p.drawRoundedRect(r, 7, 7);

        // A miniature 2x2 zone grid, the same mark as the menu-bar icon.
        p.setBrush(QColor(255, 255, 255, 235));
        const qreal m = 6.5, g = 1.6;
        const qreal w = (r.width() - 2 * m - g) / 2;
        const qreal h = (r.height() - 2 * m - g) / 2;
        for (int row = 0; row < 2; ++row) {
            for (int c = 0; c < 2; ++c) {
                p.drawRoundedRect(QRectF(m + c * (w + g), m + row * (h + g), w, h),
                                  1.2, 1.2);
            }
        }
    }
};

// A full-width footer row: title on the left, optional shortcut hint on the
// right, highlight on hover. Plain QWidget, so it needs no moc of its own.
class FooterRow : public QWidget {
public:
    FooterRow(const QString &title, const QString &hint,
              std::function<void()> onClick, QWidget *parent,
              Qt::Alignment align = Qt::AlignLeft)
        : QWidget(parent), m_onClick(std::move(onClick))
    {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(8, 6, 8, 6);
        row->setSpacing(6);
        if (align & Qt::AlignRight) row->addStretch();
        auto *label = new QLabel(title, this);
        label->setStyleSheet(QStringLiteral("color: #ECEFF4; font-size: 13px;"));
        row->addWidget(label);
        if (!hint.isEmpty()) {
            auto *hintLabel = new QLabel(hint, this);
            hintLabel->setStyleSheet(
                QStringLiteral("color: rgba(236,239,244,110); font-size: 12px;"));
            row->addWidget(hintLabel);
        }
        if (!(align & Qt::AlignRight)) row->addStretch();
    }

protected:
    void enterEvent(QEnterEvent *) override { m_hover = true; update(); }
    void leaveEvent(QEvent *) override { m_hover = false; update(); }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())
            && m_onClick) {
            m_onClick();
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        if (!m_hover) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 28));
        p.drawRoundedRect(rect().adjusted(4, 0, -4, 0), 6, 6);
    }

private:
    std::function<void()> m_onClick;
    bool m_hover = false;
};

} // unnamed namespace

TrayPopover::TrayPopover(SettingsStore *store, AccessibilityBridge *ax, QWidget *parent)
    : QFrame(parent), m_store(store), m_ax(ax)
{
    // NOT Qt::Popup: a Qt popup installs a mouse grab and dismisses itself on
    // the first click outside its rect. On macOS the status-item click that
    // opens the popover is still in flight when the grab goes up, so its
    // trailing mouse-up lands "outside" and closes the popover immediately.
    // macOS 26/27 reworked NSStatusItem event delivery, which turned that
    // race into a guaranteed instant-hide. We use a tool window and handle
    // dismissal ourselves instead.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFixedSize(kBodyW, kBodyH + kBeakH);
    setStyleSheet(theme::styleSheet());
    m_beakX = width() / 2.0;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12 + kBeakH, 14, 8);
    layout->setSpacing(10);

    // Header: app mark, name, and the running indicator.
    auto *header = new QHBoxLayout();
    header->setSpacing(10);
    header->addWidget(new AppMark(this), 0, Qt::AlignVCenter);
    auto *appName = new QLabel(tr("iFancyZones"), this);
    appName->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 700;"));
    header->addWidget(appName, 0, Qt::AlignVCenter);
    header->addStretch();
    header->addWidget(new StatusPill(tr("Running"), this), 0, Qt::AlignVCenter);
    layout->addLayout(header);

    auto *segments = new SegmentedControl({tr("Displays"), tr("Layouts")}, this);
    layout->addWidget(segments);

    m_pages = new QStackedWidget(this);
    m_screenTab = new ScreenLayoutTab(m_store, this);
    m_screenTab->setAccessibilityBridge(m_ax);
    m_layoutsTab = new LayoutsTab(m_store, this);
    m_pages->addWidget(m_screenTab);
    m_pages->addWidget(m_layoutsTab);
    layout->addWidget(m_pages, 1);
    connect(segments, &SegmentedControl::currentIndexChanged,
            m_pages, &QStackedWidget::setCurrentIndex);

    // Hairline above the footer, as in the reference popover.
    auto *rule = new QFrame(this);
    rule->setFrameShape(QFrame::HLine);
    rule->setFixedHeight(1);
    rule->setStyleSheet(QStringLiteral("background: rgba(255,255,255,30); border: 0;"));
    layout->addWidget(rule);

    auto *footer = new QHBoxLayout();
    footer->setSpacing(0);
    footer->addWidget(new FooterRow(tr("Settings…"), QString(), [this]() {
        emit settingsRequested();
        dismiss();
    }, this), 1);
    footer->addWidget(new FooterRow(tr("Quit"), QStringLiteral("⌘Q"), [this]() {
        emit quitRequested();
        dismiss();
    }, this, Qt::AlignRight), 0);
    layout->addLayout(footer);

    connect(m_screenTab, &ScreenLayoutTab::settingsRequested,
            this, &TrayPopover::settingsRequested);

    connect(m_layoutsTab, &LayoutsTab::newLayoutRequested, this, [this]() {
        emit newLayoutRequested();
        dismiss();
    });
    connect(m_layoutsTab, &LayoutsTab::editLayoutRequested, this, [this](const QUuid &id) {
        emit editLayoutRequested(id);
        dismiss();
    });
    connect(m_layoutsTab, &LayoutsTab::duplicateLayoutRequested,
            this, &TrayPopover::duplicateLayoutRequested);
    connect(m_layoutsTab, &LayoutsTab::deleteLayoutRequested,
            this, &TrayPopover::deleteLayoutRequested);
    connect(m_layoutsTab, &LayoutsTab::templateRequested, this,
            [this](const QString &name, const QVector<QRectF> &zones) {
                emit templateRequested(name, zones);
                dismiss();
            });
}

void TrayPopover::toggleAtTray(const QRect &trayIconGeometry)
{
    if (isVisible()) {
        dismiss();
        return;
    }
    // Clicking the icon while the popover is open deactivates it first, so by
    // the time `activated` arrives we are already hidden. Without this the
    // popover would just blink and reopen.
    if (m_hiddenAt.isValid() && m_hiddenAt.elapsed() < kToggleGraceMs) {
        return;
    }
    showAtTray(trayIconGeometry);
}

void TrayPopover::dismiss()
{
    if (!isVisible()) return;
    m_shownAt.invalidate();
    hide();
    m_hiddenAt.start();
}

void TrayPopover::showAtTray(const QRect &trayIconGeometry)
{
    if (m_screenTab) m_screenTab->reload();
    if (m_layoutsTab) m_layoutsTab->reload();

    // Anchor relative to the screen the icon lives on, not the primary one.
    QScreen *s = trayIconGeometry.isEmpty()
                     ? QGuiApplication::primaryScreen()
                     : QGuiApplication::screenAt(trayIconGeometry.center());
    if (!s) s = QGuiApplication::primaryScreen();
    if (!s) return;
    QRect ag = s->availableGeometry();

    QPoint targetTopLeft;
    int iconCenterX;
    if (!trayIconGeometry.isEmpty()) {
        iconCenterX = trayIconGeometry.center().x();
        // Center under the tray icon; the beak covers the remaining gap.
        targetTopLeft = QPoint(iconCenterX - width() / 2, trayIconGeometry.bottom() + 2);
    } else {
        // Qt can report an empty rect for the status item. Fall back to the
        // top-right of the menu bar, where menu-bar popovers belong, rather
        // than the middle of the screen.
        targetTopLeft = QPoint(ag.right() - width() - 12, ag.top() + 2);
        iconCenterX = targetTopLeft.x() + width() / 2;
    }
    // Clamp to screen bounds.
    int x = qBound(ag.left() + 8, targetTopLeft.x(), ag.right() - width() - 8);
    int y = qBound(ag.top() + 2, targetTopLeft.y(), ag.bottom() - height() - 8);
    move(x, y);

    // Clamping may have shifted the panel off-centre; the beak still has to
    // point at the icon, so it is positioned independently.
    m_beakX = iconCenterX - x;

    // Force the platform window so AppKit can be configured before it appears.
    winId();
    AppKitBridge::configureAsPopover(windowHandle());

    m_hiddenAt.invalidate();
    m_shownAt.start();
    show();
    raise();
    // After show(), so the vibrancy view is sized from a laid-out window.
    AppKitBridge::addPopoverBlur(windowHandle(), kRadius, kBeakH, kBeakW, beakCenterX());
    update();
    // An accessory (LSUIElement) app is not frontmost, so activateWindow()
    // alone never makes the popover key, and a window that is never key can
    // neither take keyboard input nor tell us when to close.
    AppKitBridge::activateApp();
    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
}

qreal TrayPopover::beakCenterX() const
{
    // Keep the beak clear of the rounded corners.
    const qreal minX = kRadius + kBeakW / 2.0 + 2;
    const qreal maxX = width() - kRadius - kBeakW / 2.0 - 2;
    return qBound(minX, m_beakX, maxX);
}

QPainterPath TrayPopover::panelPath() const
{
    const QRectF body(0.5, kBeakH + 0.5, width() - 1.0, height() - kBeakH - 1.0);
    QPainterPath path;
    path.addRoundedRect(body, kRadius, kRadius);

    const qreal cx = beakCenterX();
    const qreal half = kBeakW / 2.0;
    QPainterPath beak;
    beak.moveTo(cx - half, body.top() + 1.0);
    beak.cubicTo(cx - half * 0.35, kBeakH * 0.15 + 0.5,
                 cx - half * 0.18, 0.5,
                 cx, 0.5);
    beak.cubicTo(cx + half * 0.18, 0.5,
                 cx + half * 0.35, kBeakH * 0.15 + 0.5,
                 cx + half, body.top() + 1.0);
    beak.closeSubpath();

    // One outline, so the border stroke does not cut across the beak's base.
    return path.united(beak);
}

bool TrayPopover::event(QEvent *e)
{
    if (e->type() == QEvent::WindowDeactivate && isVisible()) {
        if (!m_shownAt.isValid() || m_shownAt.elapsed() >= kOpenGraceMs) {
            dismiss();
        }
    }
    return QFrame::event(e);
}

void TrayPopover::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        dismiss();
        return;
    }
    QFrame::keyPressEvent(e);
}

void TrayPopover::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPainterPath path = panelPath();
    // The NSVisualEffectView behind us supplies the blur; this is only a tint
    // to darken it, plus the hairline border that defines the panel's edge.
    p.fillPath(path, QColor(22, 24, 32, 120));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 48), 1));
    p.drawPath(path);
}

} // namespace ifz
