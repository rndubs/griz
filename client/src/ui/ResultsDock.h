#pragma once

#include <QDockWidget>
#include <QString>

#include "model/SessionState.h"

// Current-result picker + summary dock. QComboBox listing the full
// q_state.results.results catalog on top; picking an entry emits
// commandRequested("show <name>"). Below the combo, the summary panel
// shows the active result's field/component/griz_name/min/max, driven
// by SessionState::resultsChanged.

class QComboBox;
class QLabel;

namespace griz::ui {

class ResultsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit ResultsDock(QWidget *parent = nullptr);

public slots:
    void setResults(const griz::model::ResultsState &results);

signals:
    // User picked a new result. App wires this to Worker::sendCommand.
    void commandRequested(const QString &cmd);

private slots:
    void onComboActivated(int index);

private:
    QComboBox *m_combo = nullptr;
    QLabel    *m_primaryLabel   = nullptr;
    QLabel    *m_componentLabel = nullptr;
    QLabel    *m_grizNameLabel  = nullptr;
    QLabel    *m_minLabel       = nullptr;
    QLabel    *m_maxLabel       = nullptr;
    QLabel    *m_placeholder    = nullptr;

    // Suppress the activated signal while we're mirroring server state.
    bool       m_suppressEmit = false;
};

} // namespace griz::ui
