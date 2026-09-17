#pragma once

#include <QHash>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QRect>

class QScreen;
class QTimer;

namespace ifz {

class SettingsStore;
class AccessibilityBridge;
class DragDetector;
class HotkeyManager;
class ZoneOverlay;

// Orchestrates: drag detection + activation-key state + per-screen overlays
// + AX window move on drop.
class WindowManager : public QObject {
    Q_OBJECT
public:
    WindowManager(SettingsStore *store,
                  AccessibilityBridge *ax,
                  QObject *parent = nullptr);
    ~WindowManager() override;

    void start();  // installs the CGEventTap; safe to call repeatedly
    void stop();

private slots:
    void onDragStarted(QPoint p);
    void onDragMoved(QPoint p);
    void onDragEnded(QPoint p);
    void onActivationHeldChanged(bool held);
    void onScreensChanged();
    void onBindingsChanged();
    void onSettingsChanged();
    void onLayoutsChanged();
    void onCycleHotkey();

private:
    void rebuildOverlays();
    void showOverlaysForDrag();
    void hideAllOverlays();
    ZoneOverlay *overlayAt(const QPoint &globalPt) const;

    SettingsStore *m_store;
    AccessibilityBridge *m_ax;
    DragDetector *m_drag = nullptr;
    HotkeyManager *m_cycleHotkey = nullptr;

    QHash<QScreen *, QPointer<ZoneOverlay>> m_overlays;
    QTimer *m_axWatchdog = nullptr;
    bool m_axTrustedSnapshot = false;

    bool m_dragging = false;
    bool m_activationHeld = false;
    bool m_overlaysVisible = false;
    QPoint m_dragOrigin;
    ZoneOverlay *m_activeOverlay = nullptr;
    int m_activeZoneIndex = -1;
    void *m_capturedWindow = nullptr;   // AX handle captured at drag start
};

} // namespace ifz
