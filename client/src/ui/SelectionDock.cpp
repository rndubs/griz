#include "SelectionDock.h"

#include <QLabel>

namespace griz::ui {

SelectionDock::SelectionDock(QWidget *parent)
    : QDockWidget(tr("Selection"), parent) {
    setObjectName(QStringLiteral("SelectionDock"));
    auto *placeholder = new QLabel(tr("(nothing selected)"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setMargin(12);
    setWidget(placeholder);
}

} // namespace griz::ui
