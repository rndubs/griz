# Shared — Query Commands and State Schema

## Scope

Defines the set of read-only `q_*` commands that expose structured viewer state, and the canonical state-dict schema they return. Both front ends use these:

- The MCP Python package calls them on demand (`Griz.state()`, `field.list()`, `view` lookups, etc.).
- The Qt UI client pulls them for initial snapshot and on event-gap recovery, and receives the same schema pushed as `state_changed` events between pulls.

One schema, two delivery modes.

## Current state (2026-04)

| Command | Status | Implementation | Gap vs. schema below |
|---------|--------|----------------|----------------------|
| `q_state` | **shipped** | `build_q_state_data()` at `Src/viewer.c:2993–3017` | Returns `time` + `viewport` + `current_field` + `result_title` only — far from the full schema. No `database`, no `render`, no `selection`, no `materials` inline, partial `results`. |
| `q_time` | **shipped** | `build_q_time_data()` `Src/viewer.c:2955–2977` | Matches schema: `state`, `state_max`/`state_count`, `time`/`max_time`. |
| `q_view` | **shipped (subset)** | `build_q_view_data()` `Src/viewer.c:2980–2990` | Returns `viewport: {width, height}` only. Missing `rotate`, `translate`, `scale`, `zoom`. |
| `q_materials` | **shipped (subset)** | `build_q_materials_data()` `Src/viewer.c:3020–3045` | Returns `{id, visible, enabled}` per material. Missing `label`, `color`, and per-material `n_elements`. |
| `q_results` | **shipped** | `build_q_results_data()` `Src/viewer.c:3048–3076`, iterating primal + derived hash tables via `server_build_results_from_htable()` (`Src/results.c`). | Returns terse Griz names (e.g. `sx`, `seff`) rather than human-readable `(field, component)` pairs. The results-map translation happens Python-side (`pygriz/src/griz/results_map.py`) today. |
| `q_selection` | **not implemented** | — | — |
| `q_render` | **not implemented** | — | — |
| `q_database` | **not implemented** | — | — |

The dispatcher that routes these bypasses `parse_command()` and emits a `response` with a populated `data` field directly: `server_try_query()` at `Src/viewer.c:3082–3116`.

The `state_changed` event stream described in [§ State-changed event shape](#state-changed-event-shape) is **not yet implemented** — there is no `notify_state()`, no `state_seq` counter, and no emission site. Today's clients rely on pull (re-querying `q_*`) after any command that might mutate state.

### Design drift to revisit

The current `q_state` payload shape is close to, but not identical to, the schema in this doc. Example from a live `q_state` response:

```json
{"type":"response","id":"req_1","status":"ok","stdout":"","stderr":"","data":{
  "time": {"time_state":1,"max_time_state":71,"state_count":71,
           "time_value":0.0,"max_time_value":0.001},
  "viewport": {"width":1024,"height":1024},
  "current_field": null,
  "result_title": ""
}}
```

Key drifts from the target schema:

- `time_state` vs. `state` (current vs. schema).
- `max_time_state` vs. `state_max`. (`state_count` also present.)
- `time_value` / `max_time_value` vs. `time` / (implicit). No `state_min` reported.
- `viewport` is at the top level rather than nested under `view`.
- `current_field` / `result_title` vs. `results: {active: {...}}`.

Before the Qt client consumes these, the C side should either adopt the schema as documented or the documented schema should migrate to match the code. Recommended direction: **match the documented schema**, since it was designed to round-trip with the `state_changed` diff event and to nest cleanly under sub-object queries (`q_time`, `q_view`). Follow-on ticket.

## Related

- [`server-binary.md`](server-binary.md)
- [`command-protocol.md`](command-protocol.md) — defines the `data` field these commands populate and the `state_changed` event that carries this same schema.
- [`../MCP.md`](../MCP.md) §4.5 (original proposal for `q_*`; folded in here).
- [`../ui-design/01-architecture.md`](../ui-design/01-architecture.md) §4 "State events".

## Commands

All `q_*` commands are read-only, thread-safe against the command thread's single-writer discipline, and routed through the normal `parse_command()` dispatcher (UI invariant I1). Each populates the response's `data` object with a subset of the state schema defined below.

| Command | Returns | Notes |
|---------|---------|-------|
| `q_state` | Full state dict (all keys). | Expensive-ish; prefer `q_view`/`q_time` for hot paths. Used on reconnect to reseed the client. |
| `q_time` | `time` sub-object only. | State index, time value, min/max state, animating flag. |
| `q_view` | `view` sub-object only. | Camera rotation, translation, scale, zoom, near/far. |
| `q_materials` | `materials` array. | Per-material id, visibility, enable flags, color. |
| `q_results` | `results` sub-object: available fields and current selection. | Available list is a function of the loaded DB, cached. |
| `q_selection` | `selection` sub-object. | Picked/highlighted object set, per-object metadata. |
| `q_render` | `render` sub-object. | Current render mode, toggles (coord axes, time label, colormap, minmax). |
| `q_database` | `database` sub-object. | Path, format version, mesh counts, element type breakdown. |

Every command returns `status:"ok"` with the relevant partial state on success, or a typed error (e.g. `no_database` if a DB is required but none is open).

## State schema

The canonical state dict. A `q_state` response carries the whole object. A partial `q_*` response carries only the named sub-object. A `state_changed` event carries only the keys that changed, with absolute (not delta) values.

```json
{
  "schema_version": 1,
  "session_id": "griz-ab12cd34",

  "database": {
    "path": "/projects/blast/runs/blast.plt",
    "open": true,
    "format_version": "Mili-1.3",
    "n_nodes": 128934,
    "n_elements": 742310,
    "n_states": 201
  },

  "time": {
    "state": 42,
    "state_min": 1,
    "state_max": 201,
    "time": 0.00420,
    "animating": false
  },

  "view": {
    "rotate":    {"x": 30.0, "y": 0.0, "z": 0.0},
    "translate": {"x": 0.0,  "y": 0.0, "z": 0.0},
    "scale":     {"x": 1.0,  "y": 1.0, "z": 1.0},
    "zoom":      1.0,
    "viewport":  {"width": 1024, "height": 1024}
  },

  "render": {
    "mode": "solid",
    "toggles": {
      "coord": true,
      "time":  true,
      "cmap":  true,
      "minmax": true
    },
    "colormap": "cool-warm"
  },

  "materials": [
    {"id": 1, "label": "steel",   "visible": true,  "enabled": true,  "color": [0.8, 0.8, 0.85]},
    {"id": 2, "label": "concrete","visible": true,  "enabled": true,  "color": [0.6, 0.6, 0.6]},
    {"id": 3, "label": "air",     "visible": false, "enabled": false, "color": [0.9, 0.9, 1.0]}
  ],

  "results": {
    "available": {
      "stress":       ["xx", "yy", "zz", "xy", "yz", "zx", "von_mises", "pressure"],
      "strain":       ["xx", "yy", "zz", "xy", "yz", "zx"],
      "temperature":  [null],
      "displacement": ["x", "y", "z", "magnitude"]
    },
    "active": {
      "field":     "stress",
      "component": "von_mises",
      "griz_name": "seff",
      "min":       1.2e5,
      "max":       8.7e8
    }
  },

  "selection": {
    "picked": [
      {"kind": "node",    "id": 12345, "metadata": {"coords": [0.1, 0.0, 0.2]}},
      {"kind": "element", "id": 99821, "metadata": {"material": 1}}
    ],
    "highlighted": null
  }
}
```

Conventions:

- `schema_version` is bumped when the shape changes; clients tolerate unknown keys, reject unknown required keys.
- Field names inside `results.available` are the human-readable names from [`results-map.md`](results-map.md); component names are the human-readable components. `results.active.griz_name` exposes the terse Griz command name for clients that want it.
- `null` component in `results.available` (e.g. for `temperature`) indicates the field is scalar — use `component=None` in the Python API, omit the component in the MCP tool.
- Coordinate arrays are `[x, y, z]` with the same frame convention Griz uses internally.

## State-changed event shape

From [`command-protocol.md`](command-protocol.md) § 3, repeated here to show how it reuses this schema:

```json
{
  "type": "event",
  "event": "state_changed",
  "state_seq": 18,
  "fields": {
    "view": { "rotate": {"x": 45.0, "y": 0.0, "z": 0.0} },
    "time": { "state": 43, "time": 0.00430 }
  }
}
```

A diff may include any subset of the top-level keys; nested objects are always delivered whole (i.e., the `view.rotate` above replaces the entire rotation sub-object, not just `x`). This keeps client-side merge trivial and unambiguous.

## Implementation notes

- Implement the `q_*` commands in a new translation unit `Src/server_query.c` that is linked only into `griz-server`. Each command reads from the same `Analysis *analy` globals that `interpret.c` mutates, so no new accessor functions are needed — just JSON serialization.
- Serialization goes through a small helper that writes into the per-command buffer described in [`output-capture.md`](output-capture.md), tagged so the dispatcher routes it to `response.data` rather than `response.stdout`. Suggested helper: `griz_data_object(const char *json)` — the caller builds the JSON body, the helper wraps it.
- `state_changed` emission is centralized: `interpret.c` command handlers that mutate state call a new `notify_state(key, value)` helper after their mutation succeeds. The server I/O thread coalesces consecutive notifications of the same key into a single event.

## Conflict resolution

`MCP.md` §4.5 defined these commands for MCP only. `UI.md` / `ui-design/01-architecture.md` defined state events with overlapping but independently-shaped data. The conflict is resolved by:

- Moving the `q_*` catalog into this shared doc so both front ends consume the same commands.
- Adopting a single state schema, reused by `q_*` responses and by `state_changed` events. The original UI.md text describing events as "state changes" is compatible; only the payload shape needed to be named and pinned.
- The UI needs push (events) and the MCP needs pull (commands). Both are supported; clients that don't need one can ignore it.

## Open questions

- **Materials: include element counts?** Useful in UI material manager; adds a linear scan at first `q_state`. Current plan: include as `n_elements` per material, compute lazily and cache.
- **Results: stat caching.** `results.active.min` / `max` can be expensive to recompute on state change. Cache per (field, component, state index); invalidate on DB reload.
- **Selection payload size.** A box-select can pick tens of thousands of elements. Current plan: truncate `picked` to the first 1024 in state snapshots, with a `truncated: true` flag and a `selection_count` total. Clients that want the full set use a dedicated `q_selection_all` paged command (phase 2).
- **Per-mesh vs. global.** Griz supports multiple meshes. State schema currently assumes one active mesh. Multi-mesh extension is a phase-2 schema bump.
