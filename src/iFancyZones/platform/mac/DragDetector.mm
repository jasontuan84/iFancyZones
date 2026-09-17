#include "DragDetector.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h> // for kVK_*

#include <QDebug>
#include <QMetaObject>

namespace ifz {

struct DragDetector::Impl {
    CFMachPortRef tap = nullptr;
    CFRunLoopSourceRef src = nullptr;
    int activationKeyCode = kVK_Space;
    bool activationHeld = false;
    bool dragging = false;
    QPoint dragOrigin;
    DragDetector *owner = nullptr;

    void postDragStarted(CGPoint p) {
        const QPoint q(int(p.x), int(p.y));
        DragDetector *o = owner;
        QMetaObject::invokeMethod(o, [o, q]() {
            emit o->dragStarted(q);
        }, Qt::QueuedConnection);
    }
    void postDragMoved(CGPoint p) {
        const QPoint q(int(p.x), int(p.y));
        DragDetector *o = owner;
        QMetaObject::invokeMethod(o, [o, q]() {
            emit o->dragMoved(q);
        }, Qt::QueuedConnection);
    }
    void postDragEnded(CGPoint p) {
        const QPoint q(int(p.x), int(p.y));
        DragDetector *o = owner;
        QMetaObject::invokeMethod(o, [o, q]() {
            emit o->dragEnded(q);
        }, Qt::QueuedConnection);
    }
    void postActivationHeldChanged(bool held) {
        DragDetector *o = owner;
        QMetaObject::invokeMethod(o, [o, held]() {
            emit o->activationHeldChanged(held);
        }, Qt::QueuedConnection);
    }
};

namespace {

CGEventRef TapCallback(CGEventTapProxy /*proxy*/, CGEventType type,
                       CGEventRef event, void *userData)
{
    auto *d = static_cast<DragDetector::Impl *>(userData);

    switch (type) {
        case kCGEventTapDisabledByTimeout:
        case kCGEventTapDisabledByUserInput:
            if (d->tap) CGEventTapEnable(d->tap, true);
            return event;

        case kCGEventLeftMouseDown: {
            CGPoint p = CGEventGetLocation(event);
            d->dragging = true;
            d->dragOrigin = QPoint(int(p.x), int(p.y));
            d->postDragStarted(p);
            break;
        }

        case kCGEventLeftMouseDragged: {
            if (!d->dragging) break;
            CGPoint p = CGEventGetLocation(event);
            d->postDragMoved(p);
            break;
        }

        case kCGEventLeftMouseUp: {
            if (!d->dragging) break;
            CGPoint p = CGEventGetLocation(event);
            d->dragging = false;
            d->postDragEnded(p);
            break;
        }

        case kCGEventKeyDown: {
            int kc = (int)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
            if (kc == d->activationKeyCode && !d->activationHeld) {
                d->activationHeld = true;
                d->postActivationHeldChanged(true);
            }
            break;
        }

        case kCGEventKeyUp: {
            int kc = (int)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
            if (kc == d->activationKeyCode && d->activationHeld) {
                d->activationHeld = false;
                d->postActivationHeldChanged(false);
            }
            break;
        }

        case kCGEventFlagsChanged: {
            // Only meaningful when the activation key is a modifier.
            const CGEventFlags flags = CGEventGetFlags(event);
            bool held = false;
            switch (d->activationKeyCode) {
                case kVK_Shift:       held = (flags & kCGEventFlagMaskShift)    != 0; break;
                case kVK_RightShift:  held = (flags & kCGEventFlagMaskShift)    != 0; break;
                case kVK_Control:     held = (flags & kCGEventFlagMaskControl)  != 0; break;
                case kVK_RightControl:held = (flags & kCGEventFlagMaskControl)  != 0; break;
                case kVK_Option:      held = (flags & kCGEventFlagMaskAlternate)!= 0; break;
                case kVK_RightOption: held = (flags & kCGEventFlagMaskAlternate)!= 0; break;
                case kVK_Command:     held = (flags & kCGEventFlagMaskCommand)  != 0; break;
                case kVK_RightCommand:held = (flags & kCGEventFlagMaskCommand)  != 0; break;
                default: return event;
            }
            if (held != d->activationHeld) {
                d->activationHeld = held;
                d->postActivationHeldChanged(held);
            }
            break;
        }

        default: break;
    }
    return event; // never swallow user input
}

} // unnamed namespace

DragDetector::DragDetector(QObject *parent)
    : QObject(parent), d(new Impl)
{
    d->owner = this;
}

DragDetector::~DragDetector()
{
    uninstall();
    delete d;
}

bool DragDetector::install()
{
    if (d->tap) return true;

    const CGEventMask mask =
        CGEventMaskBit(kCGEventLeftMouseDown)    |
        CGEventMaskBit(kCGEventLeftMouseDragged) |
        CGEventMaskBit(kCGEventLeftMouseUp)      |
        CGEventMaskBit(kCGEventKeyDown)          |
        CGEventMaskBit(kCGEventKeyUp)            |
        CGEventMaskBit(kCGEventFlagsChanged);

    d->tap = CGEventTapCreate(kCGHIDEventTap,
                              kCGHeadInsertEventTap,
                              kCGEventTapOptionListenOnly,
                              mask,
                              &TapCallback,
                              d);
    if (!d->tap) {
        return false;
    }

    d->src = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, d->tap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), d->src, kCFRunLoopCommonModes);
    CGEventTapEnable(d->tap, true);
    return true;
}

void DragDetector::uninstall()
{
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

bool DragDetector::isInstalled() const
{
    return d->tap != nullptr;
}

void DragDetector::setActivationKeyCode(int kVK)
{
    d->activationKeyCode = kVK;
}

} // namespace ifz
