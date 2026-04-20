#pragma once

#include <QDockWidget>

// Selection table dock per planning/ui-design/04-client.md §7.

namespace griz::ui {

class SelectionDock : public QDockWidget {
    Q_OBJECT
public:
    explicit SelectionDock(QWidget *parent = nullptr);
};

} // namespace griz::ui
