#include "SettingsWindow.h"

#include "ChordEdit.h"
#include "HotkeyEdit.h"
#include "services/AppSettings.h"
#include "services/SettingsStore.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ifz {

SettingsWindow::SettingsWindow(SettingsStore *store, QWidget *parent)
    : QWidget(parent), m_store(store)
{
    setWindowTitle(tr("iFancyZones Settings"));
    setMinimumSize(480, 460);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(20, 20, 20, 20);
    outer->setSpacing(14);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    m_hotkey = new HotkeyEdit(this);
    form->addRow(tr("Snap activation key:"), m_hotkey);

    m_cycleChord = new ChordEdit(this);
    form->addRow(tr("Cycle window in zone:"), m_cycleChord);

    m_switcherChord = new ChordEdit(this);
    form->addRow(tr("App switcher (3D carousel):"), m_switcherChord);

    m_launchAtLogin = new QCheckBox(tr("Launch at login"), this);
    form->addRow(QString(), m_launchAtLogin);

    m_defaultGap = new QSpinBox(this);
    m_defaultGap->setRange(0, 64);
    m_defaultGap->setSuffix(QStringLiteral(" px"));
    form->addRow(tr("Default zone gap:"), m_defaultGap);

    m_showNumbers = new QCheckBox(tr("Show zone numbers during snap preview"), this);
    form->addRow(QString(), m_showNumbers);

    m_restoreSize = new QCheckBox(tr("Restore original size when unsnapping"), this);
    form->addRow(QString(), m_restoreSize);

    // Zone color: a swatch that opens the color picker, plus a reset to the
    // default green. The choice applies on Save.
    auto *colorRow = new QHBoxLayout();
    colorRow->setSpacing(8);
    m_zoneColorBtn = new QPushButton(this);
    m_zoneColorBtn->setFixedSize(56, 24);
    m_zoneColorBtn->setCursor(Qt::PointingHandCursor);
    m_zoneColorBtn->setToolTip(tr("Choose the zone color"));
    colorRow->addWidget(m_zoneColorBtn);
    auto *resetColorBtn = new QPushButton(tr("Reset"), this);
    colorRow->addWidget(resetColorBtn);
    colorRow->addStretch(1);
    form->addRow(tr("Zone color:"), colorRow);

    connect(m_zoneColorBtn, &QPushButton::clicked, this, [this]() {
        const QColor c = QColorDialog::getColor(m_zoneColor, this, tr("Zone color"));
        if (c.isValid()) setPendingZoneColor(c);
    });
    connect(resetColorBtn, &QPushButton::clicked, this, [this]() {
        setPendingZoneColor(AppSettings::defaultZoneColor());
    });

    outer->addLayout(form);

    // TODO (v1.1): Re-enable the "Excluded applications" section. Hidden for
    // v1 because WindowManager doesn't yet honour the exclusion list at snap
    // time; surfacing the UI now would mislead users into thinking it works.
    // The QListWidget / QLineEdit members are kept (as nullptr) so persisted
    // settings round-trip unchanged.
    m_exclusions = nullptr;
    m_exclusionInput = nullptr;
    outer->addStretch(1);

    // ── About / Author ─────────────────────────────────────────────
    // Bordered card: icon on the left, four label lines on the right. The
    // whole block is wrapped in a framed container and every label inside is
    // rendered in white.
    auto *aboutFrame = new QFrame(this);
    aboutFrame->setObjectName(QStringLiteral("aboutFrame"));
    aboutFrame->setStyleSheet(QStringLiteral(
        "QFrame#aboutFrame {"
        "  border: 1px solid rgba(255, 255, 255, 0.35);"
        "  border-radius: 8px;"
        "}"));

    auto *aboutOuter = new QHBoxLayout(aboutFrame);
    aboutOuter->setContentsMargins(14, 12, 14, 12);
    aboutOuter->setSpacing(14);

    auto *iconLabel = new QLabel(this);
    QPixmap appPm(QStringLiteral(":/icons/app.png"));
    if (!appPm.isNull()) {
        // Scale for the backing store so the icon stays sharp on Retina.
        const qreal dpr = devicePixelRatioF();
        QPixmap scaled = appPm.scaled(QSize(64, 64) * dpr,
            Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        iconLabel->setPixmap(scaled);
    }
    iconLabel->setFixedSize(64, 64);
    iconLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    aboutOuter->addWidget(iconLabel, 0, Qt::AlignTop);

    auto *aboutLayout = new QVBoxLayout();
    aboutLayout->setContentsMargins(0, 0, 0, 0);
    aboutLayout->setSpacing(2);

    const QString kTransparent =
        QStringLiteral("background: transparent; color: white; border: none;");

    auto *appLine = new QLabel(
        tr("<b>%1</b>").arg(qApp->applicationName().isEmpty() ?
                            QStringLiteral("iFancyZones") : qApp->applicationName()),
        this);
    QFont nameFont = appLine->font();
    nameFont.setPointSize(nameFont.pointSize() + 2);
    appLine->setFont(nameFont);
    appLine->setStyleSheet(kTransparent);
    aboutLayout->addWidget(appLine);

    auto *versionLine = new QLabel(
        tr("Version %1").arg(qApp->applicationVersion().isEmpty() ?
                             QStringLiteral("1.0.0") : qApp->applicationVersion()),
        this);
    versionLine->setStyleSheet(kTransparent);
    aboutLayout->addWidget(versionLine);

    auto *authorLine = new QLabel(
        tr("Author: <b>Jason</b> · "
           "<a href=\"https://tuanquynh.com\">tuanquynh.com</a>"),
        this);
    authorLine->setOpenExternalLinks(true);
    authorLine->setTextInteractionFlags(Qt::TextBrowserInteraction);
    authorLine->setStyleSheet(kTransparent);
    aboutLayout->addWidget(authorLine);

    auto *copyrightLine = new QLabel(tr("© 2026 Jason. All rights reserved."), this);
    copyrightLine->setStyleSheet(kTransparent);
    aboutLayout->addWidget(copyrightLine);

    aboutOuter->addLayout(aboutLayout, 1);
    outer->addWidget(aboutFrame);

    auto *footer = new QHBoxLayout();
    footer->addStretch();
    auto *saveBtn = new QPushButton(tr("Save"), this);
    saveBtn->setDefault(true);
    footer->addWidget(saveBtn);
    outer->addLayout(footer);

    connect(saveBtn,   &QPushButton::clicked, this, &SettingsWindow::onSave);

    loadFromStore();
}

void SettingsWindow::loadFromStore()
{
    const AppSettings &s = m_store->settings();
    m_hotkey->setHotkey(s.activationKeyCode, s.activationKeyLabel);
    m_cycleChord->setChord(s.cycleFocusKeyCode, s.cycleFocusModifiers, s.cycleFocusLabel);
    m_switcherChord->setChord(s.switcherKeyCode, s.switcherModifiers, s.switcherLabel);
    m_launchAtLogin->setChecked(s.launchAtLogin);
    m_defaultGap->setValue(s.defaultGap);
    m_showNumbers->setChecked(s.showZoneNumbers);
    m_restoreSize->setChecked(s.restoreSizeOnUnsnap);
    setPendingZoneColor(s.zoneColor);
    // Exclusions: hidden in v1; values still round-trip through AppSettings.
}

void SettingsWindow::setPendingZoneColor(const QColor &c)
{
    m_zoneColor = c;
    m_zoneColorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid rgba(255, 255, 255, 0.5);"
        " border-radius: 4px; }").arg(c.name(QColor::HexRgb)));
    m_zoneColorBtn->setAccessibleName(tr("Zone color %1").arg(c.name(QColor::HexRgb)));
}

void SettingsWindow::onAddExclusion()    { /* TODO v1.1: exclusion UI hidden */ }
void SettingsWindow::onRemoveExclusion() { /* TODO v1.1: exclusion UI hidden */ }

void SettingsWindow::onSave()
{
    AppSettings s = m_store->settings();
    s.activationKeyCode = m_hotkey->keyCode();
    s.activationKeyLabel = m_hotkey->label();
    s.cycleFocusKeyCode = m_cycleChord->keyCode();
    s.cycleFocusModifiers = m_cycleChord->modifiers();
    s.cycleFocusLabel = m_cycleChord->label();
    s.switcherKeyCode = m_switcherChord->keyCode();
    s.switcherModifiers = m_switcherChord->modifiers();
    s.switcherLabel = m_switcherChord->label();
    s.launchAtLogin = m_launchAtLogin->isChecked();
    s.defaultGap = m_defaultGap->value();
    s.showZoneNumbers = m_showNumbers->isChecked();
    s.restoreSizeOnUnsnap = m_restoreSize->isChecked();
    s.zoneColor = m_zoneColor;
    // Exclusions are not edited in v1; keep whatever was already persisted.
    m_store->setSettings(s);
    close();
}

} // namespace ifz
