#pragma once

#include <QMainWindow>
#include <QString>

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

class QLabel;

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

    Console       *console()       const { return m_console; }
    Viewport      *viewport()      const { return m_viewport; }
    MaterialsDock *materialsDock() const { return m_materialsDock; }

public slots:
    void setConnectionStatus(const QString &text);
    void setHostLabel(const QString &text);
    void setStateSeqLabel(quint64 seq);
    void setFpsLabel(double fps);

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

    QLabel        *m_statusConnection = nullptr;
    QLabel        *m_statusFps        = nullptr;
    QLabel        *m_statusHost       = nullptr;
    QLabel        *m_statusSeq        = nullptr;
};

} // namespace griz::ui
