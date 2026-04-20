#include "App.h"

#include "model/SessionState.h"
#include "ui/Console.h"
#include "ui/MainWindow.h"
#include "ui/MaterialsDock.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QTimer>

#ifndef GRIZ_REPO_ROOT
#define GRIZ_REPO_ROOT ""
#endif

namespace griz {

App::App(QObject *parent) : QObject(parent) {
    m_worker       = new net::Worker(this);
    m_sessionState = new model::SessionState(this);
    m_mainWindow   = new ui::MainWindow();

    connect(m_mainWindow->console(), &ui::Console::commandEntered,
            this, &App::onConsoleCommand);

    connect(m_worker, &net::Worker::connected,
            this, &App::onWorkerConnected);
    connect(m_worker, &net::Worker::disconnected,
            this, &App::onWorkerDisconnected);
    connect(m_worker, &net::Worker::responseReceived,
            this, &App::onWorkerResponse);
    connect(m_worker, &net::Worker::eventReceived,
            this, &App::onWorkerEvent);

    connect(m_sessionState, &model::SessionState::materialsChanged,
            this, [this]() {
        m_mainWindow->materialsDock()->setMaterials(m_sessionState->materials());
    });
    connect(m_sessionState, &model::SessionState::databaseChanged,
            this, [this](const model::DatabaseInfo &db) {
        const QString title = db.title.isEmpty()
            ? QFileInfo(db.path).fileName()
            : db.title;
        if (!title.isEmpty()) {
            m_mainWindow->setWindowTitle(QStringLiteral("Griz — %1").arg(title));
        }
    });
    connect(m_sessionState, &model::SessionState::stateOverflow,
            this, [this]() {
        // Refresh on missed sequence — see planning/ui-design/04-client.md §4.2.
        m_initialQStateId = m_worker->sendCommand(QStringLiteral("q_state"));
    });

    if (auto *qa = qobject_cast<QApplication *>(QCoreApplication::instance())) {
        connect(qa, &QApplication::aboutToQuit, this, &App::onAboutToQuit);
    }
}

App::~App() {
    if (m_worker) {
        m_worker->shutdown(2000);
    }
    delete m_mainWindow;
    m_mainWindow = nullptr;
}

void App::parseArgs(const QStringList &args) {
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Griz Qt client"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption localOption(
        QStringLiteral("local"),
        QStringLiteral("Launch a local griz-server (default)."));
    QCommandLineOption binOption(
        QStringLiteral("bin"),
        QStringLiteral("Path to griz-server binary (autodetected if omitted)."),
        QStringLiteral("path"));
    QCommandLineOption dbOption(
        QStringLiteral("db"),
        QStringLiteral("Path to Mili database (.pltA) to open."),
        QStringLiteral("path"));

    parser.addOption(localOption);
    parser.addOption(binOption);
    parser.addOption(dbOption);
    parser.process(args);

    // MVP only supports local launch; --local is accepted for forward compat.
    m_localMode    = true;
    Q_UNUSED(localOption);
    m_serverBinary = parser.isSet(binOption) ? parser.value(binOption)
                                             : autodetectServerBinary();
    m_databasePath = parser.isSet(dbOption)  ? parser.value(dbOption)
                                             : autodetectDatabasePath();
}

void App::show() {
    m_mainWindow->show();
    QTimer::singleShot(0, this, &App::startSession);
}

void App::startSession() {
    if (m_sessionStarted) return;
    m_sessionStarted = true;

    if (m_serverBinary.isEmpty()) {
        m_mainWindow->setConnectionStatus(tr("no griz-server binary"));
        m_mainWindow->console()->appendOutput(
            tr("[no-bin] griz-server not found; pass --bin <path> "
               "or run ./build.sh server"));
        return;
    }
    if (m_databasePath.isEmpty()) {
        m_mainWindow->setConnectionStatus(tr("no database"));
        m_mainWindow->console()->appendOutput(
            tr("[no-db] no database; pass --db <path>"));
        return;
    }

    m_mainWindow->setConnectionStatus(tr("connecting…"));
    m_mainWindow->console()->appendOutput(
        tr("[launch] %1 %2").arg(m_serverBinary, m_databasePath));

    const bool ok = m_worker->launchAndConnect(m_serverBinary, m_databasePath,
                                                1024, 1024, 30000);
    if (!ok) {
        m_mainWindow->setConnectionStatus(tr("launch failed"));
        m_mainWindow->console()->appendOutput(
            tr("[launch-failed] %1").arg(m_worker->errorString()));
        return;
    }
}

void App::onConsoleCommand(const QString &cmd) {
    if (!m_worker->isConnected()) {
        m_mainWindow->console()->appendOutput(
            tr("[disconnected] not sending: %1").arg(cmd));
        return;
    }
    m_worker->sendCommand(cmd);
}

void App::onWorkerConnected() {
    m_mainWindow->setConnectionStatus(tr("connected"));
    const QJsonObject info = m_worker->serverInfo();
    const QString version = info.value(QStringLiteral("version")).toString();
    if (!version.isEmpty()) {
        m_mainWindow->console()->appendOutput(
            tr("[connected] server protocol %1").arg(version));
    }
    m_initialQStateId = m_worker->sendCommand(QStringLiteral("q_state"));
}

void App::onWorkerDisconnected(const QString &reason) {
    m_mainWindow->setConnectionStatus(tr("disconnected"));
    m_mainWindow->console()->appendOutput(
        tr("[disconnected] %1").arg(reason));
}

void App::onWorkerResponse(const griz::net::Response &response) {
    // Route the initial q_state into the SessionState mirror; everything else
    // just gets surfaced in the console for now.
    if (!m_initialQStateId.isEmpty() && response.requestId == m_initialQStateId) {
        m_initialQStateId.clear();
        if (response.ok && !response.data.isEmpty()) {
            m_sessionState->applyFullState(response.data);
        }
    }
    m_mainWindow->console()->appendOutput(formatResponse(response));
}

void App::onWorkerEvent(const QJsonObject &event) {
    const QString name = event.value(QStringLiteral("event")).toString();
    if (name == QLatin1String("state_changed")) {
        m_sessionState->applyDiff(event);
        m_mainWindow->setStateSeqLabel(m_sessionState->lastSeq());
        // After every state change, refresh the q_state mirror so the dock
        // sees the new shape. The 30 Hz auto-push cap on the server keeps
        // this from spamming.
        if (m_initialQStateId.isEmpty()) {
            m_initialQStateId = m_worker->sendCommand(QStringLiteral("q_state"));
        }
    } else if (name == QLatin1String("session_ending")) {
        const QString reason = event.value(QStringLiteral("reason")).toString();
        m_mainWindow->console()->appendOutput(
            tr("[session_ending] %1").arg(reason));
    }
}

void App::onAboutToQuit() {
    if (m_worker) {
        m_worker->shutdown(3000);
    }
}

QString App::formatResponse(const griz::net::Response &response) {
    if (!response.ok) {
        return QStringLiteral("[%1] %2")
            .arg(response.error.code.isEmpty() ? QStringLiteral("error")
                                               : response.error.code,
                 response.error.message);
    }
    const QString stdoutTxt = response.raw
        .value(QStringLiteral("stdout")).toString();
    if (!stdoutTxt.isEmpty()) {
        return stdoutTxt.trimmed();
    }
    if (!response.data.isEmpty()) {
        return QString::fromUtf8(QJsonDocument(response.data)
            .toJson(QJsonDocument::Compact));
    }
    return QStringLiteral("[ok]");
}

QString App::autodetectServerBinary() {
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString fromEnv = env.value(QStringLiteral("GRIZ_BIN"));
    if (!fromEnv.isEmpty() && QFileInfo(fromEnv).isExecutable()) {
        return fromEnv;
    }

    const QString rootStr = QString::fromUtf8(GRIZ_REPO_ROOT);
    if (rootStr.isEmpty()) return {};

    const QDir srcDir(QDir(rootStr).filePath(QStringLiteral("Src")));
    QFileInfoList candidates;
    const QStringList buildDirs =
        srcDir.entryList({QStringLiteral("GRIZ4-*")}, QDir::Dirs);
    for (const QString &entry : buildDirs) {
        const QString binPath = srcDir.filePath(
            entry + QStringLiteral("/bin_server_opt/griz-server"));
        QFileInfo info(binPath);
        if (info.isExecutable()) {
            candidates.append(info);
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() > b.lastModified();
    });
    return candidates.isEmpty() ? QString()
                                : candidates.first().absoluteFilePath();
}

QString App::autodetectDatabasePath() {
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString fromEnv = env.value(QStringLiteral("GRIZ_TEST_DB"));
    if (!fromEnv.isEmpty() && QFileInfo(fromEnv).exists()) {
        return fromEnv;
    }
    const QString rootStr = QString::fromUtf8(GRIZ_REPO_ROOT);
    if (rootStr.isEmpty()) return {};
    const QString db = QDir(rootStr).filePath(
        QStringLiteral("Src/test/image/bar71/bar71.pltA"));
    return QFileInfo(db).exists() ? db : QString();
}

} // namespace griz
