#include "MaterialsDock.h"

#include <QColor>
#include <QHeaderView>
#include <QPixmap>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace griz::ui {

namespace {
enum Column { ColVisible = 0, ColEnabled = 1, ColColor = 2, ColLabel = 3 };
constexpr int kIdRole = Qt::UserRole + 0;
}  // namespace

MaterialsDock::MaterialsDock(QWidget *parent)
    : QDockWidget(tr("Materials"), parent) {
    setObjectName(QStringLiteral("MaterialsDock"));

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({
        tr("Vis"), tr("Enab"), tr(""), tr("Material"),
    });
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);

    auto *hdr = m_table->horizontalHeader();
    hdr->setSectionResizeMode(ColVisible, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(ColEnabled, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(ColColor,   QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(ColLabel,   QHeaderView::Stretch);

    setWidget(m_table);

    connect(m_table, &QTableWidget::itemChanged,
            this, &MaterialsDock::onItemChanged);
}

void MaterialsDock::setMaterials(const std::vector<griz::model::Material> &materials) {
    m_suppressEmit = true;
    m_table->setRowCount(0);
    if (materials.empty()) {
        m_suppressEmit = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(materials.size()));
    for (int row = 0; row < static_cast<int>(materials.size()); ++row) {
        const auto &m = materials[row];

        auto *vis = new QTableWidgetItem();
        vis->setFlags((vis->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsUserCheckable);
        vis->setCheckState(m.visible ? Qt::Checked : Qt::Unchecked);
        vis->setData(kIdRole, m.id);
        vis->setToolTip(tr("Visible — toggles vis/invis"));
        m_table->setItem(row, ColVisible, vis);

        auto *enab = new QTableWidgetItem();
        enab->setFlags((enab->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsUserCheckable);
        enab->setCheckState(m.enabled ? Qt::Checked : Qt::Unchecked);
        enab->setData(kIdRole, m.id);
        enab->setToolTip(tr("Enabled — toggles enable/disable"));
        m_table->setItem(row, ColEnabled, enab);

        auto *color = new QTableWidgetItem();
        color->setFlags(color->flags() & ~(Qt::ItemIsEditable | Qt::ItemIsUserCheckable));
        if (m.hasColor) {
            QPixmap pm(14, 14);
            pm.fill(QColor::fromRgbF(m.colorR, m.colorG, m.colorB));
            color->setData(Qt::DecorationRole, pm);
        }
        m_table->setItem(row, ColColor, color);

        const QString label = m.label.isEmpty()
            ? QStringLiteral("mat %1").arg(m.id)
            : m.label;
        auto *text = new QTableWidgetItem(
            QStringLiteral("%1: %2").arg(m.id).arg(label));
        text->setFlags(text->flags() & ~(Qt::ItemIsEditable | Qt::ItemIsUserCheckable));
        text->setData(kIdRole, m.id);
        m_table->setItem(row, ColLabel, text);
    }
    m_suppressEmit = false;
}

void MaterialsDock::onItemChanged(QTableWidgetItem *item) {
    if (m_suppressEmit || !item) return;
    const int col = item->column();
    if (col != ColVisible && col != ColEnabled) return;

    const int id = item->data(kIdRole).toInt();
    const bool on = (item->checkState() == Qt::Checked);

    QString cmd;
    if (col == ColVisible) {
        cmd = on ? QStringLiteral("vis %1").arg(id)
                 : QStringLiteral("invis %1").arg(id);
    } else {
        cmd = on ? QStringLiteral("enable %1").arg(id)
                 : QStringLiteral("disable %1").arg(id);
    }
    emit commandRequested(cmd);
}

} // namespace griz::ui
