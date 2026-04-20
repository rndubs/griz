#include "MainWindow.h"

#include "Console.h"
#include "MaterialsDock.h"
#include "ResultsDock.h"
#include "SelectionDock.h"
#include "TimeSlider.h"
#include "Viewport.h"

#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>

namespace griz::ui {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("Griz"));
    resize(1280, 800);

    m_viewport = new Viewport(this);
    setCentralWidget(m_viewport);

    buildMenuBar();
    buildDocks();
    buildStatusBar();
}

void MainWindow::buildMenuBar() {
    auto *mb = menuBar();
    // Menu actions are deliberately empty in the scaffold — see
    // planning/ui-design/04-client.md §2.3 and planning/ui-design/08-feature-parity.md
    // for the per-item wiring plan. The top-level structure mirrors the current
    // Motif build so muscle memory survives.
    mb->addMenu(tr("&File"));
    mb->addMenu(tr("&Edit"));
    mb->addMenu(tr("&View"));
    mb->addMenu(tr("&Draw"));
    mb->addMenu(tr("&Select"));
    mb->addMenu(tr("&Animate"));
    mb->addMenu(tr("&Window"));
    mb->addMenu(tr("&Help"));
}

void MainWindow::buildDocks() {
    m_materialsDock = new MaterialsDock(this);
    m_selectionDock = new SelectionDock(this);
    m_resultsDock   = new ResultsDock(this);
    m_timeSlider    = new TimeSlider(this);

    addDockWidget(Qt::LeftDockWidgetArea,  m_materialsDock);
    addDockWidget(Qt::RightDockWidgetArea, m_resultsDock);
    splitDockWidget(m_materialsDock, m_selectionDock, Qt::Vertical);
    splitDockWidget(m_resultsDock,   m_timeSlider,    Qt::Vertical);

    m_console = new Console(this);
    auto *consoleDock = new QDockWidget(tr("Console"), this);
    consoleDock->setObjectName(QStringLiteral("ConsoleDock"));
    consoleDock->setWidget(m_console);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
}

void MainWindow::buildStatusBar() {
    auto *sb = statusBar();
    sb->addPermanentWidget(new QLabel(tr("disconnected"), this));
    sb->addPermanentWidget(new QLabel(tr("—"), this)); // FPS
    sb->addPermanentWidget(new QLabel(tr("local"), this)); // host
    sb->addPermanentWidget(new QLabel(tr("seq=0"), this));
}

} // namespace griz::ui
