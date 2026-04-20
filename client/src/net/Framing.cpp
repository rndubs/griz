#include "Framing.h"

namespace griz::net {

Framer::Framer(QObject *parent) : QObject(parent) {}

void Framer::feed(const QByteArray &bytes) {
    // Implementation lands alongside Worker::connectToServer() in the next commit
    // (see planning/UI.md Phase 5 — network bring-up). Placeholder keeps the
    // class compile-clean so UI + model layers can link against the intended API.
    Q_UNUSED(bytes);
}

QByteArray Framer::encode(FrameKind kind, const QByteArray &payload) {
    Q_UNUSED(kind);
    Q_UNUSED(payload);
    return {};
}

} // namespace griz::net
