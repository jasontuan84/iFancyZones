#pragma once

#include <QElapsedTimer>
#include <QFrame>
#include <QPainterPath>
#include <QPointer>
#include <QRectF>
#include <QUuid>
#include <QVector>

class QStackedWidget;

namespace ifz {

class AccessibilityBridge;
class SettingsStore;
class ScreenLayoutTab;
class LayoutsTab;

// Frameless popover anchored to the menu-bar icon: a vibrancy panel with a
// beak pointing at the status item, in the shape macOS menu-bar apps use.
class TrayPopover : public QFrame {
    Q_OBJECT
public:
    explicit TrayPopover(SettingsStore *store,
                         AccessibilityBridge *ax,
                         QWidget *parent = nullptr);

    // `trayIconGeometry` is in global screen coords (top-left).
    void showAtTray(const QRect &trayIconGeometry);

    // Open, or close again if this click is the one that dismissed us.
    // This is what the menu-bar icon should call.
    void toggleAtTray(const QRect &trayIconGeometry);

    // Close and remember when, so the next icon click reads as a toggle.
    void dismiss();

signals:
    void settingsRequested();
    void quitRequested();
    void newLayoutRequested();
    void editLayoutRequested(const QUuid &layoutId);
    void duplicateLayoutRequested(const QUuid &layoutId);
    void deleteLayoutRequested(const QUuid &layoutId);
    void templateRequested(const QString &name, const QVector<QRectF> &zones);

protected:
    bool event(QEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private:
    // Outline of the whole panel: rounded body plus the beak on top.
    QPainterPath panelPath() const;
    // Beak tip, in widget coords, clamped so it stays on the rounded body.
    qreal beakCenterX() const;

    SettingsStore *m_store;
    AccessibilityBridge *m_ax;
    QStackedWidget *m_pages = nullptr;
    QPointer<ScreenLayoutTab> m_screenTab;
    QPointer<LayoutsTab> m_layoutsTab;
    qreal m_beakX = 0;         // desired beak tip, widget coords
    QElapsedTimer m_shownAt;   // guards the click that opened us
    QElapsedTimer m_hiddenAt;  // guards the click that closed us
};

} // namespace ifz
