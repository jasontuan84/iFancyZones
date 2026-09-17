#include "EditorToolbar.h"

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

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(16, 8, 16, 8);
    row->setSpacing(8);

    auto mkBtn = [this](const QString &text) {
        auto *b = new QPushButton(text, this);
        b->setMinimumHeight(36);
        b->setStyleSheet(
            "QPushButton { background: rgba(50,50,50,220); color: white;"
            " border-radius: 6px; padding: 4px 14px; font-weight: 500; }"
            "QPushButton:hover { background: rgba(80,80,80,240); }"
            "QPushButton:pressed { background: rgba(30,30,30,240); }"
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
        "QLineEdit { background: rgba(20,20,20,160); color: white;"
        " border: 1px solid rgba(255,255,255,60); border-radius: 6px;"
        " padding: 4px 10px; font-weight: 500; min-height: 28px; }"
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
        "QPushButton { background: rgba(60,160,90,230); }"
        "QPushButton:hover { background: rgba(80,180,110,240); }"
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
    QColor bg(30, 30, 30, 220);
    p.setBrush(bg);
    p.setPen(QPen(QColor(255, 255, 255, 60), 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 12, 12);
}

} // namespace ifz
