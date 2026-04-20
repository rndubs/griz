#include "Rendezvous.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace griz::net {

namespace {

void setError(QString *out, const QString &msg) {
    if (out) {
        *out = msg;
    }
}

} // namespace

Rendezvous::ParseStatus Rendezvous::parseFile(const QString &path,
                                              RendezvousInfo *out,
                                              QString *error) {
    if (!out) {
        setError(error, QStringLiteral("null output pointer"));
        return ParseStatus::SchemaInvalid;
    }

    QFileInfo info(path);
    if (!info.exists()) {
        setError(error, QStringLiteral("rendezvous file does not exist: %1").arg(path));
        return ParseStatus::FileMissing;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("cannot open rendezvous file: %1").arg(file.errorString()));
        return ParseStatus::FileUnreadable;
    }

    const QByteArray raw = file.readAll();
    file.close();

    if (raw.isEmpty()) {
        setError(error, QStringLiteral("rendezvous file is empty (server still writing?)"));
        return ParseStatus::FilePartial;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("rendezvous JSON parse error: %1").arg(parseError.errorString()));
        // Could be a partial write; let the caller decide whether to retry.
        return ParseStatus::FilePartial;
    }
    if (!doc.isObject()) {
        setError(error, QStringLiteral("rendezvous JSON is not an object"));
        return ParseStatus::SchemaInvalid;
    }

    const QJsonObject obj = doc.object();
    out->version    = obj.value(QStringLiteral("version")).toInt(0);
    out->sessionId  = obj.value(QStringLiteral("session_id")).toString();
    out->host       = obj.value(QStringLiteral("host")).toString();
    out->port       = static_cast<quint16>(obj.value(QStringLiteral("port")).toInt(0));
    out->token      = obj.value(QStringLiteral("token")).toString();
    out->serverPid  = static_cast<qint64>(obj.value(QStringLiteral("server_pid")).toDouble(0.0));
    out->startedAt  = obj.value(QStringLiteral("started_at")).toString();

    if (out->host.isEmpty() || out->port == 0 || out->token.isEmpty()) {
        setError(error, QStringLiteral("rendezvous JSON missing required fields (host/port/token)"));
        return ParseStatus::SchemaInvalid;
    }

    return ParseStatus::Ok;
}

} // namespace griz::net
