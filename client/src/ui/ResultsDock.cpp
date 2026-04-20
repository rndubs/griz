#include "ResultsDock.h"

#include <QFormLayout>
#include <QLabel>
#include <QWidget>

namespace griz::ui {

ResultsDock::ResultsDock(QWidget *parent)
    : QDockWidget(tr("Results"), parent) {
    setObjectName(QStringLiteral("ResultsDock"));

    auto *body = new QWidget(this);
    auto *form = new QFormLayout(body);
    form->setContentsMargins(8, 4, 8, 4);
    form->setLabelAlignment(Qt::AlignRight);

    m_placeholder    = new QLabel(tr("(no result selected)"), body);
    m_primaryLabel   = new QLabel(tr("—"), body);
    m_componentLabel = new QLabel(tr("—"), body);
    m_grizNameLabel  = new QLabel(tr("—"), body);
    m_minLabel       = new QLabel(tr("—"), body);
    m_maxLabel       = new QLabel(tr("—"), body);

    form->addRow(m_placeholder);
    form->addRow(tr("primary:"),   m_primaryLabel);
    form->addRow(tr("component:"), m_componentLabel);
    form->addRow(tr("griz name:"), m_grizNameLabel);
    form->addRow(tr("min:"),       m_minLabel);
    form->addRow(tr("max:"),       m_maxLabel);

    m_primaryLabel->hide();
    m_componentLabel->hide();
    m_grizNameLabel->hide();
    m_minLabel->hide();
    m_maxLabel->hide();

    setWidget(body);
}

void ResultsDock::setResults(const griz::model::ResultsState &results) {
    const bool active = results.hasActive;
    m_placeholder->setVisible(!active);
    m_primaryLabel->setVisible(active);
    m_componentLabel->setVisible(active);
    m_grizNameLabel->setVisible(active);
    m_minLabel->setVisible(active);
    m_maxLabel->setVisible(active);
    if (!active) return;

    m_primaryLabel  ->setText(results.primary.isEmpty()   ? tr("—") : results.primary);
    m_componentLabel->setText(results.component.isEmpty() ? tr("—") : results.component);
    m_grizNameLabel ->setText(results.grizName.isEmpty()  ? tr("—") : results.grizName);
    m_minLabel      ->setText(QString::number(results.min, 'g', 6));
    m_maxLabel      ->setText(QString::number(results.max, 'g', 6));
}

} // namespace griz::ui
