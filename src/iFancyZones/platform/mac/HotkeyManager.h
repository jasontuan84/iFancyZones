#pragma once

#include <QObject>

namespace ifz {

// Wraps Carbon's RegisterEventHotKey to publish a global hotkey as a Qt
// signal. One instance owns at most one hotkey at a time; calling
// registerHotkey() replaces any prior registration.
class HotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager() override;

    // Carbon modifier mask constants (re-exported so callers don't need
    // to include <Carbon/Carbon.h>):
    //   cmdKey     = 1 << 8
    //   shiftKey   = 1 << 9
    //   optionKey  = 1 << 11
    //   controlKey = 1 << 12
    static constexpr unsigned ModCmd     = 1u << 8;
    static constexpr unsigned ModShift   = 1u << 9;
    static constexpr unsigned ModOption  = 1u << 11;
    static constexpr unsigned ModControl = 1u << 12;

    // Register a global hotkey. keycode is a macOS virtual keycode (kVK_*).
    // modifiers is a Carbon mask (OR of ModCmd/ModShift/ModOption/ModControl).
    // Returns true on success.
    bool registerHotkey(unsigned keycode, unsigned modifiers);
    void unregisterHotkey();
    bool isRegistered() const;

signals:
    void triggered();

public:
    struct Impl;          // public so the C handler in the .mm can reach it
private:
    Impl *d;
};

} // namespace ifz
