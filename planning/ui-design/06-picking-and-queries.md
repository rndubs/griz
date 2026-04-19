# 06 — Picking and queries

## Scope

Interactive selection (point, box, ray) and on-demand metadata queries about the loaded mesh. Both run server-side because the client does not have the mesh (invariant **I4**). This doc covers the pick RPC, the ID-buffer mechanism, selection state ownership, and the metadata-query catalog.

Out of scope: the selection-dock UI (see [04-client](04-client.md) §7), wire framing (see [02-protocol](02-protocol.md)), rendering of highlight overlays (handled through the regular frame stream, see [05-rendering-and-streaming](05-rendering-and-streaming.md)).

## Related

- `../UI.md` §5 (Large-mesh priority and selection)
- [`../shared/query-commands.md`](../shared/query-commands.md) — `q_selection` is specified there but not yet implemented.
- [03-server](03-server.md) §4 (server query set), §5 (state events)
- [02-protocol](02-protocol.md) §3 (binary frames, used for diagnostic pick-buffer dumps)

## 1. Current state (2026-04)

**Nothing interactive on the server side yet.** But Griz itself has a long-standing picking and hiliting vocabulary in the command language that already works headlessly:

- `hilite nod|brick|shell|...  <id>` — hilite a specific object by id and re-render.
- `clrhil` — clear hilite.
- `vcent nod <id>` — center view on a node.
- Plus full element/material selection via interpret.c at a coarser granularity.

These go through `parse_command()` (`Src/interpret.c`) exactly like any other command, so the server already supports them: an MCP client or the future Qt client can invoke them today. What's **missing**:

- A **point-pick RPC** that takes a viewport `(x, y)` and returns the hit id. No current Griz command accepts pixel coordinates; it needs per-object ids.
- An **ID-buffer render pass** to support the point-pick resolution.
- `q_selection` query and `selection` section of the state schema.
- Rich metadata queries (`query_node(id)`, `query_element(id)`).

## 2. Pick primitives

Three user-visible primitives in v1; one more deferred.

### 2.1 Point pick

Client sends:

```json
{ "type": "request", "id": "pick-42", "cmd": "pick",
  "data": { "x": 512, "y": 300,
            "mode": "node" | "element" | "material" | "any",
            "modifiers": ["shift"] } }
```

(Note: `data` on a request is a new field, additive to the envelope in [`../shared/command-protocol.md`](../shared/command-protocol.md). It lets structured pick args ride without needing to pack them into a `cmd` string. Alternative: keep the envelope unchanged and use a dedicated command name `pick_at <x> <y> <mode>` that parses in `interpret.c`. **Recommend the command-name path** — it keeps invariant **I1** clean, keeps request envelope unchanged, and gives the MCP bridge a ready-to-use command.)

Revised:

```
pick_at <x> <y> <mode>
pick_at 512 300 element
```

Server returns:

```json
{ "type": "response", "id": "pick-42", "status": "ok", "data": {
    "kind": "element",
    "id":   99821,
    "coords_world": [0.1, 0.0, 0.2],
    "material": 1,
    "result_value": 2.4e8          // current active result at this object
  }
}
```

Miss: `"data": null` with `status="ok"` (picking nothing is not an error).

### 2.2 Box select

```
box_select <x0> <y0> <x1> <y1> <mode> [add|replace|subtract]
box_select 100 100 400 400 element replace
```

Server returns the full set of hit ids, truncated per the selection-payload-size policy in [`../shared/query-commands.md`](../shared/query-commands.md) (1024 items inline, `truncated: true`, use paged `q_selection_all` for the rest).

### 2.3 Ray pick

```
pick_ray <ox> <oy> <oz> <dx> <dy> <dz> <mode>
```

Origin + direction in world space. Used for picking through clip planes or transparent materials. Lower priority; ship in v1 if cheap.

### 2.4 Lasso

Deferred. The current Griz UI doesn't have a lasso, so dropping it in v1 is a no-regression.

## 3. ID-buffer implementation

The mechanism the server uses to resolve `pick_at x y mode` to an id without the client needing geometry. Analogous to what every modern mesh viewer does, and reasonably cheap at small render sizes.

### 3.1 ID-buffer render pass

A second render pass, one per pick batch (not per frame):

1. Rebind OSMesa context to the ID buffer (separate RGBA8 buffer the same size as the screen buffer).
2. Re-issue the current camera + clip state.
3. Render every mesh primitive with its id packed into color bits:
   - 24-bit id space: `R<<16 | G<<8 | B`. Sufficient for ~16M ids.
   - 8-bit alpha: object kind (node / element / material / surface).
4. Read pixel under cursor. `(r, g, b, a)` → `(id, kind)`.

Cost per pick batch: one full render pass. On large meshes this can be 10s to 100s of ms — acceptable for a click (users don't click at 60 FPS), costly for hover probes (see §4 interactive probes).

### 3.2 Caching

Hold the ID buffer from the most recent **display frame** in a per-session cache. On a pick_at whose (x,y) lies within the current buffer's viewport and whose state hasn't changed since, answer from the cache — zero cost.

Invalidation: any state change that affects geometry (camera, clip, visibility, time step). Instrumented alongside `notify_state` in [03-server](03-server.md) §5.

### 3.3 Transparency and overlap

- Transparent materials: omit from the ID buffer or render opaque-only. Document as a pick limitation in v1.
- Overlap on edge: ID buffer returns whichever primitive wrote last. Acceptable; users re-click if it's the wrong one.

### 3.4 Ray path

For `pick_ray`: no ID buffer. Traverse the mesh against the ray; reuse any existing spatial structures in `Src/` (a quick audit during implementation — Mili or Griz may already have a BVH for other uses). If nothing exists: brute-force iterate meshes on v1, build a BVH lazily in phase 2.

## 4. Interactive probes (hover)

A hover-over scalar probe is a nice UX feature but risks flooding the server. Design:

- Client hovers → sends `pick_at` with `mode=any` + `hover=true` flag, throttled to ≤5 Hz.
- Server answers from the cached ID buffer only. If the cache is invalid, return `miss` (do **not** re-render — that would be a hover-induced 100 ms stall).
- Client paints the probe label inside its own viewport overlay (not server-rendered). Since this is text on top of the streamed frame, it does not violate **I4** — the client is painting UI chrome, not mesh.

## 5. Selection state

### 5.1 Ownership

Server owns, client mirrors. Mirrors the general **I2** from [01-architecture](01-architecture.md) §8.

### 5.2 Schema

From [`../shared/query-commands.md`](../shared/query-commands.md) § State schema:

```json
"selection": {
  "picked": [
    {"kind": "node",    "id": 12345, "metadata": {"coords": [...]}},
    {"kind": "element", "id": 99821, "metadata": {"material": 1}}
  ],
  "highlighted": null
}
```

`picked` is the multi-select set used by most operations. `highlighted` is the current singleton (last click, hover target). Separating them lets the UI distinguish "focus" from "multi-select".

### 5.3 Lifecycle

- `pick_at` / `box_select` / `pick_ray` mutate `picked` according to the modifier arg (add/replace/subtract). Emit `state_changed` with the new `selection`.
- `hilite` command mutates `highlighted` similarly and re-renders.
- Re-render applies the Griz standard "hilite color" to selected primitives. Already implemented in `Src/draw.c` via existing hilite paths.
- `clrhil` / `clear_selection` empty both. Emit `state_changed`.

### 5.4 Truncation

Payload cap: 1024 inline entries, `selection_count` + `truncated: true` when exceeded. Paged access via `q_selection_all?offset=N&limit=M` (phase 2).

## 6. Metadata queries

Independent of picking — any time the UI wants detail for an id without having to go through selection state.

### 6.1 Query catalog

| Command | Returns |
|---------|---------|
| `q_node <id>` | `{id, coords, displacement, result_value, attached_elements: [...]}` |
| `q_element <id>` | `{id, type (hex/brick/shell/…), material, connectivity: [nodes], result_value_per_int_pt: [...]}` |
| `q_material <id>` | `{id, label, color, visible, enabled, n_elements}` |
| `q_probe <kind> <id>` | Compact subset used by hover labels; cheap. |

All read-only, go through `server_try_query()` in [03-server](03-server.md) §2.3, bypass `parse_command()`. Their responses land in `data`.

### 6.2 `q_element` detail

Element-level queries are the heavy ones. A `q_element 99821` on a large mesh should not trigger a full array scan — rely on the existing Griz indexing (`Mesh_data->element_tables`, `analy->mesh_qty`). Implementation stays close to whatever `hilite brick <id>` already uses for its lookup.

### 6.3 Batch

For common UI patterns (selecting 200 elements and populating a table), a batch form is useful:

```
q_elements 99821 99822 99823 99824 ...
```

Returns an array. Bounded at 512 per request; beyond that the client pages. Avoids N envelopes for a list selection.

## 7. Multi-select operations

Server-side derivations over an existing selection:

| Command | Effect |
|---------|--------|
| `select_by_material <mat>` | Set `picked` to all elements of material `mat` |
| `select_connected` | Grow `picked` by adjacency once |
| `select_boundary` | Set `picked` to boundary elements of current set |
| `invert_selection` | `all_elements \ picked` |
| `clear_selection` | Empty `picked` |

Each mutates server-side selection state and emits `state_changed`. The UI dock exposes these as buttons / context-menu items (see [04-client](04-client.md) §7).

## 8. Performance budget

- **Point pick on cached ID buffer:** <1 ms. Just a pixel read.
- **Point pick requiring ID-buffer re-render:** bounded by the render time of the mesh at current viewport. Target <300 ms for interactive feel on large meshes.
- **Box select:** same as point pick for ID buffer cost; O(pixels in box) for the set extraction. Still ≤300 ms target.
- **Hover probe:** must not re-render. Either cache hit (<1 ms) or "miss" (also <1 ms).
- **q_element / q_node:** O(1) via existing indexing; <10 ms target.

A pick RPC that exceeds 1 s is a bug.

## 9. Relationship to existing Griz commands

Where possible, new pick/query commands reuse existing interpret.c handlers under the hood:

- `pick_at x y node` → resolve via ID buffer → call the same code path as `hilite nod <id>`.
- `pick_at x y element mode=material` → resolve → `hilite mat <id>` path.
- `box_select` → enumerate via ID buffer → call `hilite` on each.

This preserves invariant **I1** and means the highlight re-render uses the already-well-tested hilite code in `Src/draw.c`.

## 10. Implementation order

Under [03-server](03-server.md) §9 steps 8–9:

1. `pick_at x y mode` command that uses the *currently-rendered* display buffer as the ID buffer source. Acceptable approximation for v0 (no second pass yet).
2. Proper ID-buffer second pass. Add an `ID_BUFFER` OSMesa context or reuse the primary one and re-render with an "id-encode" flag through `draw.c`.
3. Selection state + `q_selection`.
4. `pick_ray`.
5. Metadata queries `q_node`, `q_element`, `q_material`.
6. Multi-select derivations.
7. Hover probe throttling + cache.

## Open questions

- **Where does ID encoding live in `draw.c`?** There's no clean hook today; needs a `draw_mode=DRAW_IDS` global flag + per-primitive colour override inside the draw loop. Estimate: a hundred lines of surgical changes in `Src/draw.c` (<1% of 18k).
- **Transparent materials in ID buffer.** Skip or render first as opaque? A later decision depending on common user datasets.
- **Selection versioning for undo.** Client-side undo is nice-to-have; not v1.
- **Multi-mesh picking.** Griz supports multiple meshes per DB; current selection schema is single-mesh. Phase 2 when multi-mesh schema lands (same open question in [`../shared/query-commands.md`](../shared/query-commands.md)).
- **Spatial index reuse.** Audit `Src/` for any BVH / octree already used for other queries before implementing a new one for rays.
