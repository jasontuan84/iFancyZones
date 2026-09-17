#include "HotkeyManager.h"

#import <Carbon/Carbon.h>

#include <QMetaObject>

namespace ifz {

struct HotkeyManager::Impl {
    EventHotKeyRef ref = nullptr;
    EventHandlerRef handler = nullptr;
    HotkeyManager *owner = nullptr;
    UInt32 hotkeyId = 0;     // unique per instance
};

namespace {

// Each HotkeyManager installs its own application-level handler. Because the
// handlers share one event stack, a handler MUST only consume the hotkey it
// registered and let every other hotkey fall through to the next handler —
// otherwise the last-installed handler swallows ALL hotkeys.
OSStatus HandleHotkey(EventHandlerCallRef /*caller*/, EventRef event, void *userData)
{
    auto *d = static_cast<HotkeyManager::Impl *>(userData);
    HotkeyManager *owner = d->owner;
    if (!owner) return eventNotHandledErr;

    EventHotKeyID hkid;
    if (GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID,
                          nullptr, sizeof(hkid), nullptr, &hkid) != noErr) {
        return eventNotHandledErr;
    }
    if (hkid.id != d->hotkeyId) return eventNotHandledErr;  // not ours

    QMetaObject::invokeMethod(owner, [owner]() {
        emit owner->triggered();
    }, Qt::QueuedConnection);
    return noErr;
}

// Monotonic source of unique hotkey ids (main thread only).
UInt32 NextHotkeyId()
{
    static UInt32 counter = 0;
    return ++counter;
}

} // unnamed namespace

HotkeyManager::HotkeyManager(QObject *parent)
    : QObject(parent), d(new Impl)
{
    d->owner = this;
    d->hotkeyId = NextHotkeyId();
    EventTypeSpec evt;
    evt.eventClass = kEventClassKeyboard;
    evt.eventKind  = kEventHotKeyPressed;
    InstallApplicationEventHandler(&HandleHotkey, 1, &evt, d, &d->handler);
}

HotkeyManager::~HotkeyManager()
{
    unregisterHotkey();
    if (d->handler) RemoveEventHandler(d->handler);
    delete d;
}

bool HotkeyManager::registerHotkey(unsigned keycode, unsigned modifiers)
{
    unregisterHotkey();
    EventHotKeyID id;
    id.signature = 'iFZH';
    id.id = d->hotkeyId;
    OSStatus s = RegisterEventHotKey((UInt32)keycode,
                                     (UInt32)modifiers,
                                     id,
                                     GetApplicationEventTarget(),
                                     0,
                                     &d->ref);
    return s == noErr;
}

void HotkeyManager::unregisterHotkey()
{
    if (d->ref) {
        UnregisterEventHotKey(d->ref);
        d->ref = nullptr;
    }
}

bool HotkeyManager::isRegistered() const
{
    return d->ref != nullptr;
}

} // namespace ifz
