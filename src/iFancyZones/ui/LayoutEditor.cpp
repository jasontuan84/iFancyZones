#include "LayoutEditor.h"

#include "EditorToolbar.h"
#include "ZoneEditorWidget.h"
#include "core/LayoutEngine.h"
#include "platform/mac/AppKitBridge.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>

namespace ifz {

LayoutEditor::LayoutEditor(const Layout &initial, bool createNew, QWidget *parent)
    : QWidget(parent), m_layout(initial), m_isNew(createNew)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);

    m_toolbar = new EditorToolbar(this);
    connect(m_toolbar.data(), &EditorToolbar::newZoneRequested, this, &LayoutEditor::onNewZone);
    connect(m_toolbar.data(), &EditorToolbar::addGapRequested, this, &LayoutEditor::onAddGap);
    connect(m_toolbar.data(), &EditorToolbar::reduceGapRequested, this, &LayoutEditor::onReduceGap);
    connect(m_toolbar.data(), &EditorToolbar::saveRequested, this, &LayoutEditor::onSave);
    connect(m_toolbar.data(), &EditorToolbar::cancelRequested, this, &LayoutEditor::cancel);
    connect(m_toolbar.data(), &EditorToolbar::layoutNameEdited, this, [this](const QString &n) {
        m_layout.setName(n);
    });
}

void LayoutEditor::open()
{
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    setGeometry(screen->geometry());
    // Lock to full-screen: the backdrop is not movable/resizable, only the
    // zones inside it are interactive.
    setFixedSize(screen->geometry().size());

    // Force native window creation so AppKitBridge can configure the NSWindow.
    winId();
    if (QWindow *qw = windowHandle()) {
        AppKitBridge::configureAsEditor(qw);
    }

    m_toolbar->setLayoutName(m_layout.name());
    rebuildZoneWidgets();

    // Toolbar: floating top-center, slightly wider to fit name editor + buttons.
    const QSize tbHint(780, 56);
    m_toolbar->resize(tbHint);
    m_toolbar->move((width() - tbHint.width()) / 2, 24);

    show();
    raise();
    activateWindow();
    setFocus();
}

void LayoutEditor::cancel()
{
    emit cancelled();
    close();
}

QRect LayoutEditor::canvasRect() const
{
    // The whole screen is the canvas: zones can be dragged/dropped anywhere and
    // alignment presets are measured from the very top edge. The toolbar floats
    // on top of this area (see raiseToolbar()) rather than carving space out of it.
    return rect();
}

void LayoutEditor::rebuildZoneWidgets()
{
    for (ZoneEditorWidget *w : m_zones) w->deleteLater();
    m_zones.clear();
    m_selected = nullptr;

    const QRect canvas = canvasRect();
    const auto &zones = m_layout.zones();
    for (int i = 0; i < zones.size(); ++i) {
        QRect r = LayoutEngine::zoneToPixelsRaw(zones[i], canvas);
        auto *w = new ZoneEditorWidget(i, canvas, this);
        w->setGeometry(r);
        w->setGap(m_layout.gap());
        connect(w, &ZoneEditorWidget::deleteRequested,
                this, &LayoutEditor::onZoneDeleteRequested);
        connect(w, &ZoneEditorWidget::selected, this, &LayoutEditor::onZoneSelected);
        connect(w, &ZoneEditorWidget::changed, this, &LayoutEditor::onZoneChanged);
        w->show();
        m_zones.append(w);
    }

    if (m_zones.isEmpty()) {
        onNewZone();
    } else {
        selectZone(m_zones.back());
    }
}

int LayoutEditor::indexOf(ZoneEditorWidget *w) const
{
    for (int i = 0; i < m_zones.size(); ++i) if (m_zones[i] == w) return i;
    return -1;
}

void LayoutEditor::raiseToolbar()
{
    if (m_toolbar) m_toolbar->raise();
}

void LayoutEditor::selectZone(ZoneEditorWidget *w)
{
    if (m_selected == w) return;
    if (m_selected) m_selected->setSelected(false);
    m_selected = w;
    if (m_selected) {
        m_selected->setSelected(true);
        m_selected->raise();
    }
    // The selected zone was just raised above its siblings; keep the toolbar
    // above everything so its controls are never hidden behind a zone.
    raiseToolbar();
}

void LayoutEditor::onZoneSelected()
{
    selectZone(qobject_cast<ZoneEditorWidget *>(sender()));
}

void LayoutEditor::onZoneChanged()
{
    // Updated rect will be read back at Save time.
}

void LayoutEditor::onZoneDeleteRequested()
{
    auto *w = qobject_cast<ZoneEditorWidget *>(sender());
    if (!w) return;
    int idx = indexOf(w);
    if (idx < 0) return;
    m_zones.removeAt(idx);
    if (m_selected == w) m_selected = nullptr;
    w->deleteLater();
    for (int i = 0; i < m_zones.size(); ++i) {
        m_zones[i]->setIndex(i);
    }
    if (!m_zones.isEmpty()) selectZone(m_zones.back());
}

void LayoutEditor::onNewZone()
{
    const QRect canvas = canvasRect();
    QRect r(canvas.center().x() - 200, canvas.center().y() - 150, 400, 300);
    auto *w = new ZoneEditorWidget(m_zones.size(), canvas, this);
    w->setGeometry(r);
    w->setGap(m_layout.gap());
    connect(w, &ZoneEditorWidget::deleteRequested,
            this, &LayoutEditor::onZoneDeleteRequested);
    connect(w, &ZoneEditorWidget::selected, this, &LayoutEditor::onZoneSelected);
    connect(w, &ZoneEditorWidget::changed, this, &LayoutEditor::onZoneChanged);
    w->show();
    m_zones.append(w);
    selectZone(w);
}

void LayoutEditor::onAddGap()
{
    m_layout.setGap(m_layout.gap() + 4);
    for (ZoneEditorWidget *w : m_zones) w->setGap(m_layout.gap());
    update();
}

void LayoutEditor::onReduceGap()
{
    m_layout.setGap(m_layout.gap() - 4);
    for (ZoneEditorWidget *w : m_zones) w->setGap(m_layout.gap());
    update();
}

void LayoutEditor::onSave()
{
    // Force-commit the toolbar's name field — user might have typed a new
    // name but never pressed Enter (so editingFinished hasn't fired yet).
    if (m_toolbar) {
        const QString currentName = m_toolbar->currentName();
        if (!currentName.isEmpty()) {
            m_layout.setName(currentName);
        }
    }

    const QRect canvas = canvasRect();
    m_layout.clearZones();
    for (ZoneEditorWidget *w : m_zones) {
        QRectF norm = LayoutEngine::pixelsToNormalized(w->geometry(), canvas);
        m_layout.addZone(Zone(QUuid::createUuid(), norm));
    }
    emit saved(m_layout);
    close();
}

void LayoutEditor::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        cancel();
        return;
    }
    QWidget::keyPressEvent(e);
}

void LayoutEditor::mousePressEvent(QMouseEvent *e)
{
    // Click on empty backdrop: deselect.
    Q_UNUSED(e);
    if (m_selected) {
        m_selected->setSelected(false);
        m_selected = nullptr;
    }
}

void LayoutEditor::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 110));
}

} // namespace ifz
