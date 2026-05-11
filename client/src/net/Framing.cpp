#include "Framing.h"

#include <QtEndian>

namespace griz::net {

namespace {

constexpr int kHeaderBytes = 5; // uint32 length + uint8 kind

bool isValidKind(quint8 raw) {
    return raw == static_cast<quint8>(FrameKind::Json)
        || raw == static_cast<quint8>(FrameKind::Binary)
        || raw == static_cast<quint8>(FrameKind::Heartbeat);
}

} // namespace

Framer::Framer(QObject *parent) : QObject(parent) {}

void Framer::feed(const QByteArray &bytes) {
    m_buffer.append(bytes);

    while (true) {
        if (m_buffer.size() < kHeaderBytes) {
            return;
        }

        const quint32 length = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar *>(m_buffer.constData()));
        const quint8 rawKind = static_cast<quint8>(m_buffer.at(4));

        if (length > static_cast<quint32>(kMaxPayloadBytes)) {
            emit framingError(QStringLiteral("frame length %1 exceeds 16 MiB cap").arg(length));
            m_buffer.clear();
            return;
        }
        if (!isValidKind(rawKind)) {
            emit framingError(QStringLiteral("unknown frame kind 0x%1").arg(rawKind, 2, 16, QChar('0')));
            m_buffer.clear();
            return;
        }

        const int total = kHeaderBytes + static_cast<int>(length);
        if (m_buffer.size() < total) {
            return; // wait for more bytes
        }

        Frame frame;
        frame.kind = static_cast<FrameKind>(rawKind);
        frame.payload = m_buffer.mid(kHeaderBytes, static_cast<int>(length));
        m_buffer.remove(0, total);
        emit frameReady(frame);
    }
}

QByteArray Framer::encode(FrameKind kind, const QByteArray &payload) {
    QByteArray out;
    out.resize(kHeaderBytes);
    qToBigEndian<quint32>(static_cast<quint32>(payload.size()),
                          reinterpret_cast<uchar *>(out.data()));
    out[4] = static_cast<char>(static_cast<quint8>(kind));
    out.append(payload);
    return out;
}

} // namespace griz::net
