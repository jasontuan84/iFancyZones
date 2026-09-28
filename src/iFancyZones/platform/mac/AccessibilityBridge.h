#pragma once

#include <QObject>
#include <QRect>
#include <QString>

namespace ifz {

// Pure-C++ facade over the macOS Accessibility framework.
// All Cocoa/AX types are hidden inside the .mm implementation.
class AccessibilityBridge : public QObject {
    Q_OBJECT
public:
    explicit AccessibilityBridge(QObject *parent = nullptr);
    ~AccessibilityBridge() override;

    // Returns true if Accessibility permission is currently granted.
    bool isTrusted() const;

    // Opens the system prompt. The user must toggle the switch manually
    // in System Settings; this call returns immediately. Returns the
    // current trust state at the moment of the call.
    bool requestTrust();

    // Move + resize the frontmost focused window of the frontmost app
    // to `targetGlobal` (in Qt's top-left point coordinates).
    // Returns true on success.
    bool moveFrontmostWindow(const QRect &targetGlobal);

    // Move + resize the focused window of the app owning the window
    // currently at `screenPoint` (drag source) to `targetGlobal`.
    // This is more robust than `moveFrontmostWindow` because during a
    // drag the user's window may briefly lose focus.
    bool moveWindowAtPoint(const QPoint &screenPoint, const QRect &targetGlobal);

    // Cache the rect of the window at `screenPoint`. Returned by
    // `releaseCachedFrame` if the user unsnaps later.
    QRect frameOfWindowAtPoint(const QPoint &screenPoint);

    // Opaque handle to a specific AX window. Use at drag start to lock onto
    // the window the user is dragging; survives focus changes mid-drag.
    using WindowHandle = void *;

    // Returns a retained handle to the focused window of the frontmost app,
    // or nullptr if none. Caller must call releaseWindowHandle() exactly once.
    WindowHandle captureFocusedWindow();

    // Returns a retained handle to the topmost window under `screenPoint`,
    // skipping our own windows, or nullptr if none. At mouse-up this is the
    // window the user dragged, even when it did not exist at mouse-down (a
    // torn-off browser tab) or never got AX focus. Caller must call
    // releaseWindowHandle() exactly once.
    WindowHandle captureWindowAtPoint(const QPoint &screenPoint);

    // Move + resize a previously captured window. Returns true on success.
    bool moveCapturedWindow(WindowHandle h, const QRect &targetGlobal);

    // Release the retain held by captureFocusedWindow().
    void releaseWindowHandle(WindowHandle h);

    // Return the frame (top-left primary-screen coords) of the frontmost
    // app's focused window, plus its pid via *outPid. Returns an invalid
    // QRect if nothing is focused.
    QRect frameOfFocusedWindow(qint64 *outPid = nullptr);

    // Raise + focus a specific window identified by pid + CGWindowID, and
    // activate its owning app. Returns true on success.
    bool raiseWindow(qint64 pid, quint32 cgWindowID);

private:
    struct Impl;
    Impl *d;
};

} // namespace ifz
