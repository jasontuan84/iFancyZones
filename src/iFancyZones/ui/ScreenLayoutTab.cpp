#include "ScreenLayoutTab.h"

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
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace ifz {

ScreenLayoutTab::ScreenLayoutTab(SettingsStore *store, QWidget *parent)
    : QWidget(parent), m_store(store)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);

    // Accessibility-missing banner (hidden until setAccessibilityBridge() detects it).
    auto *bannerRow = new QHBoxLayout();
    m_axBanner = new QLabel(this);
    m_axBanner->setWordWrap(true);
    m_axBanner->setStyleSheet(
        "QLabel { background: rgba(255, 180, 60, 35); color: #B25900;"
        " padding: 8px 12px; border: 1px solid rgba(255, 180, 60, 120);"
        " border-radius: 6px; }");
    m_axBanner->setText(
        tr("iFancyZones needs Accessibility access to move windows. "
           "Open System Settings → Privacy & Security → Accessibility and enable iFancyZones."));
    auto *openSettingsBtn = new QPushButton(tr("Open Settings"), this);
    openSettingsBtn->setObjectName(QStringLiteral("openAxSettings"));
    connect(openSettingsBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")));
    });
    bannerRow->addWidget(m_axBanner, 1);
    bannerRow->addWidget(openSettingsBtn);
    outer->addLayout(bannerRow);
    m_axBanner->hide();
    openSettingsBtn->hide();
    openSettingsBtn->setProperty("isAxBannerButton", true);

    auto *introRow = new QHBoxLayout();
    auto *intro = new QLabel(tr("Bind a layout to each screen. Choose “None” to leave a screen unmanaged."), this);
    intro->setWordWrap(true);
    intro->setStyleSheet("color: #FFFFFF;");
    introRow->addWidget(intro, 1);

    auto *detectBtn = new QPushButton(tr("Detect"), this);
    detectBtn->setToolTip(tr("Flash each screen's number on its own display for 3 seconds."));
    connect(detectBtn, &QPushButton::clicked, this, [this]() {
        const QList<Monitor> monitors = MacScreenInfo::currentMonitors();
        for (const Monitor &m : monitors) {
            if (!m.isValid()) continue;
            // Center the overlay on each screen's available geometry.
            auto *ov = new ScreenIdentifierOverlay(m.index, m.availableGeometry);
            ov->flash(3000);
        }
    });
    introRow->addWidget(detectBtn, 0);
    outer->addLayout(introRow);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *host = new QWidget(scroll);
    m_rowsLayout = new QVBoxLayout(host);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(8);
    m_rowsLayout->addStretch();
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

void ScreenLayoutTab::setAccessibilityBridge(AccessibilityBridge *ax)
{
    m_ax = ax;
    reload();
}

void ScreenLayoutTab::reload()
{
    if (m_ax && m_axBanner) {
        const bool show = !m_ax->isTrusted();
        m_axBanner->setVisible(show);
        // Find the sibling Open Settings button by object name and toggle it too.
        for (QObject *o : children()) {
            if (auto *b = qobject_cast<QPushButton *>(o)) {
                if (b->objectName() == QStringLiteral("openAxSettings")) {
                    b->setVisible(show);
                }
            }
        }
    }
    rebuildRows();
}

void ScreenLayoutTab::rebuildRows()
{
    // Clear existing rows.
    while (QLayoutItem *it = m_rowsLayout->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
    m_rows.clear();

    const QList<Monitor> monitors = MacScreenInfo::currentMonitors();
    qDebug() << "[iFancyZones] ScreenLayoutTab::rebuildRows: received"
             << monitors.size() << "monitors";
    for (const Monitor &m : monitors) {
        qDebug() << "[iFancyZones]   monitor" << m.index
                 << "valid=" << m.isValid()
                 << "unitNumber=" << m.unitNumber
                 << "name=" << m.name
                 << "geom=" << m.geometry;
    }
    for (const Monitor &m : monitors) {
        if (!m.isValid()) continue;

        auto *rowW = new QWidget(this);
        auto *row = new QHBoxLayout(rowW);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);

        auto *badge = new ScreenNumberBadge(m.index, 26, rowW);
        row->addWidget(badge, 0);

        QString labelText = m.name.isEmpty() ? tr("Display %1").arg(m.index) : m.name;
        auto *label = new QLabel(labelText, rowW);
        label->setMinimumWidth(200);
        row->addWidget(label, 1);

        const quint32 key = m.stableKey();
        auto *combo = new QComboBox(rowW);
        combo->addItem(tr("None"), QString());
        const QUuid currentId = m_store->bindingFor(key);
        int selectIndex = 0;
        for (const Layout &l : m_store->layouts()) {
            QString idStr = l.id().toString(QUuid::WithoutBraces);
            combo->addItem(l.name(), idStr);
            if (l.id() == currentId && !currentId.isNull()) {
                selectIndex = combo->count() - 1;
            }
        }
        combo->setCurrentIndex(selectIndex);
        row->addWidget(combo);

        Row r{key, m.name, combo};
        m_rows.append(r);

        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, combo, key](int idx) {
                    Q_UNUSED(idx);
                    QString idStr = combo->currentData().toString();
                    QUuid id = idStr.isEmpty() ? QUuid() : QUuid::fromString(idStr);
                    m_store->setBinding(key, id);
                });

        m_rowsLayout->addWidget(rowW);
    }

    m_rowsLayout->addStretch();
}

void ScreenLayoutTab::onComboChanged(int /*index*/, quint32 /*unitNumber*/)
{
    // (Wiring done inline in rebuildRows; this method is kept for future use.)
}

} // namespace ifz
