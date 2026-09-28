#pragma once

#include <QWidget>

namespace ifz {

// Small zone-colored circle with a white number inside. Used as the
// monitor-index badge at the start of each row in the Screen & Layout tab.
class ScreenNumberBadge : public QWidget {
    Q_OBJECT
public:
    explicit ScreenNumberBadge(int number = 0,
                               int diameter = 26,
                               QWidget *parent = nullptr);

    void setNumber(int n);
    int  number() const { return m_number; }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    int m_number = 0;
    int m_diameter = 26;
};

// Full-screen borderless overlay that paints a giant number in the centre
// of a single screen. Used by the "Detect" button: one instance per screen,
// auto-closes after a fixed duration.
class ScreenIdentifierOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ScreenIdentifierOverlay(int number, const QRect &screenGeometry);

    // Show, raise, and auto-close after `durationMs`.
    void flash(int durationMs);

protected:
    void paintEvent(QPaintEvent *e) override;
    void showEvent(QShowEvent *e) override;

private:
    int m_number;
    bool m_native = false;
};

} // namespace ifz
