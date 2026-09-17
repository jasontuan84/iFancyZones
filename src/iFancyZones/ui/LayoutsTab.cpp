#include "LayoutsTab.h"

#include "core/Layout.h"
#include "services/SettingsStore.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace ifz {

LayoutsTab::LayoutsTab(SettingsStore *store, QWidget *parent)
    : QWidget(parent), m_store(store)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *host = new QWidget(scroll);
    m_rowsLayout = new QVBoxLayout(host);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(6);
    m_rowsLayout->addStretch();
    scroll->setWidget(host);
    outer->addWidget(scroll, 1);

    auto *footer = new QHBoxLayout();
    footer->addStretch();
    auto *newBtn = new QPushButton(tr("+ New"), this);
    newBtn->setDefault(true);
    connect(newBtn, &QPushButton::clicked, this, &LayoutsTab::newLayoutRequested);
    footer->addWidget(newBtn);
    outer->addLayout(footer);

    connect(store, &SettingsStore::layoutsChanged, this, &LayoutsTab::reload);
}

void LayoutsTab::reload()
{
    rebuildRows();
}

void LayoutsTab::rebuildRows()
{
    while (QLayoutItem *it = m_rowsLayout->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }

    bool sawCustomHeader = false;
    for (const Layout &l : m_store->layouts()) {
        if (!l.isBuiltIn() && !sawCustomHeader) {
            auto *sep = new QLabel(tr("Custom"), this);
            sep->setStyleSheet("color: palette(mid); padding-top: 6px;");
            m_rowsLayout->addWidget(sep);
            sawCustomHeader = true;
        }
        if (l.isBuiltIn() && m_rowsLayout->count() == 0) {
            auto *sep = new QLabel(tr("Built-in"), this);
            sep->setStyleSheet("color: palette(mid);");
            m_rowsLayout->addWidget(sep);
        }

        auto *rowW = new QWidget(this);
        auto *row = new QHBoxLayout(rowW);
        row->setContentsMargins(0, 0, 0, 0);

        auto *name = new QLabel(l.name(), rowW);
        name->setMinimumWidth(220);
        row->addWidget(name, 1);

        auto *editBtn = new QPushButton(tr("Edit"), rowW);
        editBtn->setEnabled(!l.isBuiltIn());
        QUuid lid = l.id();
        connect(editBtn, &QPushButton::clicked, this, [this, lid]() {
            emit editLayoutRequested(lid);
        });
        row->addWidget(editBtn);

        auto *dupBtn = new QPushButton(tr("Duplicate"), rowW);
        connect(dupBtn, &QPushButton::clicked, this, [this, lid]() {
            emit duplicateLayoutRequested(lid);
        });
        row->addWidget(dupBtn);

        auto *delBtn = new QPushButton(tr("Delete"), rowW);
        delBtn->setEnabled(!l.isBuiltIn());
        connect(delBtn, &QPushButton::clicked, this, [this, lid]() {
            emit deleteLayoutRequested(lid);
        });
        row->addWidget(delBtn);

        m_rowsLayout->addWidget(rowW);
    }
    m_rowsLayout->addStretch();
}

} // namespace ifz
