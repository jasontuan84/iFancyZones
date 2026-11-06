#include "EditorToolbar.h"

#include "platform/mac/AppKitBridge.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>

namespace ifz {

EditorToolbar::EditorToolbar(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedHeight(56);
    m_glass = AppKitBridge::hasLiquidGlass();

    // Capsule controls, concentric with the capsule toolbar: a 36 pt control
    // inside a 10 pt margin has radius 18 = 28 - 10.
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(10, 10, 10, 10);
    row->setSpacing(8);

    auto mkBtn = [this](const QString &text) {
        auto *b = new QPushButton(text, this);
        b->setFixedHeight(36);
        b->setStyleSheet(
            "QPushButton { background: rgba(255,255,255,34); color: white;"
            " border: 1px solid rgba(255,255,255,40);"
            " border-radius: 18px; padding: 4px 16px; font-weight: 500; }"
            "QPushButton:hover { background: rgba(255,255,255,58); }"
            "QPushButton:pressed { background: rgba(255,255,255,20); }"
        );
        return b;
    };

    auto *bNew    = mkBtn(tr("+ New Zone"));

    // Inline editable layout name. Single click puts focus in the field;
    // pressing Enter or losing focus commits the new name.
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setMinimumWidth(180);
    m_nameEdit->setPlaceholderText(tr("Layout name"));
    m_nameEdit->setStyleSheet(
        "QLineEdit { background: rgba(0,0,0,70); color: white;"
        " border: 1px solid rgba(255,255,255,50); border-radius: 18px;"
        " padding: 4px 14px; font-weight: 500; min-height: 26px; }"
        "QLineEdit:focus { border: 1px solid rgba(120,180,255,200); }"
    );
    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this]() {
        emit layoutNameEdited(m_nameEdit->text().trimmed());
    });

    auto *bAddGap = mkBtn(tr("▶ Gap"));
    auto *bSubGap = mkBtn(tr("◀ Gap"));
    auto *bSave   = mkBtn(tr("Save"));
    auto *bCancel = mkBtn(tr("Cancel"));

    bSave->setStyleSheet(bSave->styleSheet() +
        "QPushButton { background: rgba(60,160,90,210); border: 0; }"
        "QPushButton:hover { background: rgba(80,180,110,235); }"
    );

    row->addWidget(bNew);
    row->addSpacing(6);
    row->addWidget(m_nameEdit, 1);
    row->addSpacing(8);
    row->addWidget(bSubGap);
    row->addWidget(bAddGap);
    row->addSpacing(16);
    row->addWidget(bSave);
    row->addWidget(bCancel);

    connect(bNew,    &QPushButton::clicked, this, &EditorToolbar::newZoneRequested);
    connect(bAddGap, &QPushButton::clicked, this, &EditorToolbar::addGapRequested);
    connect(bSubGap, &QPushButton::clicked, this, &EditorToolbar::reduceGapRequested);
    connect(bSave,   &QPushButton::clicked, this, &EditorToolbar::saveRequested);
    connect(bCancel, &QPushButton::clicked, this, &EditorToolbar::cancelRequested);
}

void EditorToolbar::setLayoutName(const QString &name)
{
    if (!m_nameEdit) return;
    if (m_nameEdit->text() == name) return;
    QSignalBlocker block(m_nameEdit);
    m_nameEdit->setText(name);
}

QString EditorToolbar::currentName() const
{
    return m_nameEdit ? m_nameEdit->text().trimmed() : QString();
}

void EditorToolbar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal rad = r.height() / 2;
    // Over glass, only a light veil: it keeps the controls legible when a
    // zone is dragged under the toolbar. Without glass, a solid dark capsule.
    p.setBrush(m_glass ? QColor(16, 18, 26, 70) : QColor(30, 30, 30, 220));
    p.setPen(m_glass ? Qt::NoPen : QPen(QColor(255, 255, 255, 60), 1));
    p.drawRoundedRect(r, rad, rad);
}

} // namespace ifz
