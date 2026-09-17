#pragma once

#include "core/Layout.h"

#include <QFrame>
#include <QWidget>

#include <functional>

class QLabel;

namespace ifz {

// "DISPLAYS ──────────────  [trailing]": small caps label, a hairline filling
// the remaining width, and an optional widget parked on the right.
class SectionHeader : public QWidget {
    Q_OBJECT
public:
    explicit SectionHeader(const QString &title, QWidget *parent = nullptr);

    // Extra text after the title, dimmer: the "8" in "MY LAYOUTS  8".
    void setCount(int n);
    void addTrailing(QWidget *w);

private:
    QLabel *m_title;
    QLabel *m_count;
};

// Rounded translucent surface. Set `interactive` for hover feedback and a
// click callback; `selected` paints the blue active treatment.
class Card : public QFrame {
    Q_OBJECT
public:
    explicit Card(QWidget *parent = nullptr);

    void setInteractive(bool on, std::function<void()> onClick = {});
    // List rows carry no chrome of their own; only hover and the active state
    // draw a surface. Cards in a gallery stay outlined at rest.
    void setPlain(bool on);
    void setSelected(bool on);
    bool isSelected() const { return m_selected; }
    bool isHovered() const { return m_hover; }

signals:
    void hoverChanged(bool hovered);

protected:
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private:
    std::function<void()> m_onClick;
    bool m_interactive = false;
    bool m_plain = false;
    bool m_selected = false;
    bool m_hover = false;
};

// Scaled preview of a layout's zones. Optionally drawn inside a monitor
// outline with a stand, which is how displays are shown in the Displays tab.
class LayoutThumbnail : public QWidget {
    Q_OBJECT
public:
    explicit LayoutThumbnail(int w, int h, QWidget *parent = nullptr);

    void setLayout(const Layout &l);
    void setActive(bool on);      // blue zones instead of grey
    void setMonitorFrame(bool on);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    Layout m_layout;
    bool m_active = false;
    bool m_monitorFrame = false;
};

// "● Running" status pill.
class StatusPill : public QWidget {
    Q_OBJECT
public:
    explicit StatusPill(const QString &text, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QString m_text;
};

// Small uppercase tag, e.g. MAIN.
class Tag : public QWidget {
    Q_OBJECT
public:
    explicit Tag(const QString &text, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QString m_text;
};

// Two-segment control that drives a QStackedWidget.
class SegmentedControl : public QWidget {
    Q_OBJECT
public:
    explicit SegmentedControl(const QStringList &titles, QWidget *parent = nullptr);

    int currentIndex() const { return m_index; }
    void setCurrentIndex(int i);

signals:
    void currentIndexChanged(int index);

protected:
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private:
    int segmentAt(const QPoint &p) const;

    QStringList m_titles;
    int m_index = 0;
    int m_hover = -1;
};

} // namespace ifz
