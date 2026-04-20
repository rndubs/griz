#pragma once

#include <QMainWindow>

// Top-level window per planning/ui-design/04-client.md §3.
// MVP layout (hardcoded defaults; save/restore is post-MVP §13):
//
//     File Edit View Draw Select Animate Window Help
//   +-------+------------------------+--------+
//   | Mat   |                        | Res    |
//   +-------+       Viewport         +--------+
//   | Sel   |                        | Time   |
//   +-------+------------------------+--------+
//   | Console                                 |
//   +-----------------------------------------+
//   | status: connection | fps | host | seq  |
//   +-----------------------------------------+

namespace griz::ui {

class Console;
class MaterialsDock;
class ResultsDock;
class SelectionDock;
class TimeSlider;
class Viewport;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildMenuBar();
    void buildDocks();
    void buildStatusBar();

    Viewport      *m_viewport      = nullptr;
    Console       *m_console       = nullptr;
    MaterialsDock *m_materialsDock = nullptr;
    ResultsDock   *m_resultsDock   = nullptr;
    SelectionDock *m_selectionDock = nullptr;
    TimeSlider    *m_timeSlider    = nullptr;
};

} // namespace griz::ui
