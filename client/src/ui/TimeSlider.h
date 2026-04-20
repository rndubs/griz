#pragma once

#include <QDockWidget>

// Time-state scrubber dock per planning/ui-design/04-client.md §4.3.

namespace griz::ui {

class TimeSlider : public QDockWidget {
    Q_OBJECT
public:
    explicit TimeSlider(QWidget *parent = nullptr);
};

} // namespace griz::ui
