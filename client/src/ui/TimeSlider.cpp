#include "TimeSlider.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QWidget>

namespace griz::ui {

TimeSlider::TimeSlider(QWidget *parent)
    : QDockWidget(tr("Time"), parent) {
    setObjectName(QStringLiteral("TimeSlider"));

    auto *body   = new QWidget(this);
    auto *layout = new QHBoxLayout(body);
    layout->setContentsMargins(8, 4, 8, 4);

    m_slider = new QSlider(Qt::Horizontal, body);
    m_slider->setMinimum(1);
    m_slider->setMaximum(1);
    m_slider->setValue(1);
    m_slider->setEnabled(false);
    m_slider->setTracking(false);  // emit only on release — avoids a command flood

    m_label = new QLabel(tr("(no states)"), body);
    m_label->setMinimumWidth(140);

    layout->addWidget(m_slider, 1);
    layout->addWidget(m_label);

    setWidget(body);

    connect(m_slider, &QSlider::valueChanged,
            this, &TimeSlider::onSliderValueChanged);
}

void TimeSlider::setTime(const griz::model::TimeState &time) {
    m_suppressEmit = true;
    const int lo = time.stateMin > 0 ? time.stateMin : 1;
    const int hi = time.stateMax > lo ? time.stateMax : lo;
    m_slider->setRange(lo, hi);
    const int clamped = qBound(lo, time.state > 0 ? time.state : lo, hi);
    m_slider->setValue(clamped);
    m_slider->setEnabled(hi > lo);
    m_suppressEmit = false;

    if (hi <= lo) {
        m_label->setText(tr("(1 state)  t=%1").arg(time.time, 0, 'g', 4));
    } else {
        m_label->setText(tr("state %1 / %2  t=%3")
            .arg(clamped).arg(hi).arg(time.time, 0, 'g', 4));
    }
}

void TimeSlider::onSliderValueChanged(int value) {
    if (m_suppressEmit) return;
    emit stateRequested(value);
}

} // namespace griz::ui
