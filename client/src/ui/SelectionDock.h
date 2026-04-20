#pragma once

#include <QDockWidget>

#include "model/SessionState.h"

// Selection table dock per planning/ui-design/04-client.md §7.
// Two lists (elements / nodes) mirroring SessionState::selection. Read-only
// for MVP — driving a pick_at from a click happens through CommandBridge,
// and manual `hilite`/`select` edits still go through the Console.

class QLabel;
class QListWidget;

namespace griz::ui {

class SelectionDock : public QDockWidget {
    Q_OBJECT
public:
    explicit SelectionDock(QWidget *parent = nullptr);

public slots:
    void setSelection(const griz::model::Selection &selection);

private:
    QLabel      *m_elementsHeader = nullptr;
    QListWidget *m_elementsList   = nullptr;
    QLabel      *m_nodesHeader    = nullptr;
    QListWidget *m_nodesList      = nullptr;
    QLabel      *m_hilightedLabel = nullptr;
};

} // namespace griz::ui
