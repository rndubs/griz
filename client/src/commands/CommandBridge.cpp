#include "CommandBridge.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>

#include <cmath>

namespace griz::commands {

namespace {
constexpr double kRotDegPerPx   = 0.25;   // one full turn per ~1440 px
constexpr double kTransPerPx    = 0.002;  // translate scale (world units / px)
constexpr double kZoomPerNotch  = 1.10;   // 10% per 120-unit wheel notch
constexpr int    kClickSlopPx   = 3;      // move threshold for click-vs-drag
}  // namespace

CommandBridge::CommandBridge(griz::net::Worker *worker,
                             QWidget           *watched,
                             QObject           *parent)
    : QObject(parent), m_worker(worker), m_watched(watched) {
    if (m_watched) {
        m_watched->installEventFilter(this);
        m_watched->setFocusPolicy(Qt::StrongFocus);
    }
    if (m_worker) {
        connect(m_worker, &griz::net::Worker::responseReceived,
                this, &CommandBridge::onResponseReceived);
        connect(m_worker, &griz::net::Worker::disconnected,
                this, &CommandBridge::onWorkerDisconnected);
    }
}

bool CommandBridge::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_watched) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton || me->button() == Qt::MiddleButton) {
            m_dragging      = true;
            m_pressPos      = me->pos();
            m_lastPos       = me->pos();
            m_dragManhattan = 0;
            m_pendingDx     = 0.0;
            m_pendingDy     = 0.0;
            const bool shift = (me->modifiers() & Qt::ShiftModifier) != 0;
            m_dragMode = (me->button() == Qt::MiddleButton || shift)
                ? DragMode::Translate
                : DragMode::Rotate;
            m_watched->setFocus(Qt::MouseFocusReason);
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        if (!m_dragging) return false;
        auto *me = static_cast<QMouseEvent *>(event);
        const QPoint delta = me->pos() - m_lastPos;
        m_lastPos = me->pos();
        m_dragManhattan += std::abs(delta.x()) + std::abs(delta.y());
        accumulateDrag(delta.x(), delta.y());
        return true;
    }
    case QEvent::MouseButtonRelease: {
        if (!m_dragging) return false;
        auto *me = static_cast<QMouseEvent *>(event);
        const bool wasDrag = (m_dragManhattan > kClickSlopPx);
        const bool wasLeft = (me->button() == Qt::LeftButton);
        m_dragging = false;
        if (wasDrag) {
            // Flush any trailing motion from the last frame's worth of
            // moves now that the drag ended.
            flushPendingDrag();
        } else if (wasLeft) {
            const QPoint p = me->pos();
            trySend(QStringLiteral("pick_at %1 %2 any").arg(p.x()).arg(p.y()));
        }
        m_dragMode      = DragMode::None;
        m_dragManhattan = 0;
        return true;
    }
    case QEvent::Wheel: {
        auto *we = static_cast<QWheelEvent *>(event);
        const int dy = we->angleDelta().y();
        if (dy == 0) return false;
        const double steps  = dy / 120.0;
        const double factor = std::pow(kZoomPerNotch, std::abs(steps));
        const QString cmd = (steps > 0)
            ? QStringLiteral("zf %1").arg(factor, 0, 'f', 4)
            : QStringLiteral("zb %1").arg(factor, 0, 'f', 4);
        trySend(cmd);
        return true;
    }
    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        const bool plain = !(ke->modifiers()
            & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        if (!plain) break;
        switch (ke->key()) {
        case Qt::Key_R:
            trySend(QStringLiteral("rview"));
            return true;
        case Qt::Key_P:
            m_orthoActive = !m_orthoActive;
            trySend(m_orthoActive
                ? QStringLiteral("switch ortho")
                : QStringLiteral("switch persp"));
            return true;
        case Qt::Key_1:
            // Front view: reset to the model's default orientation.
            trySend(QStringLiteral("rview"));
            return true;
        case Qt::Key_2:
            // Right side: reset + yaw 90° around Y. Compound ';' command
            // is dispatched by parse_command() as two sequential steps.
            trySend(QStringLiteral("rview; ry 90"));
            return true;
        case Qt::Key_3:
            // Top-down: reset + pitch -90° around X so +Y points into screen.
            trySend(QStringLiteral("rview; rx -90"));
            return true;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void CommandBridge::accumulateDrag(int dx, int dy) {
    m_pendingDx += dx;
    m_pendingDy += dy;
    flushPendingDrag();
}

void CommandBridge::flushPendingDrag() {
    if (!m_inflightId.isEmpty()) return;          // drop-newest: keep coalescing
    if (m_pendingDx == 0.0 && m_pendingDy == 0.0) return;
    if (m_dragMode == DragMode::None) return;

    QString cmd;
    switch (m_dragMode) {
    case DragMode::Rotate: {
        // Horizontal drag → yaw around Y; vertical drag → pitch around X
        // (invert so dragging up tilts the model up).
        const double deg_y =  m_pendingDx * kRotDegPerPx;
        const double deg_x = -m_pendingDy * kRotDegPerPx;
        // Pick the dominant axis — sending both would need two queued
        // commands. The non-dominant axis survives in the accumulator and
        // flushes on the next idle tick.
        if (std::abs(m_pendingDx) >= std::abs(m_pendingDy)) {
            cmd = QStringLiteral("ry %1").arg(deg_y, 0, 'f', 3);
            m_pendingDx = 0.0;
        } else {
            cmd = QStringLiteral("rx %1").arg(deg_x, 0, 'f', 3);
            m_pendingDy = 0.0;
        }
        break;
    }
    case DragMode::Translate: {
        const double tx =  m_pendingDx * kTransPerPx;
        const double ty = -m_pendingDy * kTransPerPx;
        if (std::abs(m_pendingDx) >= std::abs(m_pendingDy)) {
            cmd = QStringLiteral("tx %1").arg(tx, 0, 'f', 4);
            m_pendingDx = 0.0;
        } else {
            cmd = QStringLiteral("ty %1").arg(ty, 0, 'f', 4);
            m_pendingDy = 0.0;
        }
        break;
    }
    case DragMode::None:
        return;
    }

    (void) trySend(cmd);
}

bool CommandBridge::trySend(const QString &cmd) {
    if (!m_worker || !m_worker->isConnected()) return false;
    if (!m_inflightId.isEmpty()) return false;
    m_inflightId = m_worker->sendCommand(cmd);
    return true;
}

void CommandBridge::onResponseReceived(const griz::net::Response &response) {
    if (response.requestId.isEmpty() || response.requestId != m_inflightId) {
        return;
    }
    m_inflightId.clear();
    flushPendingDrag();
}

void CommandBridge::onWorkerDisconnected(const QString &reason) {
    Q_UNUSED(reason);
    m_inflightId.clear();
    m_pendingDx = 0.0;
    m_pendingDy = 0.0;
    m_dragging  = false;
    m_dragMode  = DragMode::None;
}

} // namespace griz::commands
