#pragma once

#include <QPushButton>

namespace ifz {

// A button that captures the next key the user presses and reports its
// macOS virtual keycode (kVK_*). Click → "Press a key…" → captures next key.
class HotkeyEdit : public QPushButton {
    Q_OBJECT
public:
    explicit HotkeyEdit(QWidget *parent = nullptr);

    void setHotkey(int kVKCode, const QString &label);
    int  keyCode() const { return m_keyCode; }
    QString label() const { return m_label; }

signals:
    void hotkeyChanged(int kVKCode, const QString &label);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private slots:
    void onClicked();

private:
    void refreshText();

    int     m_keyCode = 49;            // kVK_Space
    QString m_label   = QStringLiteral("Space");
    bool    m_capturing = false;
};

} // namespace ifz
