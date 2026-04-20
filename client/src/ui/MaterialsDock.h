#pragma once

#include <QDockWidget>

// Materials list dock per planning/ui-design/04-client.md §4.3.
// MVP shell: empty placeholder; wiring to SessionState::materialsChanged()
// lands once the state mirror is populated.

namespace griz::ui {

class MaterialsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit MaterialsDock(QWidget *parent = nullptr);
};

} // namespace griz::ui
