#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include "net/Worker.h"

// Application shell. Owns the main window plus the live network Worker and the
// SessionState mirror, and stitches them together. See planning/ui-design/04-client.md
// §2.1 / §11.

namespace griz::ui    { class MainWindow; }
namespace griz::model { class SessionState; }

namespace griz {

class App : public QObject {
    Q_OBJECT
public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    void parseArgs(const QStringList &args);
    void show();

private slots:
    void startSession();
    void onConsoleCommand(const QString &cmd);
    void onWorkerConnected();
    void onWorkerDisconnected(const QString &reason);
    void onWorkerResponse(const griz::net::Response &response);
    void onWorkerEvent(const QJsonObject &event);
    void onAboutToQuit();

private:
    static QString autodetectServerBinary();
    static QString autodetectDatabasePath();
    static QString formatResponse(const griz::net::Response &response);

    bool                  m_localMode      = true;
    QString               m_serverBinary;
    QString               m_databasePath;
    bool                  m_sessionStarted = false;

    QString               m_initialQStateId;

    net::Worker          *m_worker         = nullptr;
    model::SessionState  *m_sessionState   = nullptr;
    ui::MainWindow       *m_mainWindow     = nullptr;
};

} // namespace griz
