#pragma once

#include <QWidget>

class QCheckBox;
class QSpinBox;
class QListWidget;
class QLineEdit;

namespace ifz {

class SettingsStore;
class HotkeyEdit;
class ChordEdit;

class SettingsWindow : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWindow(SettingsStore *store, QWidget *parent = nullptr);

private slots:
    void onSave();
    void onAddExclusion();
    void onRemoveExclusion();

private:
    void loadFromStore();

    SettingsStore *m_store;
    HotkeyEdit  *m_hotkey = nullptr;
    ChordEdit   *m_cycleChord = nullptr;
    ChordEdit   *m_switcherChord = nullptr;
    QCheckBox   *m_launchAtLogin = nullptr;
    QSpinBox    *m_defaultGap = nullptr;
    QCheckBox   *m_showNumbers = nullptr;
    QCheckBox   *m_restoreSize = nullptr;
    QListWidget *m_exclusions = nullptr;
    QLineEdit   *m_exclusionInput = nullptr;
};

} // namespace ifz
