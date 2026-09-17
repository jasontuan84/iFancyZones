#pragma once

#include "platform/mac/AppSwitcherBridge.h"

#include <QList>
#include <QPixmap>
#include <QVector>
#include <QWidget>

class QScreen;
class QVariantAnimation;
class QPropertyAnimation;

namespace ifz {

// Full-screen, translucent overlay that draws a 3D cover-flow carousel of the
// running apps. Purely a display surface: navigation comes from SwitcherInput
// (a CGEventTap), never from this widget's own key handling. The selected
// position is animated for a smooth slide.
class CarouselOverlay : public QWidget {
    Q_OBJECT
public:
    explicit CarouselOverlay(QWidget *parent = nullptr);

    void setApps(const QList<RunningAppInfo> &apps);
    int  count() const { return m_apps.size(); }
    int  selectedIndex() const { return m_selected; }

    // pid of the currently selected app, or -1 if the list is empty.
    qint64 selectedPid() const;

    // Animate (or snap, on wrap) the carousel to `index`.
    void setSelectedIndex(int index);

    // Position the overlay over `screen`, show it, and make the native window
    // an above-everything click-through overlay.
    void showOnScreen(QScreen *screen);
    void hideOverlay();

protected:
    void paintEvent(QPaintEvent *e) override;
    void showEvent(QShowEvent *e) override;

private:
    void configureNative();
    void drawCard(class QPainter &p, int i, qreal d, const QPointF &center);

    // Card sizing depends on the screen height (card height = 2/3 of it), so
    // the per-app card + reflection pixmaps are (re)built whenever the height
    // changes. Pre-rendering keeps per-frame painting to cheap pixmap blits.
    void rebuildCardPixmaps();
    QPixmap renderCard(const RunningAppInfo &app, qreal w, qreal h, qreal dpr,
                       bool focused) const;
    static QPixmap makeReflection(const QPixmap &card);

    QList<RunningAppInfo> m_apps;
    QVector<QPixmap> m_cardPm;        // unfocused (milky) card per app
    QVector<QPixmap> m_cardPmSel;     // focused (solid white) card per app
    QVector<QPixmap> m_reflPm;        // reflection of the unfocused card
    QVector<QPixmap> m_reflPmSel;     // reflection of the focused card
    qreal  m_cardW = 0.0;
    qreal  m_cardH = 0.0;
    int    m_builtForH = -1;         // widget height the pixmaps were built for

    int    m_selected = 0;
    qreal  m_pos = 0.0;          // animated fractional index
    QVariantAnimation *m_anim = nullptr;
    QPropertyAnimation *m_fade = nullptr;   // window opacity fade in/out
    bool   m_nativeConfigured = false;
};

} // namespace ifz
