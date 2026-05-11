#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include <deque>

// Streamed-frame viewport. See planning/ui-design/04-client.md §6 and
// planning/ui-design/02-protocol.md §3 (binary sub-header) / §4 (frame_seq).
// Decodes kind=0x02 subtype=0x01 codec=0x01 JPEG frames, tracks the
// monotonic frame_seq for an FPS estimate, and debounces widget-resize
// events into a `resize` RPC per 04-client.md §6 (250 ms quiescence).

namespace griz::ui {

class Viewport : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit Viewport(QWidget *parent = nullptr);

    void setPlaceholderText(const QString &text);

    // Emit resizeRequested() immediately on the next resize tick instead
    // of waiting for the debounce — used once the handshake completes so
    // the server's initial framebuffer matches the actual widget size.
    void flushPendingResize();

public slots:
    // Route a kind=0x02 binary frame payload from net::Worker::frameReceived.
    // Parses the 6-byte sub-header (subtype/codec/flags/reserved + uint16
    // header length), decodes JPEG via QImage, and repaints.
    void onBinaryFrame(const QByteArray &payload);

signals:
    // Debounced widget-resize → server resize. App wires this to
    // Worker::sendCommand("resize", {w, h}).
    void resizeRequested(int w, int h);

    // Rolling FPS computed from frame arrival timestamps. App wires this
    // to MainWindow::setFpsLabel.
    void fpsChanged(double fps);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void emitResizeRequest();

private:
    void recordFrameArrival(quint64 frameSeq);

    QString              m_placeholderText;
    QImage               m_frame;
    quint64              m_lastFrameSeq = 0;
    bool                 m_haveFrame    = false;

    QTimer               m_resizeTimer;   // 250 ms debounce
    QSize                m_lastRequestedSize;

    QElapsedTimer        m_fpsClock;
    std::deque<qint64>   m_frameArrivalsMs;
};

} // namespace griz::ui
