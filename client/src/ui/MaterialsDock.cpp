#include "MaterialsDock.h"

#include <QLabel>

namespace griz::ui {

MaterialsDock::MaterialsDock(QWidget *parent)
    : QDockWidget(tr("Materials"), parent) {
    setObjectName(QStringLiteral("MaterialsDock"));
    auto *placeholder = new QLabel(tr("(no database loaded)"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setMargin(12);
    setWidget(placeholder);
}

} // namespace griz::ui
