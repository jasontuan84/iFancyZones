#include "WindowManager.h"

#include "core/LayoutEngine.h"
#include "platform/mac/AccessibilityBridge.h"
#include "platform/mac/DragDetector.h"
#include "platform/mac/HotkeyManager.h"
#include "platform/mac/MacScreenInfo.h"
#include "services/SettingsStore.h"
#include "ui/ZoneOverlay.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

#import <CoreGraphics/CoreGraphics.h>

namespace ifz {

WindowManager::WindowManager(SettingsStore *store,
                             AccessibilityBridge *ax,
                             QObject *parent)
    : QObject(parent), m_store(store), m_ax(ax)
{
    m_drag = new DragDetector(this);
    m_drag->setActivationKeyCode(store->settings().activationKeyCode);

    m_cycleHotkey = new HotkeyManager(this);
    connect(m_cycleHotkey, &HotkeyManager::triggered,
            this, &WindowManager::onCycleHotkey);

    connect(m_drag, &DragDetector::dragStarted, this, &WindowManager::onDragStarted);
    connect(m_drag, &DragDetector::dragMoved,   this, &WindowManager::onDragMoved);
    connect(m_drag, &DragDetector::dragEnded,   this, &WindowManager::onDragEnded);
    connect(m_drag, &DragDetector::activationHeldChanged,
            this, &WindowManager::onActivationHeldChanged);

    connect(QGuiApplication::instance(), SIGNAL(screenAdded(QScreen*)),
            this, SLOT(onScreensChanged()));
    connect(QGuiApplication::instance(), SIGNAL(screenRemoved(QScreen*)),
            this, SLOT(onScreensChanged()));

    connect(store, &SettingsStore::bindingsChanged, this, &WindowManager::onBindingsChanged);
    connect(store, &SettingsStore::settingsChanged, this, &WindowManager::onSettingsChanged);
    connect(store, &SettingsStore::layoutsChanged,  this, &WindowManager::onLayoutsChanged);
}

WindowManager::~WindowManager()
{
    if (m_capturedWindow) {
        m_ax->releaseWindowHandle(m_capturedWindow);
        m_capturedWindow = nullptr;
    }
}

void WindowManager::start()
{
    // Dump screen info to Console.app on startup so users can verify what
    // AppKit is reporting without having to open the popover.
    const QList<Monitor> monitorsAtStart = MacScreenInfo::currentMonitors();
    qInfo() << "[iFancyZones] startup: detected"
            << monitorsAtStart.size() << "monitor(s)";

    rebuildOverlays();

    m_axTrustedSnapshot = m_ax->isTrusted();
    if (m_axTrustedSnapshot) {
        if (m_drag->install()) {
            qInfo() << "[iFancyZones] CGEventTap installed; AX permission OK";
        } else {
            qWarning() << "[iFancyZones] CGEventTap install FAILED even though AX is trusted";
        }
    } else {
        qWarning() << "[iFancyZones] AX permission not granted; tap not installed yet."
                   << "Open System Settings > Privacy & Security > Accessibility and enable iFancyZones.";
    }

    // Register the cycle-focus hotkey (independent of AX permission — Carbon
    // hotkeys don't need it).
    const AppSettings &s = m_store->settings();
    if (s.cycleFocusKeyCode > 0) {
        if (m_cycleHotkey->registerHotkey((unsigned)s.cycleFocusKeyCode,
                                          (unsigned)s.cycleFocusModifiers)) {
            qInfo() << "[iFancyZones] cycle hotkey registered:" << s.cycleFocusLabel;
        } else {
            qWarning() << "[iFancyZones] failed to register cycle hotkey" << s.cycleFocusLabel;
        }
    }

    // Watchdog: poll AX every 2s, re-install the tap as soon as the user
    // grants permission. Avoids a forced app restart.
    if (!m_axWatchdog) {
        m_axWatchdog = new QTimer(this);
        m_axWatchdog->setInterval(2000);
        connect(m_axWatchdog, &QTimer::timeout, this, [this]() {
            const bool nowTrusted = m_ax->isTrusted();
            if (nowTrusted == m_axTrustedSnapshot) return;
            m_axTrustedSnapshot = nowTrusted;
            if (nowTrusted) {
                qInfo() << "[iFancyZones] AX permission granted; installing CGEventTap";
                m_drag->install();
            } else {
                qWarning() << "[iFancyZones] AX permission revoked; uninstalling tap";
                m_drag->uninstall();
            }
        });
        m_axWatchdog->start();
    }
}

void WindowManager::stop()
{
    if (m_drag) m_drag->uninstall();
    if (m_axWatchdog) m_axWatchdog->stop();
    hideAllOverlays();
}

void WindowManager::rebuildOverlays()
{
    // Remove overlays for screens that no longer exist.
    const auto screens = QGuiApplication::screens();
    QHash<QScreen *, QPointer<ZoneOverlay>> fresh;
    for (QScreen *s : screens) {
        if (m_overlays.contains(s)) {
            fresh.insert(s, m_overlays.value(s));
        } else {
            auto *ov = new ZoneOverlay();
            ov->setGeometry(s->availableGeometry());
            ov->setShowNumbers(m_store->settings().showZoneNumbers);
            fresh.insert(s, ov);
        }
    }
    // Delete leftover overlays.
    for (auto it = m_overlays.constBegin(); it != m_overlays.constEnd(); ++it) {
        if (!fresh.contains(it.key()) && it.value()) {
            it.value()->deleteLater();
        }
    }
    m_overlays = fresh;
}

void WindowManager::onScreensChanged()
{
    rebuildOverlays();
}

void WindowManager::onSettingsChanged()
{
    const AppSettings &s = m_store->settings();
    m_drag->setActivationKeyCode(s.activationKeyCode);
    for (auto it = m_overlays.constBegin(); it != m_overlays.constEnd(); ++it) {
        if (it.value()) it.value()->setShowNumbers(s.showZoneNumbers);
    }
    // Re-register the cycle hotkey with the latest config.
    if (m_cycleHotkey && s.cycleFocusKeyCode > 0) {
        m_cycleHotkey->registerHotkey((unsigned)s.cycleFocusKeyCode,
                                      (unsigned)s.cycleFocusModifiers);
    }
}

void WindowManager::onBindingsChanged()
{
    if (m_overlaysVisible) {
        hideAllOverlays();
        showOverlaysForDrag();
    }
}

void WindowManager::onLayoutsChanged()
{
    if (m_overlaysVisible) {
        hideAllOverlays();
        showOverlaysForDrag();
    }
}

ZoneOverlay *WindowManager::overlayAt(const QPoint &globalPt) const
{
    QScreen *s = QGuiApplication::screenAt(globalPt);
    if (!s) return nullptr;
    return m_overlays.value(s).data();
}

void WindowManager::showOverlaysForDrag()
{
    if (!m_ax->isTrusted()) {
        qWarning() << "[iFancyZones] showOverlaysForDrag skipped: AX not trusted";
        return;
    }

    m_overlaysVisible = false;
    int screensWithBinding = 0;
    for (auto it = m_overlays.constBegin(); it != m_overlays.constEnd(); ++it) {
        QScreen *s = it.key();
        ZoneOverlay *ov = it.value().data();
        if (!ov) continue;

        quint32 unit = MacScreenInfo::unitNumberFor(s);
        QUuid lid = m_store->bindingFor(unit);
        if (lid.isNull()) {
            qDebug() << "[iFancyZones]   screen" << s->name()
                     << "unit=" << unit << "no binding -> skip";
            ov->hide();
            continue;
        }
        Layout l = m_store->layoutById(lid);
        if (l.zones().isEmpty()) {
            qDebug() << "[iFancyZones]   screen" << s->name() << "layout has no zones -> skip";
            ov->hide();
            continue;
        }
        ov->setLayoutAndCanvas(l, s->availableGeometry());
        ov->setActiveZone(-1);
        ov->show();
        ov->raise();
        ov->configureNative();
        ++screensWithBinding;
        m_overlaysVisible = true;
    }
    qDebug() << "[iFancyZones] showOverlaysForDrag -> overlays shown on"
             << screensWithBinding << "screen(s)";
}

void WindowManager::hideAllOverlays()
{
    for (auto it = m_overlays.constBegin(); it != m_overlays.constEnd(); ++it) {
        if (it.value()) it.value()->hide();
    }
    m_overlaysVisible = false;
    m_activeOverlay = nullptr;
    m_activeZoneIndex = -1;
}

void WindowManager::onDragStarted(QPoint p)
{
    qDebug() << "[iFancyZones] dragStarted at" << p << "activationHeld=" << m_activationHeld;
    m_dragging = true;
    m_dragOrigin = p;

    // Capture an AX handle to the focused window NOW, before the drag can
    // shift focus. We'll use this exact window on drop, no matter where the
    // cursor ends up.
    if (m_capturedWindow) {
        m_ax->releaseWindowHandle(m_capturedWindow);
        m_capturedWindow = nullptr;
    }
    m_capturedWindow = m_ax->captureFocusedWindow();
    qDebug() << "[iFancyZones]   captured AX handle =" << m_capturedWindow;

    if (m_activationHeld) showOverlaysForDrag();
}

void WindowManager::onDragMoved(QPoint p)
{
    if (!m_dragging) return;

    if (m_activationHeld && !m_overlaysVisible) {
        showOverlaysForDrag();
    }
    if (!m_activationHeld && m_overlaysVisible) {
        hideAllOverlays();
    }
    if (!m_overlaysVisible) return;

    ZoneOverlay *ov = overlayAt(p);
    // Clear previous overlay's active zone if cursor moved to a different screen.
    if (m_activeOverlay && m_activeOverlay != ov) {
        m_activeOverlay->setActiveZone(-1);
    }
    m_activeOverlay = ov;
    if (!ov) {
        m_activeZoneIndex = -1;
        return;
    }
    // hit-test relative to the screen canvas (overlay geometry).
    int idx = LayoutEngine::hitTest(/*layout=*/Layout(), ov->geometry(), p);
    Q_UNUSED(idx);

    // Hit-test against the OUTER (no-gap) rect so the entire logical zone is
    // clickable. The gap region between zones still maps to the nearest zone,
    // which feels more responsive than requiring pixel-perfect cursor placement.
    int hit = -1;
    for (int i = 0; ; ++i) {
        QRect outer = ov->pixelRectOfZoneOuter(i);
        if (outer.isNull()) break;
        if (outer.contains(p)) hit = i;
    }
    if (hit != m_activeZoneIndex) {
        m_activeZoneIndex = hit;
        ov->setActiveZone(hit);
    }
}

void WindowManager::onDragEnded(QPoint p)
{
    const bool snap = m_overlaysVisible
                      && m_activationHeld
                      && m_activeOverlay
                      && m_activeZoneIndex >= 0;
    const QRect targetRect = snap ? m_activeOverlay->pixelRectOfZone(m_activeZoneIndex) : QRect();

    qDebug() << "[iFancyZones] dragEnded at" << p
             << "snap=" << snap
             << "activationHeld=" << m_activationHeld
             << "activeOverlay=" << (m_activeOverlay != nullptr)
             << "activeZoneIndex=" << m_activeZoneIndex
             << "targetRect=" << targetRect;

    hideAllOverlays();
    m_dragging = false;

    if (!snap || targetRect.isNull()) {
        // No snap: release the captured AX handle.
        if (m_capturedWindow) {
            m_ax->releaseWindowHandle(m_capturedWindow);
            m_capturedWindow = nullptr;
        }
        return;
    }

    // Prefer the window under the cursor at drop: that is the window the user
    // dragged. The handle from drag start points to the wrong window when the
    // dragged window did not exist yet (a tab torn off into a new Chrome
    // window) or when the app still reported its previous window as focused
    // (a new JetBrains window). The drag-start handle stays as the fallback.
    void *handle = m_ax->captureWindowAtPoint(p);
    qDebug() << "[iFancyZones]   window under drop point =" << handle;
    if (handle) {
        if (m_capturedWindow) m_ax->releaseWindowHandle(m_capturedWindow);
    } else {
        handle = m_capturedWindow;
    }
    m_capturedWindow = nullptr; // ownership transferred into the closure below

    if (!handle) {
        // No handle captured (rare: app started during drag). Fall back to
        // frontmost.
        bool ok = m_ax->moveFrontmostWindow(targetRect);
        qDebug() << "[iFancyZones]   AX moveFrontmostWindow (fallback) ->" << ok
                 << "rect=" << targetRect;
        return;
    }

    // Defeat the OS drag-finalize race: the OS may write the window's
    // position one more time AFTER our mouse-up callback fires. Issue
    // two writes (immediate + +90ms) so our target wins; then release.
    bool ok1 = m_ax->moveCapturedWindow(handle, targetRect);
    qDebug() << "[iFancyZones]   AX moveCapturedWindow #1 ->" << ok1
             << "rect=" << targetRect;

    QTimer::singleShot(90, this, [this, handle, targetRect]() {
        bool ok2 = m_ax->moveCapturedWindow(handle, targetRect);
        qDebug() << "[iFancyZones]   AX moveCapturedWindow #2 ->" << ok2;
        m_ax->releaseWindowHandle(handle);
    });
}

namespace {

struct ZoneWindowCandidate {
    qint64 pid;
    quint32 cgWindowID;
    QRect frame;
};

// Enumerate visible normal-layer windows whose centre lies inside `zoneRect`.
// Returns front→back order (CGWindowList's natural order).
QList<ZoneWindowCandidate> windowsInRect(const QRect &zoneRect)
{
    QList<ZoneWindowCandidate> out;
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!list) return out;
    const CFIndex n = CFArrayGetCount(list);
    for (CFIndex i = 0; i < n; ++i) {
        CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(list, i);
        CFNumberRef layerRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowLayer);
        int layer = 0;
        if (layerRef) CFNumberGetValue(layerRef, kCFNumberIntType, &layer);
        if (layer != 0) continue;          // skip menu bar / Dock / overlays

        CFDictionaryRef bounds = (CFDictionaryRef)CFDictionaryGetValue(info, kCGWindowBounds);
        if (!bounds) continue;
        CGRect r;
        if (!CGRectMakeWithDictionaryRepresentation(bounds, &r)) continue;

        // Skip degenerate / hidden windows.
        if (r.size.width < 50 || r.size.height < 50) continue;

        QRect q((int)r.origin.x, (int)r.origin.y,
                (int)r.size.width, (int)r.size.height);
        QPoint c = q.center();
        if (!zoneRect.contains(c)) continue;

        CFNumberRef pidRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowOwnerPID);
        CFNumberRef widRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowNumber);
        int pid = 0, wid = 0;
        if (pidRef) CFNumberGetValue(pidRef, kCFNumberIntType, &pid);
        if (widRef) CFNumberGetValue(widRef, kCFNumberIntType, &wid);
        ZoneWindowCandidate c2;
        c2.pid = pid;
        c2.cgWindowID = (quint32)wid;
        c2.frame = q;
        out.append(c2);
    }
    CFRelease(list);
    return out;
}

} // unnamed namespace

void WindowManager::onCycleHotkey()
{
    qDebug() << "[iFancyZones] cycle hotkey triggered";

    // 1. Where is the currently focused window?
    qint64 focusedPid = 0;
    QRect focusedFrame = m_ax->frameOfFocusedWindow(&focusedPid);
    if (!focusedFrame.isValid()) {
        qDebug() << "[iFancyZones]   no focused window";
        return;
    }
    const QPoint focusedCenter = focusedFrame.center();

    // 2. Which screen + bound layout contains that centre?
    QScreen *screen = QGuiApplication::screenAt(focusedCenter);
    if (!screen) {
        qDebug() << "[iFancyZones]   focused window not on a known screen";
        return;
    }
    const quint32 unit = MacScreenInfo::unitNumberFor(screen);
    QUuid lid = m_store->bindingFor(unit);
    if (lid.isNull()) {
        qDebug() << "[iFancyZones]   screen has no layout bound";
        return;
    }
    Layout layout = m_store->layoutById(lid);
    if (layout.zones().isEmpty()) return;

    // 3. Which zone is the focused window in?
    const QRect canvas = screen->availableGeometry();
    int zoneIdx = LayoutEngine::hitTest(layout, canvas, focusedCenter);
    if (zoneIdx < 0) {
        qDebug() << "[iFancyZones]   focused window is not inside any zone";
        return;
    }
    const QRect zoneRect = LayoutEngine::zoneToPixelsRaw(layout.zones()[zoneIdx], canvas);
    qDebug() << "[iFancyZones]   focused window in zone" << (zoneIdx + 1)
             << "rect=" << zoneRect;

    // 4. Collect all visible windows whose centre is inside the same zone.
    QList<ZoneWindowCandidate> candidates = windowsInRect(zoneRect);
    // Filter: exclude ourselves.
    const pid_t selfPid = getpid();
    for (int i = candidates.size() - 1; i >= 0; --i) {
        if (candidates[i].pid == selfPid) candidates.removeAt(i);
    }

    qDebug() << "[iFancyZones]   candidates in zone:" << candidates.size();
    if (candidates.size() < 2) return;

    // CGWindowList returns front→back. To cycle, raise the LAST (backmost)
    // window so the order rotates.
    const ZoneWindowCandidate target = candidates.last();
    bool ok = m_ax->raiseWindow(target.pid, target.cgWindowID);
    qDebug() << "[iFancyZones]   raise pid=" << target.pid
             << "wid=" << target.cgWindowID << "->" << ok;
}

void WindowManager::onActivationHeldChanged(bool held)
{
    qDebug() << "[iFancyZones] activationHeld =" << held << "dragging=" << m_dragging;
    m_activationHeld = held;
    if (!m_dragging) return;

    if (held) {
        showOverlaysForDrag();
    } else {
        hideAllOverlays();
    }
}

} // namespace ifz
