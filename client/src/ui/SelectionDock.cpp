#include "SelectionDock.h"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace griz::ui {

namespace {
void populate(QListWidget *list, const std::vector<int> &ids) {
    list->clear();
    if (ids.empty()) {
        list->addItem(QObject::tr("(none)"));
        return;
    }
    for (int id : ids) {
        list->addItem(QString::number(id));
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

} // namespace griz::ui
