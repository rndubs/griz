#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include "Framing.h"
#include "Rendezvous.h"

// Client-side network worker. Mirrors pygriz/src/griz/rpc_worker.py — length-framed
// JSON over a loopback TCP socket, rendezvous-file token handshake, per-request-id
// correlation. See planning/ui-design/04-client.md §11 for the Python→Qt translation
// table and planning/ui-design/02-protocol.md for the on-wire contract.
//
// Threading (MVP): Worker runs on its creating thread's event loop and drives
// QTcpSocket asynchronously. The three-thread split from 04-client.md §9 is a
// future moveToThread() refinement that doesn't change this public API.

class QTcpSocket;

namespace griz::net {

struct GrizError {
    QString code;
    QString message;
};

struct Response {
    QString     requestId;
    bool        ok = false;
    GrizError   error;
    QJsonObject data;     // response.data payload
    QJsonObject raw;      // entire JSON envelope (for callers that want status/stdout/etc.)
};

class Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);
    ~Worker() override;

    // Spawn `griz-server --transport=rpc --rendezvous=<tempfile> -i <db>`, then
    // connect + handshake + wait for the ready event. Blocks on the creating
    // thread's event loop until the session is usable or the timeout expires.
    // Returns true on success; errorString() carries the failure reason.
    bool launchAndConnect(const QString &serverBinary,
                          const QString &databasePath,
                          int width  = 1024,
                          int height = 1024,
                          int timeoutMs = 30000);

    // Graceful shutdown: send `quit`, close socket, wait for the subprocess
    // to exit, then reap. Safe to call multiple times.
    void shutdown(int timeoutMs = 5000);

    // Async: send a JSON-envelope request, return the request id immediately.
    // The Response surfaces via responseReceived().
    QString sendCommand(const QString &cmd);

    // Overload for commands that take a JSON body (e.g. resize with
    // `{"w":W,"h":H}`, or future picks with `{"x":X,"y":Y,"mode":...}`).
    // Extras merge into the outgoing envelope alongside type/id/cmd.
    QString sendCommand(const QString &cmd, const QJsonObject &extras);

    // Sync helper: send + wait for the matching response. Uses a local event
    // loop; safe to call from the UI thread. Returns false on timeout or if
    // the session died before the response arrived.
    bool runCommand(const QString &cmd, int timeoutMs, Response *out);

    QString errorString() const { return m_errorString; }
    bool    isConnected() const { return m_handshakeComplete; }
    QJsonObject serverInfo() const { return m_serverInfo; }

    static constexpr const char *kClientProtocolVersion = "1.0";
    static constexpr const char *kClientIdentifier      = "griz-qt-client";

signals:
    void connected();
    void disconnected(QString reason);
    void responseReceived(griz::net::Response response);
    void eventReceived(QJsonObject event);
    void frameReceived(QByteArray payload);
    void protocolMismatch(QString serverVersion, QString clientVersion);

    // Emitted every time the response-id map or event list changes, so
    // runCommand() can wake its local event loop.
    void stateUpdated();

private slots:
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onFrame(griz::net::Frame frame);
    void onFramingError(QString message);

private:
    bool startProcess(const QString &serverBinary,
                      const QStringList &args,
                      int timeoutMs);
    bool pollRendezvous(const QString &path, int timeoutMs, RendezvousInfo *out);
    bool connectSocket(const RendezvousInfo &rv, int timeoutMs);
    bool performHandshake(const QString &token, int timeoutMs);
    bool waitForReady(int timeoutMs);

    void sendJson(const QJsonObject &obj);
    void writeFrame(FrameKind kind, const QByteArray &payload);
    void routeJsonPayload(const QByteArray &payload);

    void setError(const QString &msg);
    void fail(const QString &reason);

    QProcess          *m_process            = nullptr;
    QTcpSocket        *m_socket             = nullptr;
    Framer            *m_framer             = nullptr;
    QTemporaryDir     *m_rendezvousDir      = nullptr;
    QString            m_rendezvousPath;

    QHash<QString, Response> m_responsesById;
    QList<QJsonObject>       m_pendingEvents;
    QJsonObject              m_serverInfo;
    QJsonObject              m_helloAck;

    bool    m_handshakeComplete = false;
    bool    m_readyReceived     = false;
    quint64 m_nextRequestId     = 1;
    QString m_errorString;
};

} // namespace griz::net
