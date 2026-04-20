#include "MainWindow.h"

#include "Console.h"
#include "MaterialsDock.h"
#include "ResultsDock.h"
#include "SelectionDock.h"
#include "TimeSlider.h"
#include "Viewport.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>

namespace griz::ui {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("Griz"));
    resize(1280, 800);

    m_viewport = new Viewport(this);
    setCentralWidget(m_viewport);

    buildDocks();
    buildMenuBar();
    buildStatusBar();

    // After the hardcoded-default layout is in place, fold in whatever the
    // user had on their last exit. First run (no settings yet) leaves the
    // default alone per planning/ui-design/04-client.md §8.
    restoreLayout();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveLayout();
    QMainWindow::closeEvent(event);
}

void MainWindow::restoreLayout() {
    QSettings settings;
    const QByteArray geom  = settings.value(QStringLiteral("MainWindow/geometry")).toByteArray();
    const QByteArray state = settings.value(QStringLiteral("MainWindow/windowState")).toByteArray();
    if (!geom.isEmpty())  restoreGeometry(geom);
    if (!state.isEmpty()) restoreState(state);
}

void MainWindow::saveLayout() {
    QSettings settings;
    settings.setValue(QStringLiteral("MainWindow/geometry"),    saveGeometry());
    settings.setValue(QStringLiteral("MainWindow/windowState"), saveState());
}

void MainWindow::runCommand(const QString &cmd) {
    if (!m_console || cmd.isEmpty()) return;
    m_console->execute(cmd);
}

void MainWindow::onOpenDatabase() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Mili Database"), QString(),
        tr("Mili databases (*.pltA *.plt *.plt.*);;All files (*)"));
    if (path.isEmpty()) return;
    runCommand(QStringLiteral("load %1").arg(path));
}

void MainWindow::onRunScript() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Run Griz Script"), QString(),
        tr("Griz scripts (*.grs *.hist *.his);;All files (*)"));
    if (path.isEmpty()) return;
    runCommand(QStringLiteral("rdhis %1").arg(path));
}

void MainWindow::buildMenuBar() {
    auto *mb = menuBar();

    // Menu actions route through Console::execute — see 04-client.md §2.3 /
    // §5 (invariant I1: every UI action lowers to a Griz command string,
    // keeping the command history honest). The only exceptions are local
    // UI operations (quit, clear console, dock visibility) that don't
    // touch the server.

    // ---- File ----
    auto *fileMenu = mb->addMenu(tr("&File"));

    auto *openAct = fileMenu->addAction(tr("&Open Database…"));
    openAct->setShortcut(QKeySequence::Open);                  // Ctrl+O
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenDatabase);

    auto *runAct = fileMenu->addAction(tr("&Run Script…"));
    connect(runAct, &QAction::triggered, this, &MainWindow::onRunScript);

    fileMenu->addSeparator();

    auto *quitAct = fileMenu->addAction(tr("&Quit"));
    quitAct->setShortcut(QKeySequence::Quit);                  // Ctrl+Q
    quitAct->setMenuRole(QAction::QuitRole);
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);

    // ---- Edit ----
    auto *editMenu = mb->addMenu(tr("&Edit"));

    auto *clearConsoleAct = editMenu->addAction(tr("&Clear Console"));
    connect(clearConsoleAct, &QAction::triggered, this, [this]() {
        if (m_console) m_console->clearOutput();
    });

    // ---- View ----
    auto *viewMenu = mb->addMenu(tr("&View"));

    auto *resetViewAct = viewMenu->addAction(tr("&Reset View"));
    resetViewAct->setShortcut(QKeySequence(Qt::Key_F));
    connect(resetViewAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("rview"));
    });

    viewMenu->addSeparator();

    auto *projMenu = viewMenu->addMenu(tr("&Projection"));
    auto *projGroup = new QActionGroup(this);
    projGroup->setExclusive(true);

    auto *perspAct = projMenu->addAction(tr("&Perspective"));
    perspAct->setCheckable(true);
    perspAct->setChecked(true);
    projGroup->addAction(perspAct);
    connect(perspAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("switch persp"));
    });

    auto *orthoAct = projMenu->addAction(tr("&Orthographic"));
    orthoAct->setCheckable(true);
    projGroup->addAction(orthoAct);
    connect(orthoAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("switch ortho"));
    });

    // ---- Draw ----
    auto *drawMenu = mb->addMenu(tr("&Draw"));
    auto *modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);

    auto *solidAct = drawMenu->addAction(tr("&Solid"));
    solidAct->setCheckable(true);
    solidAct->setChecked(true);
    modeGroup->addAction(solidAct);
    connect(solidAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("switch solid"));
    });

    auto *wireAct = drawMenu->addAction(tr("&Wireframe"));
    wireAct->setCheckable(true);
    modeGroup->addAction(wireAct);
    connect(wireAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("switch wf"));
    });

    auto *hiddenAct = drawMenu->addAction(tr("&Hidden-Line"));
    hiddenAct->setCheckable(true);
    modeGroup->addAction(hiddenAct);
    connect(hiddenAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("switch hidden"));
    });

    drawMenu->addSeparator();

    auto *cloudAct = drawMenu->addAction(tr("Point &Cloud"));
    cloudAct->setCheckable(true);
    modeGroup->addAction(cloudAct);
    connect(cloudAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("switch cloud"));
    });

    // ---- Select ----
    auto *selectMenu = mb->addMenu(tr("&Select"));

    auto *clearHiliteAct = selectMenu->addAction(tr("Clear &Hilite"));
    connect(clearHiliteAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("clrhil"));
    });

    auto *clearSelAct = selectMenu->addAction(tr("Clear &All Selection"));
    connect(clearSelAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("clear_selection"));
    });

    // ---- Animate ----
    auto *animMenu = mb->addMenu(tr("&Animate"));

    auto *firstAct = animMenu->addAction(tr("&First State"));
    firstAct->setShortcut(QKeySequence(Qt::Key_Home));
    connect(firstAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("f"));
    });

    auto *prevAct = animMenu->addAction(tr("&Previous State"));
    prevAct->setShortcut(QKeySequence(Qt::Key_Comma));
    connect(prevAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("p"));
    });

    auto *nextAct = animMenu->addAction(tr("&Next State"));
    nextAct->setShortcut(QKeySequence(Qt::Key_Period));
    connect(nextAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("n"));
    });

    auto *lastAct = animMenu->addAction(tr("&Last State"));
    lastAct->setShortcut(QKeySequence(Qt::Key_End));
    connect(lastAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("l"));
    });

    animMenu->addSeparator();

    auto *playAct = animMenu->addAction(tr("Pla&y / Pause"));
    playAct->setShortcut(QKeySequence(Qt::Key_Space));
    connect(playAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("anim"));
    });

    // ---- Window (dock toggles) ----
    auto *windowMenu = mb->addMenu(tr("&Window"));
    if (m_materialsDock) windowMenu->addAction(m_materialsDock->toggleViewAction());
    if (m_selectionDock) windowMenu->addAction(m_selectionDock->toggleViewAction());
    if (m_resultsDock)   windowMenu->addAction(m_resultsDock->toggleViewAction());
    if (m_timeSlider)    windowMenu->addAction(m_timeSlider->toggleViewAction());
    if (m_consoleDock)   windowMenu->addAction(m_consoleDock->toggleViewAction());

    // ---- Help ----
    auto *helpMenu = mb->addMenu(tr("&Help"));
    auto *aboutAct = helpMenu->addAction(tr("&About Griz"));
    aboutAct->setMenuRole(QAction::AboutRole);
    connect(aboutAct, &QAction::triggered, this, [this]() {
        runCommand(QStringLiteral("info"));
    });
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
    m_consoleDock = new QDockWidget(tr("Console"), this);
    m_consoleDock->setObjectName(QStringLiteral("ConsoleDock"));
    m_consoleDock->setWidget(m_console);
    addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);
}

void MainWindow::buildStatusBar() {
    auto *sb = statusBar();
    m_statusConnection = new QLabel(tr("disconnected"), this);
    m_statusFps        = new QLabel(tr("—"), this);
    m_statusHost       = new QLabel(tr("local"), this);
    m_statusSeq        = new QLabel(tr("seq=0"), this);
    sb->addPermanentWidget(m_statusConnection);
    sb->addPermanentWidget(m_statusFps);
    sb->addPermanentWidget(m_statusHost);
    sb->addPermanentWidget(m_statusSeq);

    // Flash timer: on tick, toggle between a saturated red background
    // and the plain disconnect style. After kFlashMaxTicks toggles we
    // stop and leave the label in its muted red "disconnected" state.
    m_flashTimer.setInterval(180);
    connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
        constexpr int kFlashMaxTicks = 8;
        if (!m_statusConnection) { m_flashTimer.stop(); return; }
        const bool bright = (m_flashTicks % 2) == 0;
        m_statusConnection->setStyleSheet(bright
            ? QStringLiteral("QLabel { background: #c0392b; color: white;"
                             "          padding: 1px 6px; border-radius: 2px; }")
            : QStringLiteral("QLabel { color: #c0392b; font-weight: bold; }"));
        if (++m_flashTicks >= kFlashMaxTicks) {
            m_flashTimer.stop();
            m_statusConnection->setStyleSheet(
                QStringLiteral("QLabel { color: #c0392b; font-weight: bold; }"));
        }
    });
}

void MainWindow::setConnectionStatus(const QString &text) {
    if (!m_statusConnection) return;
    m_flashTimer.stop();
    m_flashTicks = 0;
    m_statusConnection->setStyleSheet(QString());   // restore default palette
    m_statusConnection->setText(text);
}

void MainWindow::flashDisconnect(const QString &text) {
    if (!m_statusConnection) return;
    m_statusConnection->setText(text);
    m_flashTicks = 0;
    m_flashTimer.start();
}

void MainWindow::setHostLabel(const QString &text) {
    if (m_statusHost) m_statusHost->setText(text);
}

void MainWindow::setStateSeqLabel(quint64 seq) {
    if (m_statusSeq) m_statusSeq->setText(tr("seq=%1").arg(seq));
}

void MainWindow::setFpsLabel(double fps) {
    if (!m_statusFps) return;
    if (fps <= 0.0) {
        m_statusFps->setText(QStringLiteral("—"));
    } else {
        m_statusFps->setText(QStringLiteral("%1 fps").arg(fps, 0, 'f', 1));
    }
}

} // namespace griz::ui
