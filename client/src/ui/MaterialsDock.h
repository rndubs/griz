#pragma once

#include <QDockWidget>

#include "model/SessionState.h"

class QTableWidget;
class QTableWidgetItem;

// Materials list dock per planning/ui-design/04-client.md §4.3.
// Table layout: per-material row with two checkboxes (visible / enabled),
// a color swatch, and the id/label text. Toggling a checkbox emits
// commandRequested("vis <id>"|"invis <id>"|"enable <id>"|"disable <id>")
// per Src/interpret.c line 3188.

namespace griz::ui {

class MaterialsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit MaterialsDock(QWidget *parent = nullptr);

public slots:
    void setMaterials(const std::vector<griz::model::Material> &materials);

signals:
    // Emitted when a user-initiated checkbox change needs to be lowered
    // to a Griz command. App wires this to Worker::sendCommand.
    void commandRequested(const QString &cmd);

private slots:
    void onItemChanged(QTableWidgetItem *item);

private:
    QTableWidget *m_table = nullptr;

    // Block `itemChanged` emissions while we repopulate the table in
    // response to a state_changed refresh — otherwise each setCheckState()
    // would echo back a command.
    bool m_suppressEmit = false;
};

} // namespace griz::ui
