#include "MaterialsDock.h"

#include <QColor>
#include <QListWidget>
#include <QListWidgetItem>

namespace griz::ui {

MaterialsDock::MaterialsDock(QWidget *parent)
    : QDockWidget(tr("Materials"), parent) {
    setObjectName(QStringLiteral("MaterialsDock"));
    m_list = new QListWidget(this);
    m_list->setUniformItemSizes(true);
    m_list->addItem(tr("(no database loaded)"));
    setWidget(m_list);
}

void MaterialsDock::setMaterials(const std::vector<griz::model::Material> &materials) {
    m_list->clear();
    if (materials.empty()) {
        m_list->addItem(tr("(no materials)"));
        return;
    }
    for (const auto &m : materials) {
        QString flags;
        if (!m.visible) flags += QStringLiteral(" [hidden]");
        if (!m.enabled) flags += QStringLiteral(" [disabled]");
        const QString label = m.label.isEmpty()
            ? QStringLiteral("mat %1").arg(m.id)
            : m.label;
        auto *item = new QListWidgetItem(
            QStringLiteral("%1: %2%3").arg(m.id).arg(label, flags));
        if (m.hasColor) {
            item->setData(Qt::DecorationRole,
                QColor::fromRgbF(m.colorR, m.colorG, m.colorB));
        }
        m_list->addItem(item);
    }
}

} // namespace griz::ui
