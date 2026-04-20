#include "TimeSlider.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QWidget>

namespace griz::ui {

namespace {
QToolButton *makeXportButton(QWidget *parent,
                             QStyle::StandardPixmap icon,
                             const QString &tooltip) {
    auto *b = new QToolButton(parent);
    b->setIcon(parent->style()->standardIcon(icon));
    b->setAutoRaise(true);
    b->setToolTip(tooltip);
    return b;
}
}  // namespace

TimeSlider::TimeSlider(QWidget *parent)
    : QDockWidget(tr("Time"), parent) {
    setObjectName(QStringLiteral("TimeSlider"));

    auto *body   = new QWidget(this);
    auto *layout = new QHBoxLayout(body);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    m_btnFirst = makeXportButton(body, QStyle::SP_MediaSkipBackward,
                                 tr("First state (f)"));
    m_btnPrev  = makeXportButton(body, QStyle::SP_MediaSeekBackward,
                                 tr("Previous state (p)"));
    m_btnPlay  = makeXportButton(body, QStyle::SP_MediaPlay,
                                 tr("Play / pause animation (anim)"));
    m_btnPlay->setCheckable(true);
    m_btnNext  = makeXportButton(body, QStyle::SP_MediaSeekForward,
                                 tr("Next state (n)"));
    m_btnLast  = makeXportButton(body, QStyle::SP_MediaSkipForward,
                                 tr("Last state (l)"));

    m_slider = new QSlider(Qt::Horizontal, body);
    m_slider->setMinimum(1);
    m_slider->setMaximum(1);
    m_slider->setValue(1);
    m_slider->setEnabled(false);
    m_slider->setTracking(false);  // emit only on release — avoids a command flood

    m_label = new QLabel(tr("(no states)"), body);
    m_label->setMinimumWidth(200);

    layout->addWidget(m_btnFirst);
    layout->addWidget(m_btnPrev);
    layout->addWidget(m_btnPlay);
    layout->addWidget(m_btnNext);
    layout->addWidget(m_btnLast);
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_label);

    setWidget(body);

    connect(m_slider, &QSlider::valueChanged,
            this, &TimeSlider::onSliderValueChanged);
    connect(m_btnFirst, &QToolButton::clicked, this, [this]() {
        emit commandRequested(QStringLiteral("f"));
    });
    connect(m_btnPrev, &QToolButton::clicked, this, [this]() {
        emit commandRequested(QStringLiteral("p"));
    });
    connect(m_btnPlay, &QToolButton::clicked, this, [this]() {
        // `anim` is a toggle on the server side (start/stop on repeat
        // press). We let the checked state track the server's
        // animating flag via setTime(); clicking just fires the command.
        emit commandRequested(QStringLiteral("anim"));
    });
    connect(m_btnNext, &QToolButton::clicked, this, [this]() {
        emit commandRequested(QStringLiteral("n"));
    });
    connect(m_btnLast, &QToolButton::clicked, this, [this]() {
        emit commandRequested(QStringLiteral("l"));
    });
}

void TimeSlider::setTime(const griz::model::TimeState &time) {
    m_suppressEmit = true;
    const int lo = time.stateMin > 0 ? time.stateMin : 1;
    const int hi = time.stateMax > lo ? time.stateMax : lo;
    m_slider->setRange(lo, hi);
    const int clamped = qBound(lo, time.state > 0 ? time.state : lo, hi);
    m_slider->setValue(clamped);
    const bool haveRange = (hi > lo);
    m_slider->setEnabled(haveRange);

    m_btnFirst->setEnabled(haveRange);
    m_btnPrev ->setEnabled(haveRange);
    m_btnNext ->setEnabled(haveRange);
    m_btnLast ->setEnabled(haveRange);
    m_btnPlay ->setEnabled(haveRange);
    QSignalBlocker playBlock(m_btnPlay);
    m_btnPlay->setChecked(time.animating);
    m_btnPlay->setIcon(style()->standardIcon(
        time.animating ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));

    if (!haveRange) {
        m_label->setText(tr("(1 state)  t=%1").arg(time.time, 0, 'g', 4));
    } else {
        m_label->setText(tr("state %1 / %2  t=%3  (%4..%5)")
            .arg(clamped).arg(hi)
            .arg(time.time,    0, 'g', 4)
            .arg(time.timeMin, 0, 'g', 4)
            .arg(time.timeMax, 0, 'g', 4));
    }
    m_suppressEmit = false;
}

void TimeSlider::onSliderValueChanged(int value) {
    if (m_suppressEmit) return;
    emit stateRequested(value);
}

} // namespace griz::ui
