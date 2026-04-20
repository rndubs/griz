#pragma once

#include <QDockWidget>
#include <QString>

#include "model/SessionState.h"

// Selection table dock per planning/ui-design/04-client.md §7.
// Two lists (elements / nodes) mirror SessionState::selection. Clicking
// a row emits commandRequested("hilite <kind> <id>") so App can re-fire
// the hilite for the clicked item — classic Griz behavior.

class QLabel;
class QListWidget;
class QListWidgetItem;

namespace griz::ui {

class SelectionDock : public QDockWidget {
    Q_OBJECT
public:
    explicit SelectionDock(QWidget *parent = nullptr);

public slots:
    void setSelection(const griz::model::Selection &selection);

signals:
    // Emitted when the user clicks a row. App wires this to
    // Worker::sendCommand so the pick re-highlights on the server.
    void commandRequested(const QString &cmd);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    QLabel      *m_hilightedLabel = nullptr;
    QLabel      *m_elementsHeader = nullptr;
    QListWidget *m_elementsList   = nullptr;
    QLabel      *m_nodesHeader    = nullptr;
    QListWidget *m_nodesList      = nullptr;
};

} // namespace griz::ui
