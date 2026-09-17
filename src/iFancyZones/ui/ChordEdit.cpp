#include "ChordEdit.h"

#include <QKeyEvent>
#include <QStringList>

namespace ifz {

namespace {

// Map Qt key to macOS virtual keycode for the keys most likely to be chord
// targets. Letters use Qt::Key_A..Qt::Key_Z, digits Qt::Key_0..Qt::Key_9.
int qtKeyToKVK(int qtKey)
{
    switch (qtKey) {
        case Qt::Key_Space:    return 49;
        case Qt::Key_Tab:      return 48;
        case Qt::Key_Return:   return 36;
        case Qt::Key_Enter:    return 76;
        case Qt::Key_Escape:   return 53;
        case Qt::Key_Backspace:return 51;
        case Qt::Key_Left:     return 123;
        case Qt::Key_Right:    return 124;
        case Qt::Key_Up:       return 126;
        case Qt::Key_Down:     return 125;
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
        default: break;
    }
    // Letter keys A..Z → kVK_ANSI_A..Z table
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        static const int letterMap[] = {
            0,  11, 8,  2,  14, 3,  5,  4,  34, 38, // A..J
            40, 37, 46, 45, 31, 35, 12, 15, 1,  17, // K..T
            32, 9,  13, 7,  16, 6                   // U..Z
        };
        int idx = qtKey - Qt::Key_A;
        return letterMap[idx];
    }
    // Digit keys 0..9 → kVK_ANSI_0..9 table
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        static const int digitMap[] = {
            29, 18, 19, 20, 21, 23, 22, 26, 28, 25  // 0..9
        };
        return digitMap[qtKey - Qt::Key_0];
    }
    return -1;
}

QString labelForKey(int qtKey, const QString &text)
{
    switch (qtKey) {
        case Qt::Key_Space:    return QStringLiteral("Space");
        case Qt::Key_Tab:      return QStringLiteral("Tab");
        case Qt::Key_Return:
        case Qt::Key_Enter:    return QStringLiteral("Return");
        case Qt::Key_Escape:   return QStringLiteral("Esc");
        case Qt::Key_Backspace:return QStringLiteral("Backspace");
        case Qt::Key_Left:     return QStringLiteral("←");
        case Qt::Key_Right:    return QStringLiteral("→");
        case Qt::Key_Up:       return QStringLiteral("↑");
        case Qt::Key_Down:     return QStringLiteral("↓");
        case Qt::Key_F1:  return QStringLiteral("F1");
        case Qt::Key_F2:  return QStringLiteral("F2");
        case Qt::Key_F3:  return QStringLiteral("F3");
        case Qt::Key_F4:  return QStringLiteral("F4");
        case Qt::Key_F5:  return QStringLiteral("F5");
        case Qt::Key_F6:  return QStringLiteral("F6");
        case Qt::Key_F7:  return QStringLiteral("F7");
        case Qt::Key_F8:  return QStringLiteral("F8");
        case Qt::Key_F9:  return QStringLiteral("F9");
        case Qt::Key_F10: return QStringLiteral("F10");
        case Qt::Key_F11: return QStringLiteral("F11");
        case Qt::Key_F12: return QStringLiteral("F12");
        default: break;
    }
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        return QString(QChar('A' + (qtKey - Qt::Key_A)));
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        return QString(QChar('0' + (qtKey - Qt::Key_0)));
    }
    return text.toUpper();
}

QString labelForChord(int carbonMods, const QString &keyLabel)
{
    QStringList parts;
    if (carbonMods & (1 << 12)) parts << QStringLiteral("Ctrl");
    if (carbonMods & (1 << 11)) parts << QStringLiteral("Opt");
    if (carbonMods & (1 << 9))  parts << QStringLiteral("Shift");
    if (carbonMods & (1 << 8))  parts << QStringLiteral("Cmd");
    parts << keyLabel;
    return parts.join(QStringLiteral("+"));
}

int qtModsToCarbonMods(Qt::KeyboardModifiers qm)
{
    int c = 0;
    if (qm & Qt::ShiftModifier)   c |= (1 << 9);
    if (qm & Qt::ControlModifier) c |= (1 << 8);   // Qt::ControlModifier == Cmd on macOS
    if (qm & Qt::AltModifier)     c |= (1 << 11);
    if (qm & Qt::MetaModifier)    c |= (1 << 12);  // Qt::MetaModifier == Ctrl on macOS
    return c;
}

} // unnamed namespace

ChordEdit::ChordEdit(QWidget *parent)
    : QPushButton(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setCheckable(false);
    refreshText();
    connect(this, &QPushButton::clicked, this, &ChordEdit::onClicked);
}

void ChordEdit::setChord(int kVK, int carbonModifiers, const QString &label)
{
    m_keyCode = kVK;
    m_modifiers = carbonModifiers;
    m_label = label;
    refreshText();
}

void ChordEdit::onClicked()
{
    m_capturing = true;
    setText(QStringLiteral("Press a chord…"));
}

void ChordEdit::refreshText()
{
    setText(QStringLiteral("Cycle key: %1").arg(m_label));
}

void ChordEdit::focusOutEvent(QFocusEvent *e)
{
    if (m_capturing) {
        m_capturing = false;
        refreshText();
    }
    QPushButton::focusOutEvent(e);
}

void ChordEdit::keyPressEvent(QKeyEvent *e)
{
    if (!m_capturing) {
        QPushButton::keyPressEvent(e);
        return;
    }
    // Ignore standalone modifier presses — wait for the base key.
    int qk = e->key();
    if (qk == Qt::Key_Shift || qk == Qt::Key_Control ||
        qk == Qt::Key_Alt   || qk == Qt::Key_Meta) {
        return;
    }
    int kvk = qtKeyToKVK(qk);
    if (kvk < 0) return;

    const int carbonMods = qtModsToCarbonMods(e->modifiers());
    if (carbonMods == 0) {
        // A chord without modifiers is unusual; allow it but warn the user.
    }
    m_keyCode = kvk;
    m_modifiers = carbonMods;
    m_label = labelForChord(carbonMods, labelForKey(qk, e->text()));
    m_capturing = false;
    refreshText();
    emit chordChanged(m_keyCode, m_modifiers, m_label);
}

} // namespace ifz
