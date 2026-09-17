#pragma once

#include <QObject>
#include <QPoint>

namespace ifz {

// Installs a global CGEventTap that detects a left-mouse drag and tracks
// the configured activation key (default kVK_Space = 49). Emits Qt signals
// on the main thread.
//
// Lifecycle: call install() once after Accessibility permission has been
// granted. uninstall() on app quit.
class DragDetector : public QObject {
    Q_OBJECT
public:
    explicit DragDetector(QObject *parent = nullptr);
    ~DragDetector() override;

    bool install();
    void uninstall();
    bool isInstalled() const;

    // Update the keycode we treat as the "activation key" (Space by default).
    // 0 means "no activation key required" — never used in v1.
    void setActivationKeyCode(int kVK);

signals:
    // Fired the moment a left-mouse-down lands on a non-overlay window.
    void dragStarted(QPoint screenPoint);
    // Fired on every drag move.
    void dragMoved(QPoint screenPoint);
    // Fired when the user releases the mouse.
    void dragEnded(QPoint screenPoint);
    // Fired when the activation-key-held state flips during a drag (or anytime).
    void activationHeldChanged(bool held);

public:
    struct Impl; // exposed so the C-style event-tap callback (defined in the .mm)
                 // can reach into it.
private:
    Impl *d;
};

} // namespace ifz
