#include "Worker.h"

#include "Rendezvous.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QTimer>

namespace griz::net {

namespace {

// Pick a rendezvous path the MCP bridge can find. Prefer
// "$HOME/.griz/rendezvous/ui-<pid>.json" so a user can export
// GRIZ_MCP_ATTACH_RENDEZVOUS=<that path> without parsing logs; fall back
// to a unique tempfile when $HOME/.griz/rendezvous isn't writable.
// On return, *usedStableDir tells the caller whether cleanup should
// unlink the file (stable dir) or leave QTemporaryDir to handle it.
QString chooseRendezvousPath(QTemporaryDir **fallbackDir, bool *usedStableDir) {
    *usedStableDir = false;
    const QString home = QDir::homePath();
    if (!home.isEmpty()) {
        const QString dirPath = home + QStringLiteral("/.griz/rendezvous");
        QDir dir;
        if (dir.mkpath(dirPath)) {
            QFileInfo info(dirPath);
            if (info.isDir() && info.isWritable()) {
                *usedStableDir = true;
                return dirPath + QStringLiteral("/ui-%1.json")
                    .arg(QCoreApplication::applicationPid());
            }
        }
    }
    *fallbackDir = new QTemporaryDir();
    (*fallbackDir)->setAutoRemove(true);
    if (!(*fallbackDir)->isValid()) {
        return {};
    }
    return (*fallbackDir)->filePath(QStringLiteral("rendezvous.json"));
}

} // namespace

Worker::Worker(QObject *parent) : QObject(parent) {}

Worker::~Worker() {
    shutdown(2000);
}

bool Worker::launchAndConnect(const QString &serverBinary,
                              const QString &databasePath,
                              int width,
                              int height,
                              int timeoutMs) {
    QDeadlineTimer deadline(timeoutMs);

    m_rendezvousDir  = nullptr;
    m_rendezvousStable = false;
    m_rendezvousPath = chooseRendezvousPath(&m_rendezvousDir,
                                            &m_rendezvousStable);
    if (m_rendezvousPath.isEmpty()) {
        setError(QStringLiteral("cannot select rendezvous path"));
        return false;
    }
    // Pre-clean any leftover file from a prior crashed session at the same PID.
    QFile::remove(m_rendezvousPath);
    // Log the rendezvous path so users can point MCP at it without having to
    // scrape internal state (DEMO.md task A).
    fprintf(stderr,
            "griz-client: rendezvous=%s (export GRIZ_MCP_ATTACH_RENDEZVOUS to attach MCP)\n",
            m_rendezvousPath.toLocal8Bit().constData());

    const QStringList args {
        QStringLiteral("--transport=rpc"),
        QStringLiteral("--rendezvous=") + m_rendezvousPath,
        QStringLiteral("-i"), databasePath,
        QStringLiteral("-w"), QString::number(width), QString::number(height),
    };

    if (!startProcess(serverBinary, args, static_cast<int>(deadline.remainingTime()))) {
        return false;
    }

    RendezvousInfo rv;
    if (!pollRendezvous(m_rendezvousPath, static_cast<int>(deadline.remainingTime()), &rv)) {
        return false;
    }

    if (!connectSocket(rv, static_cast<int>(deadline.remainingTime()))) {
        return false;
    }

    if (!performHandshake(rv.token, static_cast<int>(deadline.remainingTime()))) {
        return false;
    }

    if (!waitForReady(static_cast<int>(deadline.remainingTime()))) {
        return false;
    }

    emit connected();
    return true;
}

bool Worker::startProcess(const QString &serverBinary,
                          const QStringList &args,
                          int timeoutMs) {
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setStandardInputFile(QProcess::nullDevice());
    m_process->setStandardOutputFile(QProcess::nullDevice());
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Worker::onProcessFinished);

    m_process->start(serverBinary, args);
    if (!m_process->waitForStarted(qMax(1000, timeoutMs))) {
        setError(QStringLiteral("griz-server failed to start: %1").arg(m_process->errorString()));
        return false;
    }
    return true;
}

bool Worker::pollRendezvous(const QString &path, int timeoutMs, RendezvousInfo *out) {
    QDeadlineTimer deadline(qMax(0, timeoutMs));
    while (true) {
        if (m_process == nullptr || m_process->state() == QProcess::NotRunning) {
            setError(QStringLiteral("griz-server exited before writing rendezvous; stderr: %1")
                         .arg(QString::fromUtf8(m_process ? m_process->readAllStandardError() : QByteArray())));
            return false;
        }

        QString err;
        const auto status = Rendezvous::parseFile(path, out, &err);
        if (status == Rendezvous::ParseStatus::Ok) {
            return true;
        }
        if (status == Rendezvous::ParseStatus::SchemaInvalid) {
            setError(err);
            return false;
        }
        if (deadline.hasExpired()) {
            setError(QStringLiteral("rendezvous file did not appear in time (%1)").arg(err));
            return false;
        }

        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
}

bool Worker::connectSocket(const RendezvousInfo &rv, int timeoutMs) {
    m_socket = new QTcpSocket(this);
    m_framer = new Framer(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &Worker::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &Worker::onSocketDisconnected);
    connect(m_framer, &Framer::frameReady, this, &Worker::onFrame);
    connect(m_framer, &Framer::framingError, this, &Worker::onFramingError);

    m_socket->connectToHost(rv.host, rv.port);
    if (!m_socket->waitForConnected(qMax(1000, timeoutMs))) {
        setError(QStringLiteral("TCP connect failed: %1").arg(m_socket->errorString()));
        return false;
    }
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    return true;
}

bool Worker::performHandshake(const QString &token, int timeoutMs) {
    QJsonObject hello {
        { QStringLiteral("type"),    QStringLiteral("hello") },
        { QStringLiteral("version"), QString::fromLatin1(kClientProtocolVersion) },
        { QStringLiteral("client"),  QString::fromLatin1(kClientIdentifier) },
        { QStringLiteral("token"),   token },
    };
    sendJson(hello);

    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    auto onUpdate = connect(this, &Worker::stateUpdated, &loop, [this, &loop]() {
        if (!m_helloAck.isEmpty()) {
            loop.quit();
        }
    });
    auto onTimeout = connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    auto onDisc    = connect(this, &Worker::disconnected, &loop, &QEventLoop::quit);
    timer.start(qMax(1000, timeoutMs));
    if (m_helloAck.isEmpty()) {
        loop.exec();
    }
    QObject::disconnect(onUpdate);
    QObject::disconnect(onTimeout);
    QObject::disconnect(onDisc);

    if (m_helloAck.isEmpty()) {
        setError(QStringLiteral("no hello_ack within timeout"));
        return false;
    }
    const bool compatible = m_helloAck.value(QStringLiteral("compatible")).toBool(false);
    if (!compatible) {
        const QString serverVer = m_helloAck.value(QStringLiteral("version")).toString();
        emit protocolMismatch(serverVer, QString::fromLatin1(kClientProtocolVersion));
        setError(QStringLiteral("protocol mismatch: server=%1 client=%2")
                     .arg(serverVer, QString::fromLatin1(kClientProtocolVersion)));
        return false;
    }
    m_serverInfo = m_helloAck;
    m_handshakeComplete = true;
    return true;
}

bool Worker::waitForReady(int timeoutMs) {
    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    auto onUpdate = connect(this, &Worker::stateUpdated, &loop, [this, &loop]() {
        if (m_readyReceived) {
            loop.quit();
        }
    });
    auto onTimeout = connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    auto onDisc    = connect(this, &Worker::disconnected, &loop, &QEventLoop::quit);
    timer.start(qMax(1000, timeoutMs));
    if (!m_readyReceived) {
        loop.exec();
    }
    QObject::disconnect(onUpdate);
    QObject::disconnect(onTimeout);
    QObject::disconnect(onDisc);

    if (!m_readyReceived) {
        setError(QStringLiteral("no ready event within timeout"));
        return false;
    }
    return true;
}

QString Worker::sendCommand(const QString &cmd) {
    return sendCommand(cmd, QJsonObject{});
}

QString Worker::sendCommand(const QString &cmd, const QJsonObject &extras) {
    const QString requestId = QStringLiteral("req_%1").arg(m_nextRequestId++);
    QJsonObject obj = extras;
    obj.insert(QStringLiteral("type"), QStringLiteral("request"));
    obj.insert(QStringLiteral("id"),   requestId);
    obj.insert(QStringLiteral("cmd"),  cmd);
    sendJson(obj);
    return requestId;
}

bool Worker::runCommand(const QString &cmd, int timeoutMs, Response *out) {
    const QString requestId = sendCommand(cmd);

    if (auto it = m_responsesById.find(requestId); it != m_responsesById.end()) {
        if (out) *out = it.value();
        m_responsesById.erase(it);
        return true;
    }

    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    auto onUpdate = connect(this, &Worker::stateUpdated, &loop, [this, &loop, requestId]() {
        if (m_responsesById.contains(requestId)) {
            loop.quit();
        }
    });
    auto onTimeout = connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    auto onDisc    = connect(this, &Worker::disconnected, &loop, &QEventLoop::quit);
    timer.start(qMax(100, timeoutMs));
    loop.exec();
    QObject::disconnect(onUpdate);
    QObject::disconnect(onTimeout);
    QObject::disconnect(onDisc);

    auto it = m_responsesById.find(requestId);
    if (it == m_responsesById.end()) {
        setError(QStringLiteral("command %1 did not respond within %2 ms").arg(cmd).arg(timeoutMs));
        return false;
    }
    if (out) *out = it.value();
    m_responsesById.erase(it);
    return true;
}

void Worker::shutdown(int timeoutMs) {
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState
        && m_process && m_process->state() != QProcess::NotRunning) {
        QJsonObject quitObj {
            { QStringLiteral("type"), QStringLiteral("request") },
            { QStringLiteral("cmd"),  QStringLiteral("quit") },
        };
        sendJson(quitObj);
        m_socket->flush();
    }

    if (m_socket) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(qMax(500, timeoutMs / 2));
        }
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            if (!m_process->waitForFinished(qMax(1000, timeoutMs))) {
                m_process->kill();
                m_process->waitForFinished(2000);
            }
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    if (m_framer) {
        m_framer->deleteLater();
        m_framer = nullptr;
    }
    if (m_rendezvousStable && !m_rendezvousPath.isEmpty()) {
        // Server unlinks on clean exit; belt-and-suspenders for crash-exit.
        QFile::remove(m_rendezvousPath);
    }
    if (m_rendezvousDir) {
        delete m_rendezvousDir;
        m_rendezvousDir = nullptr;
    }
    m_rendezvousStable = false;
    m_rendezvousPath.clear();

    m_handshakeComplete = false;
    m_readyReceived     = false;
    m_pendingEvents.clear();
    m_responsesById.clear();
    m_helloAck = {};
}

void Worker::sendJson(const QJsonObject &obj) {
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    writeFrame(FrameKind::Json, payload);
}

void Worker::writeFrame(FrameKind kind, const QByteArray &payload) {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        setError(QStringLiteral("cannot write frame: socket not connected"));
        return;
    }
    const QByteArray framed = Framer::encode(kind, payload);
    const qint64 wrote = m_socket->write(framed);
    if (wrote != framed.size()) {
        setError(QStringLiteral("short write: %1 of %2 bytes").arg(wrote).arg(framed.size()));
    }
}

void Worker::onSocketReadyRead() {
    if (!m_socket || !m_framer) return;
    const QByteArray bytes = m_socket->readAll();
    if (!bytes.isEmpty()) {
        m_framer->feed(bytes);
    }
}

void Worker::onSocketDisconnected() {
    emit disconnected(QStringLiteral("socket closed"));
}

void Worker::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status);
    emit disconnected(QStringLiteral("griz-server exited (rc=%1)").arg(exitCode));
}

void Worker::onFrame(Frame frame) {
    switch (frame.kind) {
    case FrameKind::Heartbeat:
        // Echo heartbeats straight back per 02-protocol §2.2 so RTT is symmetric.
        writeFrame(FrameKind::Heartbeat, frame.payload);
        return;
    case FrameKind::Binary:
        emit frameReceived(frame.payload);
        return;
    case FrameKind::Json:
        routeJsonPayload(frame.payload);
        return;
    }
}

void Worker::routeJsonPayload(const QByteArray &payload) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }
    const QJsonObject obj = doc.object();
    const QString type = obj.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("response")) {
        Response r;
        r.requestId  = obj.value(QStringLiteral("id")).toString();
        r.ok         = obj.value(QStringLiteral("status")).toString() == QLatin1String("ok");
        r.raw        = obj;
        r.data       = obj.value(QStringLiteral("data")).toObject();
        const QJsonObject errObj = obj.value(QStringLiteral("error")).toObject();
        r.error.code    = errObj.value(QStringLiteral("code")).toString();
        r.error.message = errObj.value(QStringLiteral("message")).toString();
        if (!r.requestId.isEmpty()) {
            m_responsesById.insert(r.requestId, r);
        }
        emit responseReceived(r);
    } else if (type == QLatin1String("hello_ack")) {
        m_helloAck = obj;
    } else if (type == QLatin1String("event")) {
        m_pendingEvents.append(obj);
        if (obj.value(QStringLiteral("event")).toString() == QLatin1String("ready")) {
            m_readyReceived = true;
            QJsonObject merged = m_serverInfo;
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                merged.insert(it.key(), it.value());
            }
            m_serverInfo = merged;
        }
        emit eventReceived(obj);
    }

    emit stateUpdated();
}

void Worker::onFramingError(QString message) {
    setError(QStringLiteral("framing error: %1").arg(message));
    fail(m_errorString);
}

void Worker::setError(const QString &msg) {
    m_errorString = msg;
}

void Worker::fail(const QString &reason) {
    emit disconnected(reason);
}

} // namespace griz::net
