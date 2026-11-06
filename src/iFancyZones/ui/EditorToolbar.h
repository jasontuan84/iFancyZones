#pragma once

#include <QWidget>

class QLineEdit;

namespace ifz {

// Floating toolbar of the layout editor: a capsule. On macOS 26 and later
// LayoutEditor puts a Liquid Glass surface behind it (see glassRadius()).
class EditorToolbar : public QWidget {
    Q_OBJECT
public:
    explicit EditorToolbar(QWidget *parent = nullptr);

    void setLayoutName(const QString &name);
    QString currentName() const;   // returns the field's text right now, even
                                   // if the user hasn't pressed Enter yet.

    // Corner radius of the capsule, for the glass surface behind it.
    qreal glassRadius() const { return height() / 2.0; }

signals:
    void newZoneRequested();
    void addGapRequested();
    void reduceGapRequested();
    void saveRequested();
    void cancelRequested();
    void layoutNameEdited(const QString &newName);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QLineEdit *m_nameEdit = nullptr;
    bool m_glass = false;
};

} // namespace ifz
