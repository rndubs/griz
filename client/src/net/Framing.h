#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <cstdint>

// Griz RPC framing. Wire format per planning/ui-design/02-protocol.md §2.2:
//   5-byte header = uint32 length (big-endian) + uint8 kind
//   body          = length bytes of payload
// Caps payload at 16 MiB. MVP scope is JSON (0x01) and heartbeat (0x03);
// binary (0x02) carries JPEG frames and screenshots.

namespace griz::net {

enum class FrameKind : quint8 {
    Json      = 0x01,
    Binary    = 0x02,
    Heartbeat = 0x03,
};

struct Frame {
    FrameKind  kind = FrameKind::Json;
    QByteArray payload;
};

class Framer : public QObject {
    Q_OBJECT
public:
    static constexpr int kMaxPayloadBytes = 16 * 1024 * 1024;

    explicit Framer(QObject *parent = nullptr);

    // Feed raw bytes from the socket; emits frameReady() for each complete frame
    // and framingError() if the stream desyncs or a length exceeds the cap.
    void feed(const QByteArray &bytes);

    // Encode a single frame for transmission. Stateless helper.
    static QByteArray encode(FrameKind kind, const QByteArray &payload);

signals:
    void frameReady(griz::net::Frame frame);
    void framingError(QString message);

private:
    QByteArray m_buffer;
};

} // namespace griz::net
