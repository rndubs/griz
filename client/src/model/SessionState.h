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
};

struct TimeState {
    int    stateIndex = 0;
    double value      = 0.0;
    int    numStates  = 0;
};

struct ViewState {
    double azimuth   = 0.0;
    double elevation = 0.0;
    double zoom      = 1.0;
};

struct RenderState {
    bool onTime = false;
    bool onCmap = false;
};

struct Material {
    int     id      = 0;
    QString name;
    bool    visible = true;
    bool    enabled = true;
};

struct ResultsState {
    QString primary;
    double  min = 0.0;
    double  max = 0.0;
};

struct Selection {
    std::vector<int> elements;
    std::vector<int> nodes;
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

    // Apply a full q_state response; emits every *Changed signal.
    void applyFullState(const QJsonObject &qStateResponse);

    // Apply a state_changed diff; emits only the changed signals and updates
    // lastSeq(). Nested objects under "fields" are replacements, not merges
    // (see planning/ui-design/04-client.md §4.2).
    void applyDiff(const QJsonObject &stateChangedEvent);

signals:
    void timeChanged(const griz::model::TimeState &);
    void viewChanged(const griz::model::ViewState &);
    void renderChanged(const griz::model::RenderState &);
    void materialsChanged();
    void resultsChanged(const griz::model::ResultsState &);
    void selectionChanged(const griz::model::Selection &);
    void stateOverflow();

private:
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
