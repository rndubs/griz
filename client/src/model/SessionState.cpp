#include "SessionState.h"

#include <QJsonArray>
#include <QJsonValue>

namespace griz::model {

SessionState::SessionState(QObject *parent) : QObject(parent) {}

void SessionState::applyFullState(const QJsonObject &qStateData) {
    if (qStateData.isEmpty()) {
        return;
    }
    applyDatabase  (qStateData.value(QStringLiteral("database" )).toObject());
    applyTime      (qStateData.value(QStringLiteral("time"     )).toObject());
    applyView      (qStateData.value(QStringLiteral("view"     )).toObject());
    applyRender    (qStateData.value(QStringLiteral("render"   )).toObject());
    applyMaterials (qStateData.value(QStringLiteral("materials")).toObject());
    applyResults   (qStateData.value(QStringLiteral("results"  )).toObject());
    applySelection (qStateData.value(QStringLiteral("selection")).toObject());

    emit databaseChanged(m_database);
    emit timeChanged(m_time);
    emit viewChanged(m_view);
    emit renderChanged(m_render);
    emit materialsChanged();
    emit resultsChanged(m_results);
    emit selectionChanged(m_selection);
}

void SessionState::applyDiff(const QJsonObject &stateChangedEvent) {
    const QJsonObject fields = stateChangedEvent
        .value(QStringLiteral("fields")).toObject();
    const quint64 seq = static_cast<quint64>(stateChangedEvent
        .value(QStringLiteral("state_seq")).toDouble(0));

    if (seq != 0) {
        if (m_lastSeq != 0 && seq > m_lastSeq + 1) {
            emit stateOverflow();
        }
        m_lastSeq = seq;
    }

    if (fields.contains(QStringLiteral("database"))) {
        applyDatabase(fields.value(QStringLiteral("database")).toObject());
        emit databaseChanged(m_database);
    }
    if (fields.contains(QStringLiteral("time"))) {
        applyTime(fields.value(QStringLiteral("time")).toObject());
        emit timeChanged(m_time);
    }
    if (fields.contains(QStringLiteral("view"))) {
        applyView(fields.value(QStringLiteral("view")).toObject());
        emit viewChanged(m_view);
    }
    if (fields.contains(QStringLiteral("render"))) {
        applyRender(fields.value(QStringLiteral("render")).toObject());
        emit renderChanged(m_render);
    }
    if (fields.contains(QStringLiteral("materials"))) {
        applyMaterials(fields.value(QStringLiteral("materials")).toObject());
        emit materialsChanged();
    }
    if (fields.contains(QStringLiteral("results"))) {
        applyResults(fields.value(QStringLiteral("results")).toObject());
        emit resultsChanged(m_results);
    }
    if (fields.contains(QStringLiteral("selection"))) {
        applySelection(fields.value(QStringLiteral("selection")).toObject());
        emit selectionChanged(m_selection);
    }
}

void SessionState::applyDatabase(const QJsonObject &obj) {
    if (obj.isEmpty()) return;
    m_database.path      = obj.value(QStringLiteral("path")).toString(m_database.path);
    m_database.title     = obj.value(QStringLiteral("title")).toString(m_database.title);
    m_database.numStates = obj.value(QStringLiteral("n_states")).toInt(m_database.numStates);
    m_database.numNodes  = obj.value(QStringLiteral("n_nodes")).toInt(m_database.numNodes);
    m_database.numElems  = obj.value(QStringLiteral("n_elements")).toInt(m_database.numElems);
}

void SessionState::applyTime(const QJsonObject &obj) {
    if (obj.isEmpty()) return;
    m_time.state     = obj.value(QStringLiteral("state")).toInt(m_time.state);
    m_time.stateMin  = obj.value(QStringLiteral("state_min")).toInt(m_time.stateMin);
    m_time.stateMax  = obj.value(QStringLiteral("state_max")).toInt(m_time.stateMax);
    m_time.time      = obj.value(QStringLiteral("time")).toDouble(m_time.time);
    m_time.timeMin   = obj.value(QStringLiteral("time_min")).toDouble(m_time.timeMin);
    m_time.timeMax   = obj.value(QStringLiteral("time_max")).toDouble(m_time.timeMax);
    m_time.animating = obj.value(QStringLiteral("animating")).toBool(m_time.animating);
}

static void readVec3(const QJsonObject &obj,
                     double *x, double *y, double *z) {
    *x = obj.value(QStringLiteral("x")).toDouble(*x);
    *y = obj.value(QStringLiteral("y")).toDouble(*y);
    *z = obj.value(QStringLiteral("z")).toDouble(*z);
}

void SessionState::applyView(const QJsonObject &obj) {
    if (obj.isEmpty()) return;
    if (obj.contains(QStringLiteral("rotate"))) {
        readVec3(obj.value(QStringLiteral("rotate")).toObject(),
                 &m_view.rotateX, &m_view.rotateY, &m_view.rotateZ);
    }
    if (obj.contains(QStringLiteral("translate"))) {
        readVec3(obj.value(QStringLiteral("translate")).toObject(),
                 &m_view.translateX, &m_view.translateY, &m_view.translateZ);
    }
    if (obj.contains(QStringLiteral("scale"))) {
        readVec3(obj.value(QStringLiteral("scale")).toObject(),
                 &m_view.scaleX, &m_view.scaleY, &m_view.scaleZ);
    }
    m_view.zoom = obj.value(QStringLiteral("zoom")).toDouble(m_view.zoom);
    if (obj.contains(QStringLiteral("viewport"))) {
        const QJsonObject vp = obj.value(QStringLiteral("viewport")).toObject();
        m_view.width  = vp.value(QStringLiteral("width")).toInt(m_view.width);
        m_view.height = vp.value(QStringLiteral("height")).toInt(m_view.height);
    }
}

void SessionState::applyRender(const QJsonObject &obj) {
    if (obj.isEmpty()) return;
    m_render.mode     = obj.value(QStringLiteral("mode")).toString(m_render.mode);
    m_render.colormap = obj.value(QStringLiteral("colormap")).toString(m_render.colormap);
    const QJsonObject toggles = obj.value(QStringLiteral("toggles")).toObject();
    m_render.showCoord  = toggles.value(QStringLiteral("coord" )).toBool(m_render.showCoord);
    m_render.showTime   = toggles.value(QStringLiteral("time"  )).toBool(m_render.showTime);
    m_render.showCmap   = toggles.value(QStringLiteral("cmap"  )).toBool(m_render.showCmap);
    m_render.showMinMax = toggles.value(QStringLiteral("minmax")).toBool(m_render.showMinMax);
}

void SessionState::applyMaterials(const QJsonObject &obj) {
    if (obj.isEmpty()) return;
    // build_q_materials wraps the array as {"total": N, "materials": [...]}.
    // state_changed events may instead deliver the array directly under
    // fields.materials — accept either shape.
    QJsonArray arr;
    if (obj.value(QStringLiteral("materials")).isArray()) {
        arr = obj.value(QStringLiteral("materials")).toArray();
    }
    m_materials.clear();
    m_materials.reserve(static_cast<size_t>(arr.size()));
    for (const QJsonValue &v : arr) {
        const QJsonObject m = v.toObject();
        Material mat;
        mat.id      = m.value(QStringLiteral("id")).toInt(0);
        mat.label   = m.value(QStringLiteral("label")).toString();
        mat.visible = m.value(QStringLiteral("visible")).toBool(true);
        mat.enabled = m.value(QStringLiteral("enabled")).toBool(true);
        const QJsonValue color = m.value(QStringLiteral("color"));
        if (color.isArray()) {
            const QJsonArray c = color.toArray();
            if (c.size() >= 3) {
                mat.colorR = c.at(0).toDouble(0.0);
                mat.colorG = c.at(1).toDouble(0.0);
                mat.colorB = c.at(2).toDouble(0.0);
                mat.hasColor = true;
            }
        }
        m_materials.push_back(mat);
    }
}

void SessionState::applyResults(const QJsonObject &obj) {
    if (obj.isEmpty()) return;

    // Catalog: q_results emits `results` as a flat array of
    // {name, title, origin}. The `results.available` schema from
    // planning/shared/query-commands.md §results is not live yet — the
    // server splits primary/component server-side in a later phase.
    const QJsonArray catalog = obj.value(QStringLiteral("results")).toArray();
    m_results.available.clear();
    m_results.available.reserve(catalog.size());
    for (const QJsonValue &v : catalog) {
        const QJsonObject entry = v.toObject();
        ResultItem item;
        item.name   = entry.value(QStringLiteral("name")).toString();
        item.title  = entry.value(QStringLiteral("title")).toString();
        item.origin = entry.value(QStringLiteral("origin")).toString();
        if (!item.name.isEmpty()) {
            m_results.available.push_back(item);
        }
    }

    const QJsonObject current = obj.value(QStringLiteral("current")).toObject();
    m_results.currentName = current.value(QStringLiteral("name")).toString();

    const QJsonObject active = obj.value(QStringLiteral("active")).toObject();
    if (!active.isEmpty()) {
        m_results.primary   = active.value(QStringLiteral("field")).toString(m_results.primary);
        m_results.component = active.value(QStringLiteral("component")).toString(m_results.component);
        m_results.grizName  = active.value(QStringLiteral("griz_name")).toString(m_results.grizName);
        m_results.min       = active.value(QStringLiteral("min")).toDouble(m_results.min);
        m_results.max       = active.value(QStringLiteral("max")).toDouble(m_results.max);
        m_results.hasActive = true;
    } else {
        m_results.hasActive = !m_results.currentName.isEmpty();
        if (m_results.hasActive) {
            m_results.primary   = m_results.currentName;
            m_results.component = current.value(QStringLiteral("title")).toString();
            m_results.grizName  = m_results.currentName;
        }
    }
}

void SessionState::applySelection(const QJsonObject &obj) {
    if (obj.isEmpty()) return;
    m_selection.elements.clear();
    m_selection.nodes.clear();
    const QJsonArray picked = obj.value(QStringLiteral("picked")).toArray();
    for (const QJsonValue &v : picked) {
        const QJsonObject entry = v.toObject();
        PickedItem item;
        item.kind = entry.value(QStringLiteral("kind")).toString();
        item.id   = entry.value(QStringLiteral("id")).toInt(0);
        if (item.kind == QLatin1String("node")) {
            m_selection.nodes.push_back(item);
        } else if (!item.kind.isEmpty()) {
            // Everything that isn't a node class counts as an element for
            // the dock's elements bucket — the server's `kind` is already
            // the class short name (brick/hex/quad/…) we can hand back
            // to `hilite <kind> <id>`.
            m_selection.elements.push_back(item);
        }
    }
    m_selection.hasHighlighted =
        !obj.value(QStringLiteral("highlighted")).isNull();
}

} // namespace griz::model
