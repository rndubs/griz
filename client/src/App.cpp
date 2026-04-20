#include "App.h"

#include "ui/MainWindow.h"

namespace griz {

App::App(QObject *parent) : QObject(parent) {
    m_mainWindow = new ui::MainWindow();
}

App::~App() {
    delete m_mainWindow;
}

void App::parseArgs(const QStringList &args) {
    // CLI flags (--local, --host, --rendezvous) land alongside the network
    // bring-up commit. See planning/ui-design/04-client.md §2.1 /§9.
    Q_UNUSED(args);
}

void App::show() {
    m_mainWindow->show();
}

} // namespace griz
