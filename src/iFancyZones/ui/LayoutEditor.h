#pragma once

#include "core/Layout.h"

#include <QPointer>
#include <QVector>
#include <QWidget>

namespace ifz {

class EditorToolbar;
class ZoneEditorWidget;

// Full-screen editor for a single Layout. Opens on the screen where the
// popover was anchored. Hosts:
//  - A dim translucent backdrop covering the screen.
//  - A floating toolbar at top-center.
//  - One ZoneEditorWidget per zone in the layout.
class LayoutEditor : public QWidget {
    Q_OBJECT
public:
    LayoutEditor(const Layout &initial, bool createNew, QWidget *parent = nullptr);

    void open();   // sizes, positions, shows fullscreen on a chosen screen
    void cancel();

signals:
    void saved(const Layout &layout);
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

private slots:
    void onNewZone();
    void onAddGap();
    void onReduceGap();
    void onSave();
    void onZoneChanged();
    void onZoneSelected();
    void onZoneDeleteRequested();

private:
    QRect canvasRect() const;     // rectangle inside this widget where zones live
    void rebuildZoneWidgets();
    void raiseToolbar();          // keep the floating toolbar above all zones
    void selectZone(ZoneEditorWidget *w);
    int indexOf(ZoneEditorWidget *w) const;

    Layout m_layout;
    bool m_isNew;
    QPointer<EditorToolbar> m_toolbar;
    QVector<ZoneEditorWidget *> m_zones;
    ZoneEditorWidget *m_selected = nullptr;
};

} // namespace ifz
