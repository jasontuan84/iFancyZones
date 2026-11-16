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

// Small borderless window centred on one screen: a Liquid Glass disc
// (tinted with the zone color) with the screen's number on it. Before
// macOS 26 the disc is a frosted vibrancy circle instead. Used by the
// "Detect" button: one instance per screen, fades out after a fixed time.
class ScreenIdentifierOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ScreenIdentifierOverlay(int number, const QRect &screenGeometry);

    // Fade in, then fade out and close after `durationMs`.
    void flash(int durationMs);

protected:
    void paintEvent(QPaintEvent *e) override;
    void showEvent(QShowEvent *e) override;

private:
    QRectF discRect() const;
    void fadeTo(qreal opacity, int ms);

    int m_number;
    int m_diameter;
    bool m_native = false;
    bool m_glass = false;
};

} // namespace ifz
