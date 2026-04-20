#include "SessionState.h"

namespace griz::model {

SessionState::SessionState(QObject *parent) : QObject(parent) {}

void SessionState::applyFullState(const QJsonObject &qStateResponse) {
    Q_UNUSED(qStateResponse);
}

void SessionState::applyDiff(const QJsonObject &stateChangedEvent) {
    Q_UNUSED(stateChangedEvent);
}

} // namespace griz::model
