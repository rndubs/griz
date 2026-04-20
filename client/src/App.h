#pragma once

#include <QObject>
#include <QStringList>

// Application shell. Owns the main window and (eventually) top-level services
// like the Worker thread and HostProfiles. See planning/ui-design/04-client.md
// §2.1. MVP: only parses args and shows the window.

namespace griz::ui { class MainWindow; }

namespace griz {

class App : public QObject {
    Q_OBJECT
public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    void parseArgs(const QStringList &args);
    void show();

private:
    ui::MainWindow *m_mainWindow = nullptr;
};

} // namespace griz
