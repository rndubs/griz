#pragma once

#include <QDockWidget>

#include "model/SessionState.h"

class QListWidget;

// Materials list dock per planning/ui-design/04-client.md §4.3.
// MVP: shows a list of materials with id + label + visibility/enabled flags.
// Wires to SessionState::materialsChanged() once a q_state response lands.

namespace griz::ui {

class MaterialsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit MaterialsDock(QWidget *parent = nullptr);

public slots:
    void setMaterials(const std::vector<griz::model::Material> &materials);

private:
    QListWidget *m_list = nullptr;
};

} // namespace griz::ui
