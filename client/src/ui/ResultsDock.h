#pragma once

#include <QDockWidget>

#include "model/SessionState.h"

// Current-result summary dock. Read-only in MVP — shows the active result's
// primary/component/griz_name and the live min/max from the server's
// q_state `results.active` payload, driven by SessionState::resultsChanged.

class QLabel;

namespace griz::ui {

class ResultsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit ResultsDock(QWidget *parent = nullptr);

public slots:
    void setResults(const griz::model::ResultsState &results);

private:
    QLabel *m_primaryLabel   = nullptr;
    QLabel *m_componentLabel = nullptr;
    QLabel *m_grizNameLabel  = nullptr;
    QLabel *m_minLabel       = nullptr;
    QLabel *m_maxLabel       = nullptr;
    QLabel *m_placeholder    = nullptr;
};

} // namespace griz::ui
