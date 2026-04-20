#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

// Client-side network worker. Mirrors pygriz/src/griz/rpc_worker.py — length-framed
// JSON over a loopback TCP socket, rendezvous-file token handshake, per-request-id
// correlation. See planning/ui-design/04-client.md §11 for the Python→Qt translation
// table and planning/ui-design/02-protocol.md for the on-wire contract.
//
// Stub in this commit: declarations only. Connection/handshake/dispatch land in
// the next Phase-5 commit.

namespace griz::net {

struct GrizError {
    QString code;
    QString message;
};

struct Response {
    QString     requestId;
    bool        ok = false;
    GrizError   error;
    QJsonObject data;
    QString     stdoutText;
    QString     stderrText;
};

class Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);
    ~Worker() override;

    // Connect to a server that is already running and has published a
    // rendezvous file under $HOME/.griz/rendezvous/ (see CLAUDE.md §Rendezvous).
    void connectToServer(const QString &rendezvousPath);
    void disconnectFromServer();

    // Fire-and-forget dispatch; response surfaces via responseReceived().
    // Returns the generated request id so callers can correlate.
    QString sendCommand(const QString &cmd, int timeoutMs = 30000);

signals:
    void connected();
    void disconnected(QString reason);
    void responseReceived(griz::net::Response response);
    void eventReceived(QJsonObject event);
    void frameReceived(QByteArray payload);
    void protocolMismatch(QString serverVersion, QString clientVersion);
};

} // namespace griz::net
