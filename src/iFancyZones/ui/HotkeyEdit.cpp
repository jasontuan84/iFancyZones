#include "HotkeyEdit.h"

#include <QKeyEvent>

namespace ifz {

namespace {

// Map Qt::Key to a few common macOS virtual keycodes.
// Full coverage would require AppKit; we cover the keys most likely to be
// chosen as activation. Anything not in this map records the Qt key code
// as-is (clamped to 0 if invalid).
int qtKeyToKVK(int qtKey)
{
    switch (qtKey) {
        case Qt::Key_Space:    return 49;
        case Qt::Key_Tab:      return 48;
        case Qt::Key_Return:   return 36;
        case Qt::Key_Enter:    return 76;
        case Qt::Key_Escape:   return 53;
        case Qt::Key_F1:       return 122;
        case Qt::Key_F2:       return 120;
        case Qt::Key_F3:       return 99;
        case Qt::Key_F4:       return 118;
        case Qt::Key_F5:       return 96;
        case Qt::Key_F6:       return 97;
        case Qt::Key_F7:       return 98;
        case Qt::Key_F8:       return 100;
        case Qt::Key_F9:       return 101;
        case Qt::Key_F10:      return 109;
        case Qt::Key_F11:      return 103;
        case Qt::Key_F12:      return 111;
        case Qt::Key_Shift:    return 56;
        case Qt::Key_Control:  return 59;
        case Qt::Key_Alt:      return 58;
        case Qt::Key_Meta:     return 55;
        default: return -1;
    }
}

QString labelForKey(int qtKey, const QString &text)
{
    if (!text.isEmpty()) return text.toUpper();
    switch (qtKey) {
        case Qt::Key_Space:   return QStringLiteral("Space");
        case Qt::Key_Tab:     return QStringLiteral("Tab");
        case Qt::Key_Return:  return QStringLiteral("Return");
        case Qt::Key_Escape:  return QStringLiteral("Esc");
        case Qt::Key_Shift:   return QStringLiteral("Shift");
        case Qt::Key_Control: return QStringLiteral("Ctrl");
        case Qt::Key_Alt:     return QStringLiteral("Option");
        case Qt::Key_Meta:    return QStringLiteral("Cmd");
        default: return QStringLiteral("Key");
    }
}

} // unnamed namespace

HotkeyEdit::HotkeyEdit(QWidget *parent)
    : QPushButton(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setCheckable(false);
    refreshText();
    connect(this, &QPushButton::clicked, this, &HotkeyEdit::onClicked);
}

void HotkeyEdit::setHotkey(int kVKCode, const QString &label)
{
    m_keyCode = kVKCode;
    m_label = label;
    refreshText();
}

void HotkeyEdit::onClicked()
{
    m_capturing = true;
    setText(QStringLiteral("Press a key…"));
}

void HotkeyEdit::refreshText()
{
    setText(QStringLiteral("Activation key: %1").arg(m_label));
}

void HotkeyEdit::focusOutEvent(QFocusEvent *e)
{
    if (m_capturing) {
        m_capturing = false;
        refreshText();
    }
    QPushButton::focusOutEvent(e);
}

void HotkeyEdit::keyPressEvent(QKeyEvent *e)
{
    if (!m_capturing) {
        QPushButton::keyPressEvent(e);
        return;
    }
    int kvk = qtKeyToKVK(e->key());
    if (kvk < 0) {
        // Unsupported key for now; ignore.
        return;
    }
    m_keyCode = kvk;
    m_label   = labelForKey(e->key(), e->text());
    m_capturing = false;
    refreshText();
    emit hotkeyChanged(m_keyCode, m_label);
}

} // namespace ifz
