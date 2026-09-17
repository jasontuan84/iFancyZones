#pragma once

#include <QWidget>

class QLineEdit;

namespace ifz {

class EditorToolbar : public QWidget {
    Q_OBJECT
public:
    explicit EditorToolbar(QWidget *parent = nullptr);

    void setLayoutName(const QString &name);
    QString currentName() const;   // returns the field's text right now, even
                                   // if the user hasn't pressed Enter yet.

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
};

} // namespace ifz
