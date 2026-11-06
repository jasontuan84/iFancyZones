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

// Frameless popover anchored to the menu-bar icon. On macOS 26 and later it
// is a floating Liquid Glass panel, the shape of the system's own menu-bar
// panels. On older systems it is a vibrancy panel with a beak that points at
// the status item.
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
    // Outline of the whole panel as one closed contour: the rounded body,
    // plus the beak on top when there is one. `inset` shrinks it, so a 1 pt
    // stroke at inset 0.5 lies exactly inside the filled shape.
    QPainterPath panelPath(qreal inset = 0) const;
    // Beak tip, in widget coords, clamped so it stays on the rounded body.
    qreal beakCenterX() const;
    qreal beakHeight() const;
    qreal cornerRadius() const;

    SettingsStore *m_store;
    AccessibilityBridge *m_ax;
    QStackedWidget *m_pages = nullptr;
    QPointer<ScreenLayoutTab> m_screenTab;
    QPointer<LayoutsTab> m_layoutsTab;
    const bool m_glass;        // Liquid Glass panel, no beak
    qreal m_beakX = 0;         // desired beak tip, widget coords
    QElapsedTimer m_shownAt;   // guards the click that opened us
    QElapsedTimer m_hiddenAt;  // guards the click that closed us
};

} // namespace ifz
