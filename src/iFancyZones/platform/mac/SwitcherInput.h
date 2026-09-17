#pragma once

#include <QObject>

namespace ifz {

// Transient, ACTIVE CGEventTap used only while the 3D app switcher is on
// screen. Unlike DragDetector's listen-only tap, this one swallows the keys
// it handles (arrows, Esc, Enter, the trigger key) so they never reach the
// app underneath. It is installed in begin() and torn down in end(), so there
// is zero cost in the input path while the switcher is closed.
//
// The "hold modifier" is whatever modifier(s) the trigger chord uses (e.g.
// Option for Opt+Space). Releasing it commits the current selection — the
// Cmd-Tab gesture. If the chord has no modifier, the switcher stays open
// until Enter/Esc.
class SwitcherInput : public QObject {
    Q_OBJECT
public:
    explicit SwitcherInput(QObject *parent = nullptr);
    ~SwitcherInput() override;

    // Carbon modifier mask (cmdKey=1<<8, shiftKey=1<<9, optionKey=1<<11,
    // controlKey=1<<12) and the base keycode of the trigger chord. Used to
    // know which modifier to watch for release and which base key re-advances
    // the selection.
    void setTrigger(int baseKeyCode, int carbonModifiers);

    // Install the active tap. Returns false if Accessibility permission is
    // missing (CGEventTapCreate fails). If the hold modifier is already up at
    // install time, commitRequested() is emitted on the next tick so a very
    // fast tap+release still resolves.
    bool begin();
    void end();
    bool isActive() const;

signals:
    void navigate(int delta);     // -1 = left/prev, +1 = right/next
    void commitRequested();       // hold modifier released, or Enter
    void cancelRequested();       // Escape

public:
    struct Impl;
private:
    Impl *d;
};

} // namespace ifz
