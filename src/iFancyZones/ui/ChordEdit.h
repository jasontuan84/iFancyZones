#pragma once

#include <QPushButton>

namespace ifz {

// Captures a "chord" — modifier mask (in Carbon's bit positions) + base
// keycode (kVK_*). Click → "Press a chord…" → first key+modifier press is
// captured. Examples: Option+Tab, Cmd+Shift+`.
class ChordEdit : public QPushButton {
    Q_OBJECT
public:
    explicit ChordEdit(QWidget *parent = nullptr);

    void setChord(int kVK, int carbonModifiers, const QString &label);
    int  keyCode() const { return m_keyCode; }
    int  modifiers() const { return m_modifiers; }
    QString label() const { return m_label; }

signals:
    void chordChanged(int kVK, int carbonModifiers, const QString &label);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private slots:
    void onClicked();

private:
    void refreshText();

    int     m_keyCode = 48;            // kVK_Tab
    int     m_modifiers = 1 << 11;     // optionKey
    QString m_label = QStringLiteral("Opt+Tab");
    bool    m_capturing = false;
};

} // namespace ifz
