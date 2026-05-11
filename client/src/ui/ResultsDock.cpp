#include "ResultsDock.h"

#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace griz::ui {

namespace {
constexpr int kNameRole = Qt::UserRole + 0;
}  // namespace

ResultsDock::ResultsDock(QWidget *parent)
    : QDockWidget(tr("Results"), parent) {
    setObjectName(QStringLiteral("ResultsDock"));

    auto *body   = new QWidget(this);
    auto *vbox   = new QVBoxLayout(body);
    vbox->setContentsMargins(8, 4, 8, 4);
    vbox->setSpacing(4);

    m_combo = new QComboBox(body);
    m_combo->setEditable(true);
    m_combo->setInsertPolicy(QComboBox::NoInsert);
    m_combo->setToolTip(tr("Type to filter; picking runs `show <name>`."));
    m_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_combo->setMinimumContentsLength(20);
    if (auto *c = m_combo->completer()) {
        c->setCaseSensitivity(Qt::CaseInsensitive);
        c->setFilterMode(Qt::MatchContains);
    }
    vbox->addWidget(m_combo);

    auto *form = new QFormLayout();
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

    vbox->addLayout(form);
    vbox->addStretch(1);

    m_primaryLabel->hide();
    m_componentLabel->hide();
    m_grizNameLabel->hide();
    m_minLabel->hide();
    m_maxLabel->hide();

    setWidget(body);

    connect(m_combo, qOverload<int>(&QComboBox::activated),
            this, &ResultsDock::onComboActivated);
}

void ResultsDock::setResults(const griz::model::ResultsState &results) {
    m_suppressEmit = true;
    {
        QSignalBlocker block(m_combo);
        m_combo->clear();
        m_combo->addItem(tr("(pick a result…)"), QString());
        for (const auto &r : results.available) {
            const QString display = r.title.isEmpty()
                ? r.name
                : QStringLiteral("%1  (%2)").arg(r.name, r.title);
            m_combo->addItem(display, r.name);
        }
        int selected = 0;
        if (!results.currentName.isEmpty()) {
            const int idx = m_combo->findData(results.currentName);
            if (idx > 0) selected = idx;
        }
        m_combo->setCurrentIndex(selected);
    }
    m_suppressEmit = false;

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

void ResultsDock::onComboActivated(int index) {
    if (m_suppressEmit || index <= 0) return;
    const QString name = m_combo->itemData(index).toString();
    if (name.isEmpty()) return;
    emit commandRequested(QStringLiteral("show %1").arg(name));
}

} // namespace griz::ui
