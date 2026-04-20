#include "SelectionDock.h"

#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace griz::ui {

namespace {
constexpr int kKindRole = Qt::UserRole + 0;
constexpr int kIdRole   = Qt::UserRole + 1;

void populate(QListWidget *list,
              const std::vector<griz::model::PickedItem> &items) {
    list->clear();
    if (items.empty()) {
        auto *placeholder = new QListWidgetItem(QObject::tr("(none)"));
        placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsSelectable
                                                  & ~Qt::ItemIsEnabled);
        list->addItem(placeholder);
        return;
    }
    for (const auto &item : items) {
        const QString text = QStringLiteral("%1 %2")
            .arg(item.kind).arg(item.id);
        auto *lwi = new QListWidgetItem(text);
        lwi->setData(kKindRole, item.kind);
        lwi->setData(kIdRole,   item.id);
        lwi->setToolTip(QObject::tr("Click to re-hilite this %1.")
                        .arg(item.kind));
        list->addItem(lwi);
    }
}
}  // namespace

SelectionDock::SelectionDock(QWidget *parent)
    : QDockWidget(tr("Selection"), parent) {
    setObjectName(QStringLiteral("SelectionDock"));

    auto *body   = new QWidget(this);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    m_hilightedLabel = new QLabel(tr("hilite: (none)"), body);
    m_elementsHeader = new QLabel(tr("Elements (0):"), body);
    m_elementsList   = new QListWidget(body);
    m_elementsList->setUniformItemSizes(true);
    m_nodesHeader    = new QLabel(tr("Nodes (0):"), body);
    m_nodesList      = new QListWidget(body);
    m_nodesList->setUniformItemSizes(true);

    layout->addWidget(m_hilightedLabel);
    layout->addWidget(m_elementsHeader);
    layout->addWidget(m_elementsList, 1);
    layout->addWidget(m_nodesHeader);
    layout->addWidget(m_nodesList, 1);

    populate(m_elementsList, {});
    populate(m_nodesList,    {});

    setWidget(body);

    connect(m_elementsList, &QListWidget::itemClicked,
            this, &SelectionDock::onItemClicked);
    connect(m_nodesList, &QListWidget::itemClicked,
            this, &SelectionDock::onItemClicked);
}

void SelectionDock::setSelection(const griz::model::Selection &selection) {
    m_hilightedLabel->setText(selection.hasHighlighted
        ? tr("hilite: one object")
        : tr("hilite: (none)"));
    m_elementsHeader->setText(tr("Elements (%1):")
        .arg(selection.elements.size()));
    m_nodesHeader->setText(tr("Nodes (%1):")
        .arg(selection.nodes.size()));
    populate(m_elementsList, selection.elements);
    populate(m_nodesList,    selection.nodes);
}

void SelectionDock::onItemClicked(QListWidgetItem *item) {
    if (!item) return;
    const QString kind = item->data(kKindRole).toString();
    const int     id   = item->data(kIdRole).toInt();
    if (kind.isEmpty() || id <= 0) return;
    emit commandRequested(QStringLiteral("hilite %1 %2").arg(kind).arg(id));
}

} // namespace griz::ui
