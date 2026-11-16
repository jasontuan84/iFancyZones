#include "ScreenNumberBadge.h"

#include "platform/mac/AppKitBridge.h"
#include "ui/PopoverTheme.h"

#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWindow>

namespace ifz {

// ─── ScreenNumberBadge ──────────────────────────────────────────────────

ScreenNumberBadge::ScreenNumberBadge(int number, int diameter, QWidget *parent)
    : QWidget(parent), m_number(number), m_diameter(diameter)
{
    setFixedSize(diameter, diameter);
    setAttribute(Qt::WA_TranslucentBackground);
}

void ScreenNumberBadge::setNumber(int n)
{
    m_number = n;
    update();
}

void ScreenNumberBadge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r(1, 1, m_diameter - 2, m_diameter - 2);
    p.setBrush(theme::zoneColor(240));
    p.setPen(Qt::NoPen);
    p.drawEllipse(r);

    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(qMax(10, m_diameter / 2 - 1));
    p.setFont(f);
    p.drawText(r, Qt::AlignCenter, QString::number(m_number));
}

// ─── ScreenIdentifierOverlay ────────────────────────────────────────────

namespace {

// Room around the disc for the soft shadow painted without glass.
constexpr int kMargin = 12;

} // unnamed namespace

ScreenIdentifierOverlay::ScreenIdentifierOverlay(int number, const QRect &screenGeometry)
    : QWidget(nullptr),
      m_number(number),
      // About a twelfth of the screen height: readable from the desk, but a
      // badge rather than a poster. 120 pt on a 1440 pt tall display.
      m_diameter(qBound(88, screenGeometry.height() / 12, 132)),
      m_glass(AppKitBridge::hasLiquidGlass())
{
    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint
                 | Qt::Tool
                 | Qt::WindowDoesNotAcceptFocus
                 | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);

    const int side = m_diameter + 2 * kMargin;
    QRect r(0, 0, side, side);
    r.moveCenter(screenGeometry.center());
    setGeometry(r);
}

QRectF ScreenIdentifierOverlay::discRect() const
{
    return QRectF(kMargin, kMargin, m_diameter, m_diameter);
}

void ScreenIdentifierOverlay::flash(int durationMs)
{
    setWindowOpacity(0.0);
    show();
    raise();
    // Glass for the disc. Qt's view is above it and paints the number and a
    // hairline rim, so the glass refracts the desktop, not a painted fill.
    AppKitBridge::Glass glass;
    glass.cornerRadius = m_diameter / 2.0;
    glass.tint = theme::zoneColor(110);
    AppKitBridge::setGlassBackdrop(windowHandle(), "disc", discRect(), glass);

    fadeTo(1.0, 160);
    const int fadeOut = 260;
    QTimer::singleShot(qMax(0, durationMs - fadeOut), this, [this, fadeOut]() {
        fadeTo(0.0, fadeOut);
        QTimer::singleShot(fadeOut, this, &QWidget::close);
    });
}

void ScreenIdentifierOverlay::fadeTo(qreal opacity, int ms)
{
    auto *anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(ms);
    anim->setStartValue(windowOpacity());
    anim->setEndValue(opacity);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ScreenIdentifierOverlay::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (!m_native) {
        if (QWindow *w = windowHandle()) {
            AppKitBridge::configureAsOverlay(w);
            AppKitBridge::useDarkAppearance(w);
            m_native = true;
        }
    }
}

void ScreenIdentifierOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QRectF disc = discRect();

    if (!m_glass) {
        // Before macOS 26 the backdrop is plain frosted vibrancy: add a soft
        // shadow and the zone color so the disc reads the same way.
        p.setPen(Qt::NoPen);
        for (int i = kMargin; i > 0; i -= 3) {
            p.setBrush(QColor(0, 0, 0, 10));
            p.drawEllipse(disc.adjusted(-i + 4, -i + 6, i - 4, i - 2));
        }
        p.setBrush(theme::zoneColor(150));
        p.drawEllipse(disc);
    }

    // Hairline rim, the edge light of glass.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, m_glass ? 90 : 140), 1.0));
    p.drawEllipse(disc.adjusted(0.5, 0.5, -0.5, -0.5));

    QFont f = p.font();
    f.setWeight(QFont::DemiBold);
    f.setPixelSize(int(m_diameter * 0.46));
    p.setFont(f);
    const QString text = QString::number(m_number);

    // Soft shadow under the digit keeps it legible over bright desktops.
    p.setPen(QColor(0, 0, 0, 70));
    p.drawText(disc.translated(0, 1.5), Qt::AlignCenter, text);
    p.setPen(Qt::white);
    p.drawText(disc, Qt::AlignCenter, text);
}

} // namespace ifz
