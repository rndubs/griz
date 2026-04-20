#include "TimeSlider.h"

#include <QLabel>

namespace griz::ui {

TimeSlider::TimeSlider(QWidget *parent)
    : QDockWidget(tr("Time"), parent) {
    setObjectName(QStringLiteral("TimeSlider"));
    auto *placeholder = new QLabel(tr("(no states loaded)"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setMargin(12);
    setWidget(placeholder);
}

} // namespace griz::ui
