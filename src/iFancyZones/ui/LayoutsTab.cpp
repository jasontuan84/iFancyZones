#include "LayoutsTab.h"

#include "PopoverTheme.h"
#include "PopoverWidgets.h"
#include "core/Layout.h"
#include "platform/mac/MacScreenInfo.h"
#include "services/SettingsStore.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace ifz {

namespace {

struct TemplateSpec {
    const char *name;
    QVector<QRectF> zones;
};

// The starting points offered above the user's own layouts. Normalized rects,
// so they scale to whatever display the layout is later bound to.
QVector<TemplateSpec> templateSpecs()
{
    return {
        {QT_TRANSLATE_NOOP("LayoutsTab", "Focus"),
         {QRectF(0.15, 0.10, 0.70, 0.80)}},
        {QT_TRANSLATE_NOOP("LayoutsTab", "Columns"),
         {QRectF(0, 0, 1.0 / 3, 1), QRectF(1.0 / 3, 0, 1.0 / 3, 1),
          QRectF(2.0 / 3, 0, 1.0 / 3, 1)}},
        {QT_TRANSLATE_NOOP("LayoutsTab", "Rows"),
         {QRectF(0, 0, 1, 1.0 / 3), QRectF(0, 1.0 / 3, 1, 1.0 / 3),
          QRectF(0, 2.0 / 3, 1, 1.0 / 3)}},
        {QT_TRANSLATE_NOOP("LayoutsTab", "Grid"),
         {QRectF(0, 0, 0.5, 0.5), QRectF(0.5, 0, 0.5, 0.5),
          QRectF(0, 0.5, 0.5, 0.5), QRectF(0.5, 0.5, 0.5, 0.5)}},
        {QT_TRANSLATE_NOOP("LayoutsTab", "Priority"),
         {QRectF(0, 0, 0.25, 1), QRectF(0.25, 0, 0.75, 1)}},
    };
}

Layout layoutFromZones(const QString &name, const QVector<QRectF> &zones)
{
    Layout l(QUuid::createUuid(), name, LayoutKind::Custom);
    for (const QRectF &r : zones) l.addZone(Zone(QUuid::createUuid(), r));
    return l;
}

// The magnifier drawn inside the search field's left padding.
class SearchIcon : public QWidget {
public:
    explicit SearchIcon(QWidget *parent) : QWidget(parent)
    {
        setFixedSize(16, 16);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(theme::textDim(), 1.4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(6.5, 6.5), 4.5, 4.5);
        p.drawLine(QPointF(10, 10), QPointF(14, 14));
    }
};

} // unnamed namespace

LayoutsTab::LayoutsTab(SettingsStore *store, QWidget *parent)
    : QWidget(parent), m_store(store)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *host = new QWidget(scroll);
    host->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *col = new QVBoxLayout(host);
    col->setContentsMargins(0, 2, 2, 4);
    col->setSpacing(6);

    m_search = new QLineEdit(host);
    m_search->setPlaceholderText(tr("Search layouts"));
    m_search->setClearButtonEnabled(true);
    auto *icon = new SearchIcon(m_search);
    icon->move(9, 9);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_filter = t.trimmed();
        rebuildRows();
    });
    col->addWidget(m_search);

    buildTemplates(host, col);

    m_myHeader = new SectionHeader(tr("My layouts"), host);
    col->addWidget(m_myHeader);

    auto *rowsHost = new QWidget(host);
    m_rowsLayout = new QVBoxLayout(rowsHost);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(2);
    col->addWidget(rowsHost);
    col->addStretch();

    scroll->setWidget(host);
    outer->addWidget(scroll, 1);

    connect(store, &SettingsStore::layoutsChanged, this, &LayoutsTab::reload);
    connect(store, &SettingsStore::bindingsChanged, this, &LayoutsTab::reload);
}

void LayoutsTab::buildTemplates(QWidget *host, QVBoxLayout *col)
{
    col->addWidget(new SectionHeader(tr("Templates"), host));

    auto *strip = new QWidget(host);
    auto *row = new QHBoxLayout(strip);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(5);

    for (const TemplateSpec &spec : templateSpecs()) {
        const QString name = tr(spec.name);
        auto *card = new Card(strip);
        card->setToolTip(tr("Start a new layout from the %1 template").arg(name));
        auto *cardCol = new QVBoxLayout(card);
        cardCol->setContentsMargins(4, 7, 4, 6);
        cardCol->setSpacing(5);

        auto *thumb = new LayoutThumbnail(42, 28, card);
        thumb->setLayout(layoutFromZones(name, spec.zones));
        cardCol->addWidget(thumb, 0, Qt::AlignHCenter);

        auto *label = new QLabel(name, card);
        label->setAlignment(Qt::AlignHCenter);
        label->setStyleSheet(QStringLiteral("font-size: 11px;"));
        cardCol->addWidget(label);

        const QVector<QRectF> zones = spec.zones;
        card->setInteractive(true, [this, name, zones]() {
            emit templateRequested(name, zones);
        });
        row->addWidget(card, 1);
    }

    col->addWidget(strip);
}

void LayoutsTab::reload()
{
    rebuildRows();
}

QHash<QUuid, QString> LayoutsTab::boundDisplayNames() const
{
    QHash<QUuid, QString> names;
    const QList<Monitor> monitors = MacScreenInfo::currentMonitors();
    for (const Monitor &m : monitors) {
        if (!m.isValid()) continue;
        const QUuid id = m_store->bindingFor(m.stableKey());
        if (id.isNull()) continue;
        const QString display =
            m.name.isEmpty() ? tr("Display %1").arg(m.index) : m.name;
        // A layout bound to several displays lists the first one found.
        if (!names.contains(id)) names.insert(id, display);
    }
    return names;
}

void LayoutsTab::rebuildRows()
{
    while (QLayoutItem *it = m_rowsLayout->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }

    const QHash<QUuid, QString> inUse = boundDisplayNames();
    int shown = 0;

    for (const Layout &l : m_store->layouts()) {
        if (!m_filter.isEmpty()
            && !l.name().contains(m_filter, Qt::CaseInsensitive)) {
            continue;
        }
        ++shown;

        const QUuid lid = l.id();
        const bool active = inUse.contains(lid);

        auto *card = new Card(this);
        card->setPlain(true);
        card->setSelected(active);
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(10, 8, 10, 8);
        row->setSpacing(12);

        auto *thumb = new LayoutThumbnail(52, 34, card);
        thumb->setLayout(l);
        thumb->setActive(active);
        row->addWidget(thumb, 0, Qt::AlignVCenter);

        auto *col = new QVBoxLayout();
        col->setSpacing(2);
        auto *name = new QLabel(l.name(), card);
        name->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600;"));
        col->addWidget(name);

        const int zoneCount = l.zones().size();
        QString subtitle = zoneCount == 0
                               ? tr("not configured")
                               : (zoneCount == 1 ? tr("1 zone")
                                                 : tr("%1 zones").arg(zoneCount));
        if (active) {
            subtitle += tr(" · in use on %1").arg(inUse.value(lid));
        }
        auto *sub = new QLabel(subtitle, card);
        sub->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                               .arg(theme::textDim().name()));
        col->addWidget(sub);
        row->addLayout(col, 1);

        // Edit / more appear on hover, and the check marks the active layout.
        auto *editBtn = new QPushButton(tr("✎  Edit"), card);
        editBtn->setEnabled(!l.isBuiltIn());
        editBtn->setToolTip(l.isBuiltIn() ? tr("Built-in layouts are read-only")
                                          : tr("Edit this layout's zones"));
        connect(editBtn, &QPushButton::clicked, this, [this, lid]() {
            emit editLayoutRequested(lid);
        });
        editBtn->hide();
        row->addWidget(editBtn);

        auto *moreBtn = new QPushButton(QStringLiteral("···"), card);
        moreBtn->setFixedWidth(32);
        connect(moreBtn, &QPushButton::clicked, this, [this, lid, l, moreBtn]() {
            QMenu menu(this);
            menu.addAction(tr("Duplicate"), this, [this, lid]() {
                emit duplicateLayoutRequested(lid);
            });
            QAction *del = menu.addAction(tr("Delete"), this, [this, lid]() {
                emit deleteLayoutRequested(lid);
            });
            del->setEnabled(!l.isBuiltIn());
            menu.exec(moreBtn->mapToGlobal(QPoint(0, moreBtn->height() + 4)));
        });
        moreBtn->hide();
        row->addWidget(moreBtn);

        auto *check = new QLabel(QStringLiteral("✓"), card);
        check->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: 600;")
                                 .arg(theme::accent().name()));
        check->setVisible(active);
        row->addWidget(check);

        card->setInteractive(true, {});
        connect(card, &Card::hoverChanged, this,
                [editBtn, moreBtn, check, active](bool hovered) {
                    editBtn->setVisible(hovered);
                    moreBtn->setVisible(hovered);
                    // The check and the buttons share the same slot on the right.
                    check->setVisible(active && !hovered);
                });

        m_rowsLayout->addWidget(card);
    }

    // "New layout" always closes the list, as the last row.
    if (m_filter.isEmpty()) {
        auto *card = new Card(this);
        card->setPlain(true);
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(10, 8, 10, 8);
        row->setSpacing(12);

        auto *thumb = new LayoutThumbnail(52, 34, card);
        thumb->setLayout(Layout()); // no zones: draws the dashed placeholder
        row->addWidget(thumb, 0, Qt::AlignVCenter);

        auto *col = new QVBoxLayout();
        col->setSpacing(2);
        auto *name = new QLabel(tr("New layout"), card);
        name->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600;"));
        col->addWidget(name);
        auto *sub = new QLabel(tr("1 zone · not configured"), card);
        sub->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                               .arg(theme::textDim().name()));
        col->addWidget(sub);
        row->addLayout(col, 1);

        card->setInteractive(true, [this]() { emit newLayoutRequested(); });
        m_rowsLayout->addWidget(card);
    }

    m_myHeader->setCount(shown);
}

} // namespace ifz
