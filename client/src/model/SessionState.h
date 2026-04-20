#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <vector>

// In-memory mirror of the server's state dict. Seeded by q_state, kept fresh by
// state_changed events. Schema follows planning/shared/query-commands.md exactly;
// see planning/ui-design/04-client.md §4.

namespace griz::model {

struct DatabaseInfo {
    QString path;
    QString title;
    int     numStates = 0;
    int     numNodes  = 0;
    int     numElems  = 0;
};

struct TimeState {
    int    state     = 0;
    int    stateMin  = 0;
    int    stateMax  = 0;
    double time      = 0.0;
    double timeMin   = 0.0;
    double timeMax   = 0.0;
    bool   animating = false;
};

struct ViewState {
    double rotateX  = 0.0, rotateY  = 0.0, rotateZ  = 0.0;
    double translateX = 0.0, translateY = 0.0, translateZ = 0.0;
    double scaleX   = 1.0, scaleY   = 1.0, scaleZ   = 1.0;
    double zoom     = 1.0;
    int    width    = 0;
    int    height   = 0;
};

struct RenderState {
    QString mode;
    QString colormap;
    bool    showCoord  = false;
    bool    showTime   = false;
    bool    showCmap   = false;
    bool    showMinMax = false;
};

struct Material {
    int     id      = 0;
    QString label;
    bool    visible = true;
    bool    enabled = true;
    double  colorR  = 0.0;
    double  colorG  = 0.0;
    double  colorB  = 0.0;
    bool    hasColor = false;
};

// One result entry from q_state.results.results[] (and q_results).
// Matches server_query.c::build_q_results output: flat name + title +
// origin, where origin is "primal" | "derived". No primary/component
// split on the wire today — that's a post-MVP schema refinement.
struct ResultItem {
    QString name;     // wire token for `show <name>`
    QString title;    // human-readable label for the combo
    QString origin;   // "primal" or "derived"
};

struct ResultsState {
    QString primary;       // active.field
    QString component;     // active.component
    QString grizName;      // active.griz_name
    double  min = 0.0;
    double  max = 0.0;
    bool    hasActive = false;

    // Full catalog for the picker UI. Populated from q_state.results.results.
    std::vector<ResultItem> available;
    // Current active result name (from q_state.results.current.name) —
    // used to preselect the combo. Empty when no result is active.
    QString currentName;
};

// One entry in q_selection.picked — the server's short class name (e.g.
// "node", "brick", "hex", "quad") plus its user-facing id. Kept around
// so SelectionDock can issue `hilite <kind> <id>` without another query.
struct PickedItem {
    QString kind;
    int     id = 0;
};

struct Selection {
    std::vector<PickedItem> elements;
    std::vector<PickedItem> nodes;
    bool                    hasHighlighted = false;
};

class SessionState : public QObject {
    Q_OBJECT
public:
    explicit SessionState(QObject *parent = nullptr);

    const DatabaseInfo&            database()   const { return m_database; }
    const TimeState&               time()       const { return m_time; }
    const ViewState&               view()       const { return m_view; }
    const RenderState&             render()     const { return m_render; }
    const std::vector<Material>&   materials()  const { return m_materials; }
    const ResultsState&            results()    const { return m_results; }
    const Selection&               selection()  const { return m_selection; }
    quint64                        lastSeq()    const { return m_lastSeq; }

    // Apply a full q_state response.data; emits every *Changed signal.
    void applyFullState(const QJsonObject &qStateData);

    // Apply a state_changed diff payload (the event's "fields" object, plus
    // state_seq from the envelope). Nested objects under "fields" are
    // replacements, not merges (see planning/ui-design/04-client.md §4.2).
    void applyDiff(const QJsonObject &stateChangedEvent);

signals:
    void databaseChanged(const griz::model::DatabaseInfo &);
    void timeChanged(const griz::model::TimeState &);
    void viewChanged(const griz::model::ViewState &);
    void renderChanged(const griz::model::RenderState &);
    void materialsChanged();
    void resultsChanged(const griz::model::ResultsState &);
    void selectionChanged(const griz::model::Selection &);
    void stateOverflow();

private:
    void applyDatabase(const QJsonObject &obj);
    void applyTime(const QJsonObject &obj);
    void applyView(const QJsonObject &obj);
    void applyRender(const QJsonObject &obj);
    void applyMaterials(const QJsonObject &obj);
    void applyResults(const QJsonObject &obj);
    void applySelection(const QJsonObject &obj);

    DatabaseInfo          m_database;
    TimeState             m_time;
    ViewState             m_view;
    RenderState           m_render;
    std::vector<Material> m_materials;
    ResultsState          m_results;
    Selection             m_selection;
    quint64               m_lastSeq = 0;
};

} // namespace griz::model
