#pragma once

#include <QMainWindow>
#include <QString>
#include <QTimer>

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

class QDockWidget;
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
    ResultsDock   *resultsDock()   const { return m_resultsDock; }
    SelectionDock *selectionDock() const { return m_selectionDock; }
    TimeSlider    *timeSlider()    const { return m_timeSlider; }

public slots:
    void setConnectionStatus(const QString &text);
    void flashDisconnect(const QString &text);
    void setHostLabel(const QString &text);
    void setStateSeqLabel(quint64 seq);
    void setFpsLabel(double fps);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onOpenDatabase();
    void onRunScript();

private:
    void buildMenuBar();
    void buildDocks();
    void buildStatusBar();
    void restoreLayout();
    void saveLayout();

    // Dispatch a Griz command through the console so menu clicks show up
    // in the command history (design-doc invariant I1: there is only one
    // command path).
    void runCommand(const QString &cmd);

    Viewport      *m_viewport      = nullptr;
    Console       *m_console       = nullptr;
    MaterialsDock *m_materialsDock = nullptr;
    ResultsDock   *m_resultsDock   = nullptr;
    SelectionDock *m_selectionDock = nullptr;
    TimeSlider    *m_timeSlider    = nullptr;
    QDockWidget   *m_consoleDock   = nullptr;

    QLabel        *m_statusConnection = nullptr;
    QLabel        *m_statusFps        = nullptr;
    QLabel        *m_statusHost       = nullptr;
    QLabel        *m_statusSeq        = nullptr;

    // Transient blink applied to m_statusConnection on unexpected
    // disconnect so it grabs attention. After the blink burst the label
    // stays in its "disconnected" red style until the next reconnect.
    QTimer         m_flashTimer;
    int            m_flashTicks = 0;
};

} // namespace griz::ui
