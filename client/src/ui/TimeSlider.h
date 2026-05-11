#pragma once

#include <QDockWidget>

#include "model/SessionState.h"

// Time-state scrubber dock per planning/ui-design/04-client.md §4.3.
// Drives a horizontal QSlider bound to state_min..state_max (1-based) with
// a label reporting `state N / M  t=X (min..max)`. Transport buttons
// (⏮ ⏸/▶ ⏭ and step back/forward) map to the classic first/last/prev/next/
// anim commands in Src/interpret.c. User-initiated slider moves emit
// stateRequested(N) which App routes to Worker::sendCommand("state N");
// the return trip via SessionState::timeChanged updates the slider with
// signals suppressed to avoid a feedback loop.

class QLabel;
class QSlider;
class QToolButton;

namespace griz::ui {

class TimeSlider : public QDockWidget {
    Q_OBJECT
public:
    explicit TimeSlider(QWidget *parent = nullptr);

public slots:
    void setTime(const griz::model::TimeState &time);

signals:
    // User moved the slider. N is 1-based (matches `state <N>` command).
    void stateRequested(int state);

    // Transport button pressed. App wires this to Worker::sendCommand.
    // Commands: "f" / "l" / "n" / "p" / "anim".
    void commandRequested(const QString &cmd);

private slots:
    void onSliderValueChanged(int value);

private:
    QSlider     *m_slider  = nullptr;
    QLabel      *m_label   = nullptr;
    QToolButton *m_btnFirst = nullptr;
    QToolButton *m_btnPrev  = nullptr;
    QToolButton *m_btnPlay  = nullptr;
    QToolButton *m_btnNext  = nullptr;
    QToolButton *m_btnLast  = nullptr;
    bool         m_suppressEmit = false;   // set while mirroring server state
};

} // namespace griz::ui
