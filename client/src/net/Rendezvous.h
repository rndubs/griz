#pragma once

#include <QObject>
#include <QString>

// Rendezvous-file discovery per CLAUDE.md §Rendezvous.
//
// The server writes $HOME/.griz/rendezvous/<session-id>.json with mode 0600
// before accepting its first TCP connection. Contents (version 1):
//
//     { "version": 1, "session_id": "griz-<8hex>", "host": "127.0.0.1",
//       "port": <uint16>, "token": "<32-byte base64>",
//       "server_pid": <int>, "started_at": <ISO 8601> }
//
// The token must be echoed in the first hello frame; the server does a
// constant-time compare.

namespace griz::net {

struct RendezvousInfo {
    int     version    = 0;
    QString sessionId;
    QString host;
    quint16 port       = 0;
    QString token;
    qint64  serverPid  = 0;
    QString startedAt;
};

class Rendezvous {
public:
    enum class ParseStatus {
        Ok,
        FileMissing,
        FileUnreadable,
        FilePartial,   // valid JSON not yet present; caller should retry
        SchemaInvalid, // JSON parsed but required fields missing/malformed
    };

    // Read and parse a rendezvous file. Returns FilePartial if the file is
    // empty or has zero size (server in the middle of writing); caller should
    // retry after a short sleep.
    static ParseStatus parseFile(const QString &path, RendezvousInfo *out, QString *error = nullptr);
};

} // namespace griz::net
