#include "Viewport.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

namespace griz::ui {

namespace {
constexpr int kResizeDebounceMs = 250;  // 04-client.md §6
constexpr int kFpsWindowSize    = 30;   // ~1 s at 30 Hz cap
}  // namespace

Viewport::Viewport(QWidget *parent)
    : QOpenGLWidget(parent),
      m_placeholderText(QStringLiteral("No connection — launch griz-server to begin.")) {
    setMinimumSize(480, 360);

    m_resizeTimer.setSingleShot(true);
    m_resizeTimer.setInterval(kResizeDebounceMs);
    connect(&m_resizeTimer, &QTimer::timeout,
            this, &Viewport::emitResizeRequest);

    m_fpsClock.start();
}

void Viewport::setPlaceholderText(const QString &text) {
    m_placeholderText = text;
    update();
}

void Viewport::flushPendingResize() {
    if (m_resizeTimer.isActive()) {
        m_resizeTimer.stop();
        emitResizeRequest();
    }
}

void Viewport::initializeGL() {}

void Viewport::resizeGL(int w, int h) {
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void Viewport::paintGL() {}

void Viewport::paintEvent(QPaintEvent *event) {
    QOpenGLWidget::paintEvent(event);

    QPainter painter(this);
    if (m_haveFrame && !m_frame.isNull()) {
        painter.fillRect(rect(), Qt::black);
        // Letterbox while the server still reports the pre-resize size.
        const QSize imgSize   = m_frame.size();
        const QSize widgetSz  = size();
        QSize scaled = imgSize;
        scaled.scale(widgetSz, Qt::KeepAspectRatio);
        const QRect target(
            (widgetSz.width()  - scaled.width())  / 2,
            (widgetSz.height() - scaled.height()) / 2,
            scaled.width(), scaled.height());
        painter.drawImage(target, m_frame);
        return;
    }
    painter.fillRect(rect(), QColor(32, 32, 36));
    painter.setPen(QColor(180, 180, 186));
    painter.drawText(rect(), Qt::AlignCenter, m_placeholderText);
}

void Viewport::resizeEvent(QResizeEvent *event) {
    QOpenGLWidget::resizeEvent(event);
    m_resizeTimer.start();
}

void Viewport::emitResizeRequest() {
    const QSize sz = size();
    if (sz.width() <= 0 || sz.height() <= 0) return;
    if (sz == m_lastRequestedSize) return;
    m_lastRequestedSize = sz;
    emit resizeRequested(sz.width(), sz.height());
}

void Viewport::onBinaryFrame(const QByteArray &payload) {
    // 02-protocol.md §3: subtype, codec, flags, reserved, uint16-BE hdrlen,
    // hdr_json, body. MVP only handles subtype=0x01 (frame) codec=0x01 (jpeg).
    if (payload.size() < 6) return;
    const auto *bytes = reinterpret_cast<const unsigned char *>(payload.constData());
    const unsigned char subtype = bytes[0];
    const unsigned char codec   = bytes[1];
    if (subtype != 0x01 || codec != 0x01) return;
    const int hdrLen = (int(bytes[4]) << 8) | int(bytes[5]);
    if (6 + hdrLen > payload.size()) return;

    quint64 frameSeq = 0;
    if (hdrLen > 0) {
        QJsonParseError err;
        const QJsonDocument hdrDoc = QJsonDocument::fromJson(
            payload.mid(6, hdrLen), &err);
        if (err.error == QJsonParseError::NoError && hdrDoc.isObject()) {
            frameSeq = static_cast<quint64>(
                hdrDoc.object().value(QStringLiteral("seq")).toDouble(0.0));
        }
    }

    const QByteArray body = payload.mid(6 + hdrLen);
    QImage image;
    if (!image.loadFromData(body, "JPEG")) return;

    m_frame        = std::move(image);
    m_lastFrameSeq = frameSeq;
    m_haveFrame    = true;
    recordFrameArrival(frameSeq);
    update();
}

void Viewport::recordFrameArrival(quint64 frameSeq) {
    Q_UNUSED(frameSeq);
    const qint64 now = m_fpsClock.elapsed();
    m_frameArrivalsMs.push_back(now);
    while (static_cast<int>(m_frameArrivalsMs.size()) > kFpsWindowSize) {
        m_frameArrivalsMs.pop_front();
    }
    if (m_frameArrivalsMs.size() < 2) {
        emit fpsChanged(0.0);
        return;
    }
    const qint64 dt = m_frameArrivalsMs.back() - m_frameArrivalsMs.front();
    if (dt <= 0) return;
    const double fps = 1000.0 * double(m_frameArrivalsMs.size() - 1) / double(dt);
    emit fpsChanged(fps);
}

} // namespace griz::ui
