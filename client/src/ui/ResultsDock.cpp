#include "ResultsDock.h"

#include <QLabel>

namespace griz::ui {

ResultsDock::ResultsDock(QWidget *parent)
    : QDockWidget(tr("Results"), parent) {
    setObjectName(QStringLiteral("ResultsDock"));
    auto *placeholder = new QLabel(tr("(no result selected)"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setMargin(12);
    setWidget(placeholder);
}

} // namespace griz::ui
