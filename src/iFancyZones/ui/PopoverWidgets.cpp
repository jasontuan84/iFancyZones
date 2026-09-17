#include "PopoverWidgets.h"

#include "PopoverTheme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace ifz {

namespace {

// The hairline that trails a section title.
class Rule : public QWidget {
public:
    explicit Rule(QWidget *parent) : QWidget(parent) { setFixedHeight(1); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), theme::rule());
    }
};

QFont smallCapsFont(const QWidget *w)
{
    QFont f = w->font();
    f.setPointSizeF(f.pointSizeF() - 2.0);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    f.setWeight(QFont::DemiBold);
    return f;
}

} // unnamed namespace

// ---------------------------------------------------------------- SectionHeader

SectionHeader::SectionHeader(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 6, 0, 2);
    row->setSpacing(8);

    m_title = new QLabel(title.toUpper(), this);
    m_title->setFont(smallCapsFont(this));
    m_title->setStyleSheet(QStringLiteral("color: %1;").arg(theme::textDim().name()));
    row->addWidget(m_title);

    m_count = new QLabel(this);
    m_count->setFont(smallCapsFont(this));
    m_count->setStyleSheet(QStringLiteral("color: %1;").arg(theme::textFaint().name()));
    m_count->hide();
    row->addWidget(m_count);

    row->addWidget(new Rule(this), 1);
}

void SectionHeader::setCount(int n)
{
    m_count->setText(QString::number(n));
    m_count->setVisible(n > 0);
}

void SectionHeader::addTrailing(QWidget *w)
{
    w->setParent(this);
    w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout()->addWidget(w);
}

// ------------------------------------------------------------------------ Card

Card::Card(QWidget *parent) : QFrame(parent)
{
    setAttribute(Qt::WA_Hover);
}

void Card::setInteractive(bool on, std::function<void()> onClick)
{
    m_interactive = on;
    m_onClick = std::move(onClick);
    setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void Card::setPlain(bool on)
{
    if (m_plain == on) return;
    m_plain = on;
    update();
}

void Card::setSelected(bool on)
{
    if (m_selected == on) return;
    m_selected = on;
    update();
}

void Card::enterEvent(QEnterEvent *e)
{
    QFrame::enterEvent(e);
    if (!m_interactive) return;
    m_hover = true;
    update();
    emit hoverChanged(true);
}

void Card::leaveEvent(QEvent *e)
{
    QFrame::leaveEvent(e);
    if (!m_interactive) return;
    m_hover = false;
    update();
    emit hoverChanged(false);
}

void Card::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_interactive && e->button() == Qt::LeftButton
        && rect().contains(e->position().toPoint()) && m_onClick) {
        m_onClick();
    }
    QFrame::mouseReleaseEvent(e);
}

void Card::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal rad = theme::cardRadius();

    if (m_selected) {
        p.setBrush(theme::accentFill());
        p.setPen(QPen(theme::accent(), 1.5));
    } else if (m_plain) {
        if (!m_hover) return;
        p.setBrush(theme::cardHover());
        p.setPen(Qt::NoPen);
    } else {
        p.setBrush(m_hover ? theme::cardHover() : theme::card());
        p.setPen(QPen(theme::cardBorder(), 1));
    }
    p.drawRoundedRect(r, rad, rad);
}

// -------------------------------------------------------------- LayoutThumbnail

LayoutThumbnail::LayoutThumbnail(int w, int h, QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(w, h);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void LayoutThumbnail::setLayout(const Layout &l)
{
    m_layout = l;
    update();
}

void LayoutThumbnail::setActive(bool on)
{
    if (m_active == on) return;
    m_active = on;
    update();
}

void LayoutThumbnail::setMonitorFrame(bool on)
{
    if (m_monitorFrame == on) return;
    m_monitorFrame = on;
    update();
}

void LayoutThumbnail::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF screen = QRectF(rect());
    if (m_monitorFrame) {
        // Leave room under the panel for the stand.
        screen.setHeight(screen.height() - 8);
        p.setPen(QPen(theme::textFaint(), 1.5));
        p.setBrush(QColor(0, 0, 0, 60));
        p.drawRoundedRect(screen.adjusted(0.75, 0.75, -0.75, -0.75), 3, 3);

        // Stand: a short neck centred under the panel.
        QRectF neck(screen.center().x() - 9, screen.bottom() + 3, 18, 2.5);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::textFaint());
        p.drawRoundedRect(neck, 1.25, 1.25);

        screen = screen.adjusted(4, 4, -4, -4);
    }

    theme::paintLayoutPreview(&p, screen, m_layout,
                              m_active ? theme::accent() : QColor(255, 255, 255, 80));
}

// ------------------------------------------------------------------ StatusPill

StatusPill::StatusPill(const QString &text, QWidget *parent)
    : QWidget(parent), m_text(text)
{
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() - 1.0);
    f.setWeight(QFont::DemiBold);
    setFont(f);
    const int w = QFontMetrics(f).horizontalAdvance(m_text) + 32;
    setFixedSize(w, 22);
}

void StatusPill::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(theme::running().red(), theme::running().green(),
                      theme::running().blue(), 38));
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);

    p.setBrush(theme::running());
    p.drawEllipse(QPointF(r.left() + 11, r.center().y()), 3.2, 3.2);

    p.setPen(theme::running().lighter(115));
    p.drawText(r.adjusted(20, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, m_text);
}

// ------------------------------------------------------------------------- Tag

Tag::Tag(const QString &text, QWidget *parent)
    : QWidget(parent), m_text(text.toUpper())
{
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() - 2.5);
    f.setWeight(QFont::DemiBold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    setFont(f);
    const int w = QFontMetrics(f).horizontalAdvance(m_text) + 14;
    setFixedSize(w, 17);
}

void Tag::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 30));
    p.drawRoundedRect(r, 4, 4);
    p.setPen(theme::textDim());
    p.drawText(r, Qt::AlignCenter, m_text);
}

// ------------------------------------------------------------- SegmentedControl

SegmentedControl::SegmentedControl(const QStringList &titles, QWidget *parent)
    : QWidget(parent), m_titles(titles)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(34);
}

void SegmentedControl::setCurrentIndex(int i)
{
    if (i < 0 || i >= m_titles.size() || i == m_index) return;
    m_index = i;
    update();
    emit currentIndexChanged(i);
}

int SegmentedControl::segmentAt(const QPoint &pos) const
{
    if (m_titles.isEmpty() || !rect().contains(pos)) return -1;
    const int seg = width() / m_titles.size();
    if (seg <= 0) return -1;
    return qMin(pos.x() / seg, m_titles.size() - 1);
}

void SegmentedControl::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    const int i = segmentAt(e->position().toPoint());
    if (i >= 0) setCurrentIndex(i);
}

void SegmentedControl::mouseMoveEvent(QMouseEvent *e)
{
    const int i = segmentAt(e->position().toPoint());
    if (i == m_hover) return;
    m_hover = i;
    update();
}

void SegmentedControl::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

void SegmentedControl::paintEvent(QPaintEvent *)
{
    if (m_titles.isEmpty()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(theme::cardBorder(), 1));
    p.setBrush(theme::card());
    p.drawRoundedRect(track, 9, 9);

    const qreal segW = track.width() / m_titles.size();
    for (int i = 0; i < m_titles.size(); ++i) {
        QRectF seg(track.left() + i * segW, track.top(), segW, track.height());
        const bool selected = (i == m_index);
        if (selected || i == m_hover) {
            QRectF fill = seg.adjusted(3, 3, -3, -3);
            p.setPen(Qt::NoPen);
            p.setBrush(selected ? QColor(255, 255, 255, 46) : QColor(255, 255, 255, 20));
            p.drawRoundedRect(fill, 7, 7);
        }
        QFont f = font();
        f.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        p.setFont(f);
        p.setPen(selected ? theme::text() : theme::textDim());
        p.drawText(seg, Qt::AlignCenter, m_titles.at(i));
    }
}

} // namespace ifz
