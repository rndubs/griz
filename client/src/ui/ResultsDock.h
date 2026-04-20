#pragma once

#include <QDockWidget>

// Results field/component picker dock per planning/ui-design/04-client.md §4.3.

namespace griz::ui {

class ResultsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit ResultsDock(QWidget *parent = nullptr);
};

} // namespace griz::ui
