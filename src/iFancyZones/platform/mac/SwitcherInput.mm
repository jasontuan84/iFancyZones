#include "SwitcherInput.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>   // kVK_*

#include <QMetaObject>

namespace ifz {

// Carbon modifier bits (mirror HotkeyManager / AppSettings).
static constexpr unsigned kCarbonCmd     = 1u << 8;
static constexpr unsigned kCarbonShift   = 1u << 9;
static constexpr unsigned kCarbonOption  = 1u << 11;
static constexpr unsigned kCarbonControl = 1u << 12;

struct SwitcherInput::Impl {
    CFMachPortRef tap = nullptr;
    CFRunLoopSourceRef src = nullptr;
    SwitcherInput *owner = nullptr;

    int baseKeyCode = kVK_Space;
    CGEventFlags holdMask = kCGEventFlagMaskAlternate;
    bool active = false;
    bool resolved = false;     // commit/cancel already emitted this session

    void postNavigate(int delta) {
        SwitcherInput *o = owner;
        QMetaObject::invokeMethod(o, [o, delta]() {
            emit o->navigate(delta);
        }, Qt::QueuedConnection);
    }
    void postCommit() {
        if (resolved) return;
        resolved = true;
        active = false;
        SwitcherInput *o = owner;
        QMetaObject::invokeMethod(o, [o]() {
            emit o->commitRequested();
        }, Qt::QueuedConnection);
    }
    void postCancel() {
        if (resolved) return;
        resolved = true;
        active = false;
        SwitcherInput *o = owner;
        QMetaObject::invokeMethod(o, [o]() {
            emit o->cancelRequested();
        }, Qt::QueuedConnection);
    }
};

namespace {

CGEventFlags HoldMaskFromCarbon(unsigned carbon)
{
    CGEventFlags m = 0;
    if (carbon & kCarbonCmd)     m |= kCGEventFlagMaskCommand;
    if (carbon & kCarbonShift)   m |= kCGEventFlagMaskShift;
    if (carbon & kCarbonOption)  m |= kCGEventFlagMaskAlternate;
    if (carbon & kCarbonControl) m |= kCGEventFlagMaskControl;
    return m;
}

CGEventRef TapCallback(CGEventTapProxy /*proxy*/, CGEventType type,
                       CGEventRef event, void *userData)
{
    auto *d = static_cast<SwitcherInput::Impl *>(userData);

    if (type == kCGEventTapDisabledByTimeout ||
        type == kCGEventTapDisabledByUserInput) {
        if (d->tap) CGEventTapEnable(d->tap, true);
        return event;
    }
    if (!d->active) return event;

    if (type == kCGEventFlagsChanged) {
        const CGEventFlags flags = CGEventGetFlags(event);
        if (d->holdMask != 0 && (flags & d->holdMask) != d->holdMask) {
            d->postCommit();   // the Cmd-Tab gesture: modifier released
        }
        return event;          // never swallow modifier changes
    }

    if (type == kCGEventKeyDown) {
        const int kc = (int)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        const CGEventFlags flags = CGEventGetFlags(event);
        const bool shift = (flags & kCGEventFlagMaskShift) != 0;

        switch (kc) {
            case kVK_LeftArrow:        d->postNavigate(-1); return nullptr;
            case kVK_RightArrow:       d->postNavigate(+1); return nullptr;
            case kVK_Escape:           d->postCancel();     return nullptr;
            case kVK_Return:
            case kVK_ANSI_KeypadEnter: d->postCommit();     return nullptr;
            default: break;
        }
        // Re-tapping the trigger base key advances (Shift = backwards).
        if (kc == d->baseKeyCode) {
            d->postNavigate(shift ? -1 : +1);
            return nullptr;
        }
        // Swallow other keystrokes so they don't leak to the app behind the
        // overlay while the switcher owns the keyboard.
        return nullptr;
    }

    return event;
}

} // unnamed namespace

SwitcherInput::SwitcherInput(QObject *parent)
    : QObject(parent), d(new Impl)
{
    d->owner = this;
}

SwitcherInput::~SwitcherInput()
{
    end();
    delete d;
}

void SwitcherInput::setTrigger(int baseKeyCode, int carbonModifiers)
{
    d->baseKeyCode = baseKeyCode;
    d->holdMask = HoldMaskFromCarbon((unsigned)carbonModifiers);
}

bool SwitcherInput::begin()
{
    if (d->tap) return true;

    const CGEventMask mask =
        CGEventMaskBit(kCGEventKeyDown) |
        CGEventMaskBit(kCGEventFlagsChanged);

    d->tap = CGEventTapCreate(kCGHIDEventTap,
                              kCGHeadInsertEventTap,
                              kCGEventTapOptionDefault,   // active: may swallow
                              mask,
                              &TapCallback,
                              d);
    if (!d->tap) return false;

    d->src = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, d->tap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), d->src, kCFRunLoopCommonModes);
    CGEventTapEnable(d->tap, true);

    d->active = true;
    d->resolved = false;

    // If the user already let go of the hold modifier before we installed
    // (an ultra-fast tap+release), resolve on the next tick.
    if (d->holdMask != 0) {
        const CGEventFlags now =
            CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
        if ((now & d->holdMask) != d->holdMask) {
            Impl *impl = d;
            QMetaObject::invokeMethod(this, [impl]() { impl->postCommit(); },
                                      Qt::QueuedConnection);
        }
    }
    return true;
}

void SwitcherInput::end()
{
    d->active = false;
    if (d->src) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), d->src, kCFRunLoopCommonModes);
        CFRelease(d->src);
        d->src = nullptr;
    }
    if (d->tap) {
        CGEventTapEnable(d->tap, false);
        CFRelease(d->tap);
        d->tap = nullptr;
    }
}

bool SwitcherInput::isActive() const
{
    return d->active;
}

} // namespace ifz
