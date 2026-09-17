#include "CarouselOverlay.h"

#include "platform/mac/AppKitBridge.h"

#include <QFontMetricsF>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QVariantAnimation>
#include <QVector>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace ifz {

namespace {
// Cover Flow geometry. Cards are portrait 3:4 and sized to 2/3 of the screen
// height. The centre cover faces the viewer flat and pops forward; neighbours
// rotate about the vertical axis and stack tighter the further out they sit.
constexpr qreal kCardHeightFrac = 1.0 / 3.0;  // card height / screen height
constexpr qreal kCardAspect     = 3.0 / 4.0;  // width / height (3:4)
constexpr qreal kMaxAngle       = 66.0;       // degrees side covers rotate to
constexpr qreal kCenterScale    = 1.0;
constexpr qreal kSideScale      = 0.78;
constexpr qreal kReflectFrac    = 0.32;       // reflection height / card height
constexpr int   kWindow         = 6;          // covers drawn each side of centre
}

CarouselOverlay::CarouselOverlay(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint
                 | Qt::Tool
                 | Qt::WindowDoesNotAcceptFocus
                 | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(220);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { m_pos = v.toReal(); update(); });

    // Fade in/out of the whole overlay window.
    m_fade = new QPropertyAnimation(this, "windowOpacity", this);
    m_fade->setDuration(160);
    m_fade->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_fade, &QPropertyAnimation::finished, this, [this]() {
        if (windowOpacity() <= 0.01) QWidget::hide();
    });
}

void CarouselOverlay::setApps(const QList<RunningAppInfo> &apps)
{
    m_apps = apps;
    m_selected = 0;
    m_pos = 0.0;
    m_anim->stop();
    m_cardPm.clear();
    m_cardPmSel.clear();
    m_reflPm.clear();
    m_reflPmSel.clear();
    m_builtForH = -1;   // force a rebuild on next paint
    update();
}

qint64 CarouselOverlay::selectedPid() const
{
    if (m_selected < 0 || m_selected >= m_apps.size()) return -1;
    return m_apps[m_selected].pid;
}

void CarouselOverlay::setSelectedIndex(int index)
{
    if (m_apps.isEmpty()) return;
    const int n = m_apps.size();
    index = ((index % n) + n) % n;     // wrap
    if (index == m_selected) return;
    m_selected = index;

    m_anim->stop();
    // A wrap-around jump (last -> first) would sweep the whole strip; snap
    // instead so only adjacent steps animate.
    if (std::abs(qreal(index) - m_pos) > 1.5) {
        m_pos = index;
        update();
    } else {
        m_anim->setStartValue(m_pos);
        m_anim->setEndValue(qreal(index));
        m_anim->start();
    }
}

void CarouselOverlay::showOnScreen(QScreen *screen)
{
    if (!screen) screen = QGuiApplication::primaryScreen();
    // Full screen (cover the menu bar / Dock too) for an immersive backdrop.
    if (screen) setGeometry(screen->geometry());

    m_fade->stop();
    setWindowOpacity(0.0);
    show();
    raise();
    configureNative();
    update();

    m_fade->setStartValue(windowOpacity());
    m_fade->setEndValue(1.0);
    m_fade->start();
}

void CarouselOverlay::hideOverlay()
{
    if (!isVisible()) return;
    // Fade out, then hide() once opacity reaches 0 (see finished handler).
    m_fade->stop();
    m_fade->setStartValue(windowOpacity());
    m_fade->setEndValue(0.0);
    m_fade->start();
}

void CarouselOverlay::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    configureNative();
}

void CarouselOverlay::configureNative()
{
    if (m_nativeConfigured) return;
    if (QWindow *w = windowHandle()) {
        AppKitBridge::configureAsOverlay(w);
        // NOTE: no NSVisualEffectView here — as a subview of Qt's content view
        // it would paint OVER the carousel. The translucent backdrop is drawn
        // by paintEvent() instead.
        m_nativeConfigured = true;
    }
}

QPixmap CarouselOverlay::renderCard(const RunningAppInfo &app,
                                    qreal w, qreal h, qreal dpr,
                                    bool focused) const
{
    QPixmap pm(int(w * dpr), int(h * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    const qreal radius = w * 0.07;
    QRectF card(0, 0, w, h);
    card.adjust(2, 2, -2, -2);   // leave room for the border stroke

    // Focused card is solid white so it stands out; the rest are a milky,
    // translucent white.
    const QColor fill = focused ? QColor(255, 255, 255, 255)
                                : QColor(244, 244, 247, 200);
    QPainterPath plate;
    plate.addRoundedRect(card, radius, radius);
    p.fillPath(plate, fill);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, focused ? 255 : 200),
                  std::max(2.0, w * 0.012)));
    p.drawRoundedRect(card, radius, radius);

    // Large app icon, centred in the upper portion.
    const qreal iconSide = w * 0.58;
    QRectF iconRect(0, 0, iconSide, iconSide);
    iconRect.moveCenter(QPointF(card.center().x(), card.top() + h * 0.40));
    if (!app.icon.isNull()) {
        p.drawPixmap(iconRect, app.icon, QRectF(app.icon.rect()));
    }

    // App name below the icon, black, scaled to the card, wrapped/elided.
    QFont f = p.font();
    f.setPointSizeF(std::max(15.0, h * 0.052));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(20, 20, 22));

    const int flags = Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap;
    QRectF nameRect(card.left() + w * 0.06, iconRect.bottom() + h * 0.03,
                    card.width() - w * 0.12,
                    card.bottom() - iconRect.bottom() - h * 0.06);
    QString name = app.name;
    QFontMetricsF fm(f);
    const QRectF needed = fm.boundingRect(nameRect, flags, name);
    if (needed.height() > nameRect.height()) {
        name = fm.elidedText(name, Qt::ElideRight, int(nameRect.width() * 2.0));
    }
    p.drawText(nameRect, flags, name);

    p.end();
    return pm;
}

QPixmap CarouselOverlay::makeReflection(const QPixmap &card)
{
    if (card.isNull()) return QPixmap();
    // Mirror vertically, then fade out downward via a destination-in gradient.
    QImage img = card.toImage().mirrored(false, true);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    QLinearGradient g(0, 0, 0, img.height());
    g.setColorAt(0.0, QColor(0, 0, 0, 150));   // strongest at the card's foot
    g.setColorAt(0.45, QColor(0, 0, 0, 30));
    g.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(img.rect(), g);
    p.end();

    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(card.devicePixelRatio());
    return pm;
}

void CarouselOverlay::rebuildCardPixmaps()
{
    m_cardH = height() * kCardHeightFrac;
    m_cardW = m_cardH * kCardAspect;
    const qreal dpr = devicePixelRatioF() > 0 ? devicePixelRatioF() : 1.0;

    m_cardPm.resize(m_apps.size());
    m_cardPmSel.resize(m_apps.size());
    m_reflPm.resize(m_apps.size());
    m_reflPmSel.resize(m_apps.size());
    for (int i = 0; i < m_apps.size(); ++i) {
        m_cardPm[i]    = renderCard(m_apps[i], m_cardW, m_cardH, dpr, false);
        m_cardPmSel[i] = renderCard(m_apps[i], m_cardW, m_cardH, dpr, true);
        m_reflPm[i]    = makeReflection(m_cardPm[i]);
        m_reflPmSel[i] = makeReflection(m_cardPmSel[i]);
    }
    m_builtForH = height();
}

void CarouselOverlay::drawCard(QPainter &p, int i, qreal d, const QPointF &center)
{
    if (i < 0 || i >= m_cardPm.size() || m_cardPm[i].isNull()) return;

    const bool focused = (i == m_selected);
    const QPixmap &cardPm = focused ? m_cardPmSel[i] : m_cardPm[i];
    const QPixmap &reflPm = focused ? m_reflPmSel[i] : m_reflPm[i];

    const qreal sgn     = (d > 0) ? 1.0 : (d < 0 ? -1.0 : 0.0);
    const qreal ad      = std::abs(d);
    const qreal clamped = std::clamp(d, -1.0, 1.0);

    // Spacing scales with card width so the row fills the screen width.
    const qreal centerGap   = m_cardW * 0.10;
    const qreal sideSpacing = m_cardW * 0.42;

    qreal x;
    if (ad <= 1.0) {
        x = center.x() + d * (m_cardW * 0.5 + centerGap);
    } else {
        x = center.x() + sgn * (m_cardW * 0.5 + centerGap)
          + (d - sgn) * sideSpacing;
    }
    const qreal angle   = -clamped * kMaxAngle;
    const qreal scale   = (ad <= 1.0)
                            ? kSideScale + (1.0 - ad) * (kCenterScale - kSideScale)
                            : kSideScale;
    const qreal opacity = std::max(0.30, 1.0 - 0.14 * ad);

    p.save();
    QTransform t;
    t.translate(x, center.y());
    t.rotate(angle, Qt::YAxis);
    t.scale(scale, scale);
    p.setTransform(t, /*combine=*/false);

    const QRectF cardRect(-m_cardW / 2.0, -m_cardH / 2.0, m_cardW, m_cardH);

    // Reflection first (sits just below the card's foot), then the card.
    if (!reflPm.isNull()) {
        const qreal reflH = m_cardH * kReflectFrac;
        const QRectF reflSrc(0, 0, reflPm.width(), reflPm.height() * kReflectFrac);
        const QRectF reflDst(-m_cardW / 2.0, m_cardH / 2.0 + m_cardH * 0.012,
                             m_cardW, reflH);
        p.setOpacity(opacity * 0.55);
        p.drawPixmap(reflDst, reflPm, reflSrc);
    }

    p.setOpacity(opacity);
    p.drawPixmap(cardRect, cardPm, QRectF(cardPm.rect()));

    p.restore();
}

void CarouselOverlay::paintEvent(QPaintEvent *)
{
    if (m_apps.isEmpty()) return;
    if (m_builtForH != height() || m_cardPm.size() != m_apps.size()) {
        rebuildCardPixmaps();
    }

    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // Translucent dark backdrop, full screen, no panel around the cards.
    p.fillRect(rect(), QColor(0, 0, 0, 140));

    // Vertical centre chosen so the tall card plus its reflection fit on
    // screen (reflection extends below the card's foot).
    const QPointF center(width() / 2.0,
                         height() * 0.5 - m_cardH * kReflectFrac * 0.5);

    // Draw farthest covers first so the centre lands on top.
    QVector<int> order;
    for (int i = 0; i < m_apps.size(); ++i) {
        if (std::abs(qreal(i) - m_pos) <= kWindow) order.append(i);
    }
    std::sort(order.begin(), order.end(), [this](int a, int b) {
        return std::abs(qreal(a) - m_pos) > std::abs(qreal(b) - m_pos);
    });
    for (int i : order) {
        drawCard(p, i, qreal(i) - m_pos, center);
    }
}

} // namespace ifz
