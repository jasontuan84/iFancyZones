#include "ScreenLayoutTab.h"

#include "PopoverTheme.h"
#include "PopoverWidgets.h"
#include "core/Layout.h"
#include "platform/mac/AccessibilityBridge.h"
#include "platform/mac/MacScreenInfo.h"
#include "services/SettingsStore.h"
#include "ui/ScreenNumberBadge.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace ifz {

namespace {

// The ⚠ glyph in the banner, drawn rather than shipped as an asset.
class WarnIcon : public QWidget {
public:
    explicit WarnIcon(QWidget *parent) : QWidget(parent) { setFixedSize(20, 20); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(theme::warn(), 1.6);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        QPolygonF tri({QPointF(10, 2.5), QPointF(18.5, 17), QPointF(1.5, 17)});
        p.drawPolygon(tri);
        p.drawLine(QPointF(10, 7.5), QPointF(10, 12));
        p.setBrush(theme::warn());
        p.drawEllipse(QPointF(10, 14.6), 0.9, 0.9);
    }
};

// Card background in the warning tint. Card itself is neutral, so the banner
// paints its own surface.
class WarnCard : public QFrame {
public:
    explicit WarnCard(QWidget *parent) : QFrame(parent) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const QColor w = theme::warn();
        p.setBrush(QColor(w.red(), w.green(), w.blue(), 26));
        p.setPen(QPen(QColor(w.red(), w.green(), w.blue(), 150), 1));
        p.drawRoundedRect(r, theme::cardRadius(), theme::cardRadius());
    }
};

QString describeDisplay(const Monitor &m)
{
    QStringList parts;
    parts << QStringLiteral("%1 × %2").arg(m.geometry.width()).arg(m.geometry.height());
    const int inches = m.diagonalInches();
    if (inches > 0) {
        parts << (m.isUltrawide() ? QObject::tr("%1-inch ultrawide").arg(inches)
                                  : QObject::tr("%1-inch").arg(inches));
    } else if (m.isUltrawide()) {
        parts << QObject::tr("ultrawide");
    }
    return parts.join(QStringLiteral(" · "));
}

} // unnamed namespace

ScreenLayoutTab::ScreenLayoutTab(SettingsStore *store, QWidget *parent)
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

    m_axBanner = buildAxBanner();
    col->addWidget(m_axBanner);
    m_axBanner->hide();

    auto *header = new SectionHeader(tr("Displays"), host);
    auto *detectBtn = new QPushButton(tr("Detect"), host);
    detectBtn->setToolTip(tr("Flash each screen's number on its own display for 3 seconds."));
    connect(detectBtn, &QPushButton::clicked, this, []() {
        const QList<Monitor> monitors = MacScreenInfo::currentMonitors();
        for (const Monitor &m : monitors) {
            if (!m.isValid()) continue;
            auto *ov = new ScreenIdentifierOverlay(m.index, m.availableGeometry);
            ov->flash(3000);
        }
    });
    header->addTrailing(detectBtn);
    col->addWidget(header);

    auto *rowsHost = new QWidget(host);
    m_rowsLayout = new QVBoxLayout(rowsHost);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(6);
    col->addWidget(rowsHost);

    auto *footnote = new QLabel(
        tr("Each display keeps its own layout. Choose “None” to leave a display "
           "unmanaged."), host);
    footnote->setWordWrap(true);
    footnote->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                .arg(theme::textDim().name()));
    col->addWidget(footnote);
    col->addStretch();

    scroll->setWidget(host);
    outer->addWidget(scroll, 1);

    connect(store, &SettingsStore::layoutsChanged, this, &ScreenLayoutTab::reload);
    connect(store, &SettingsStore::bindingsChanged, this, &ScreenLayoutTab::reload);

    // Live-refresh when a monitor is plugged in or out. Bindings are persisted
    // by CGDisplayUnitNumber so they automatically reapply on reconnect.
    QGuiApplication *gapp = qobject_cast<QGuiApplication *>(QGuiApplication::instance());
    if (gapp) {
        connect(gapp, &QGuiApplication::screenAdded,
                this, [this](QScreen*) { reload(); });
        connect(gapp, &QGuiApplication::screenRemoved,
                this, [this](QScreen*) { reload(); });
        connect(gapp, &QGuiApplication::primaryScreenChanged,
                this, [this](QScreen*) { reload(); });
    }
}

QWidget *ScreenLayoutTab::buildAxBanner()
{
    auto *card = new WarnCard(this);
    card->setObjectName(QStringLiteral("axBanner"));
    auto *row = new QHBoxLayout(card);
    row->setContentsMargins(14, 12, 14, 12);
    row->setSpacing(10);

    auto *icon = new WarnIcon(card);
    row->addWidget(icon, 0, Qt::AlignTop);

    auto *col = new QVBoxLayout();
    col->setSpacing(4);

    auto *title = new QLabel(tr("Accessibility access required"), card);
    title->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;")
                             .arg(theme::warn().name()));
    col->addWidget(title);

    m_axDetail = new QLabel(
        tr("iFancyZones cannot move or resize windows until you allow it in "
           "Privacy & Security."), card);
    m_axDetail->setWordWrap(true);
    m_axDetail->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                  .arg(theme::text().name()));
    col->addWidget(m_axDetail);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    auto *allow = new QPushButton(tr("Allow Access…"), card);
    allow->setObjectName(QStringLiteral("primaryWarn"));
    connect(allow, &QPushButton::clicked, this, [this]() {
        if (m_ax) m_ax->requestTrust();
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")));
    });
    buttons->addWidget(allow);

    auto *how = new QPushButton(tr("How it works"), card);
    connect(how, &QPushButton::clicked, this, [this]() {
        m_axDetail->setText(
            tr("macOS gates window control behind Accessibility. Open System "
               "Settings → Privacy & Security → Accessibility, then switch "
               "iFancyZones on. Zones start working immediately, no restart "
               "needed."));
    });
    buttons->addWidget(how);
    buttons->addStretch();
    col->addLayout(buttons);

    row->addLayout(col, 1);
    return card;
}

void ScreenLayoutTab::setAccessibilityBridge(AccessibilityBridge *ax)
{
    m_ax = ax;
    reload();
}

void ScreenLayoutTab::reload()
{
    if (m_ax && m_axBanner) {
        m_axBanner->setVisible(!m_ax->isTrusted());
    }
    rebuildRows();
}

void ScreenLayoutTab::rebuildRows()
{
    while (QLayoutItem *it = m_rowsLayout->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
    m_rows.clear();

    const QList<Monitor> monitors = MacScreenInfo::currentMonitors();
    for (const Monitor &m : monitors) {
        if (!m.isValid()) continue;

        const quint32 key = m.stableKey();
        const QUuid currentId = m_store->bindingFor(key);

        auto *card = new Card(this);
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(10, 10, 10, 10);
        row->setSpacing(10);

        auto *thumb = new LayoutThumbnail(92, 64, card);
        thumb->setMonitorFrame(true);
        thumb->setActive(!currentId.isNull());
        thumb->setLayout(m_store->layoutById(currentId));
        row->addWidget(thumb, 0, Qt::AlignVCenter);

        auto *col = new QVBoxLayout();
        col->setSpacing(4);

        auto *titleRow = new QHBoxLayout();
        titleRow->setSpacing(8);
        const QString displayName =
            m.name.isEmpty() ? tr("Display %1").arg(m.index) : m.name;
        auto *name = new QLabel(displayName, card);
        name->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600;"));
        titleRow->addWidget(name, 0);
        if (m.isMain) titleRow->addWidget(new Tag(tr("Main"), card), 0, Qt::AlignVCenter);
        titleRow->addStretch();
        col->addLayout(titleRow);

        auto *sub = new QLabel(describeDisplay(m), card);
        sub->setWordWrap(true);
        sub->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                               .arg(theme::textDim().name()));
        col->addWidget(sub);

        auto *bindRow = new QHBoxLayout();
        bindRow->setSpacing(8);
        auto *bindLabel = new QLabel(tr("Layout"), card);
        bindLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                     .arg(theme::textDim().name()));
        bindRow->addWidget(bindLabel);

        auto *combo = new QComboBox(card);
        combo->addItem(tr("None"), QString());
        int selectIndex = 0;
        for (const Layout &l : m_store->layouts()) {
            combo->addItem(l.name(), l.id().toString(QUuid::WithoutBraces));
            if (!currentId.isNull() && l.id() == currentId) {
                selectIndex = combo->count() - 1;
            }
        }
        combo->setCurrentIndex(selectIndex);
        bindRow->addWidget(combo);
        bindRow->addStretch();
        col->addLayout(bindRow);

        row->addLayout(col, 1);

        m_rows.append(Row{key, m.name, combo});

        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, combo, key, thumb](int) {
                    const QString idStr = combo->currentData().toString();
                    const QUuid id = idStr.isEmpty() ? QUuid() : QUuid::fromString(idStr);
                    // Update the preview here rather than waiting for the
                    // store's signal, which rebuilds every card.
                    thumb->setActive(!id.isNull());
                    thumb->setLayout(m_store->layoutById(id));
                    m_store->setBinding(key, id);
                });

        m_rowsLayout->addWidget(card);
    }
}

} // namespace ifz
