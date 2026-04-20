#include "Worker.h"

namespace griz::net {

Worker::Worker(QObject *parent) : QObject(parent) {}

Worker::~Worker() = default;

void Worker::connectToServer(const QString &rendezvousPath) {
    Q_UNUSED(rendezvousPath);
}

void Worker::disconnectFromServer() {}

QString Worker::sendCommand(const QString &cmd, int timeoutMs) {
    Q_UNUSED(cmd);
    Q_UNUSED(timeoutMs);
    return {};
}

} // namespace griz::net
