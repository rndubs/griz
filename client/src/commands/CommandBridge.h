#pragma once

#include <QObject>
#include <QPoint>
#include <QString>

#include "net/Worker.h"

// Translates viewport input events into Griz RPC commands. Installs itself
// as an event filter on the supplied watched widget (typically the Viewport).
//
// Per planning/ui-design/04-client.md §6 + planning/UI.md Phase 5 bullet 8:
//   * left drag              → rx / ry (rotate)          [0.25 deg / px]
//   * shift + left drag      → tx / ty (translate)       [0.002 / px]
//   * middle drag            → tx / ty (translate)       [0.002 / px]
//   * wheel                  → zf / zb (zoom)            [×1.1 per notch]
//   * left click (no drag)   → pick_at <x> <y>
//   * R key                  → rview
//
// Rate limiting: one command in flight at a time. Mouse-move deltas accumulate
// into a pending vector until the previous command's response arrives; the
// pending delta is then flushed as a single new command. Wheel / click / R
// are simply dropped while a prior command is unacked — the user can reissue.

namespace griz::commands {

class CommandBridge : public QObject {
    Q_OBJECT
public:
    // `worker` stays owned by the caller (non-owning pointer). The bridge
    // installs an event filter on `watched` and listens to the worker's
    // responseReceived signal to track the in-flight command.
    CommandBridge(griz::net::Worker *worker,
                  QWidget           *watched,
                  QObject           *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onResponseReceived(const griz::net::Response &response);
    void onWorkerDisconnected(const QString &reason);

private:
    enum class DragMode { None, Rotate, Translate };

    // Returns true iff a new command was actually sent (i.e. not gated by
    // the in-flight check or missing worker connection).
    bool trySend(const QString &cmd);

    // Coalesce accumulated drag delta into a single rx/ry or tx/ty command.
    void flushPendingDrag();

    // Accumulate a mouse-move delta into the pending vector, then try to
    // flush. Called while a left/middle button is held and moving.
    void accumulateDrag(int dx, int dy);

    griz::net::Worker *m_worker = nullptr;
    QWidget           *m_watched = nullptr;

    // Drag state. Reset on mouse-press; inspected on mouse-release to
    // decide click-vs-drag.
    bool     m_dragging      = false;
    DragMode m_dragMode      = DragMode::None;
    QPoint   m_pressPos;
    QPoint   m_lastPos;
    int      m_dragManhattan = 0;

    // Pending coalesced motion delta (pixels).
    double m_pendingDx = 0.0;
    double m_pendingDy = 0.0;

    // Request id of the outstanding command (empty == idle).
    QString m_inflightId;
};

} // namespace griz::commands
