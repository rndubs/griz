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

**MVP scope:** point pick (§2.1) only. Box select (§2.2), ray pick (§2.3), and lasso (§2.4) are *post-MVP* (§11). Rationale: point pick alone proves the full end-to-end loop — client `(x,y)` → server ID-buffer render → hit id → `hilite` re-render → streamed frame — and covers the dominant "click an element to see its values" workflow. Everything else is additive on top of the same plumbing.

### 2.1 Point pick *(MVP)*

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

### 2.2 Box select *(post-MVP)*

```
box_select <x0> <y0> <x1> <y1> <mode> [add|replace|subtract]
box_select 100 100 400 400 element replace
```

Server returns the full set of hit ids, truncated per the selection-payload-size policy in [`../shared/query-commands.md`](../shared/query-commands.md) (1024 items inline, `truncated: true`, use paged `q_selection_all` for the rest).

### 2.3 Ray pick *(post-MVP)*

```
pick_ray <ox> <oy> <oz> <dx> <dy> <dz> <mode>
```

Origin + direction in world space. Used for picking through clip planes or transparent materials.

### 2.4 Lasso *(deferred)*

Deferred. The current Griz UI doesn't have a lasso, so dropping it is a no-regression.

## 3. ID-buffer implementation

The mechanism the server uses to resolve `pick_at x y mode` to an id without the client needing geometry. Analogous to what every modern mesh viewer does, and reasonably cheap at small render sizes.

**MVP scope:** §3.1 only — one ID-buffer render pass per pick, re-rendered on demand, transparent materials skipped (§3.3). Caching (§3.2) and ray-pick spatial structures (§3.4) are *post-MVP* (§11). Rationale: a pick click is a user-initiated action that tolerates hundreds of ms; optimizing it before measuring real click-to-highlight latency on a representative mesh is premature.

### 3.1 ID-buffer render pass *(MVP)*

A second render pass, one per pick batch (not per frame):

1. Rebind OSMesa context to the ID buffer (separate RGBA8 buffer the same size as the screen buffer).
2. Re-issue the current camera + clip state.
3. Render every mesh primitive with its id packed into color bits:
   - 24-bit id space: `R<<16 | G<<8 | B`. Sufficient for ~16M ids.
   - 8-bit alpha: object kind (node / element / material / surface).
4. Read pixel under cursor. `(r, g, b, a)` → `(id, kind)`.

Cost per pick batch: one full render pass. On large meshes this can be 10s to 100s of ms — acceptable for a click (users don't click at 60 FPS).

### 3.2 Caching *(post-MVP)*

Hold the ID buffer from the most recent **display frame** in a per-session cache. On a pick_at whose (x,y) lies within the current buffer's viewport and whose state hasn't changed since, answer from the cache — zero cost.

Invalidation: any state change that affects geometry (camera, clip, visibility, time step). Instrumented alongside `notify_state` in [03-server](03-server.md) §5.

### 3.3 Transparency and overlap

- **Transparent materials (MVP):** skip — the ID buffer renders opaque primitives only. Document as a known limitation: transparent objects can't be picked in MVP. Revisit only if user feedback shows transparent-material picking is a common workflow; the fix then is a second ID pass that renders transparent primitives with depth write enabled, or switching to ray picking for that case.
- **Overlap on edge:** ID buffer returns whichever primitive wrote last. Acceptable; users re-click if it's the wrong one.

### 3.4 Ray path *(post-MVP)*

Bundled with `pick_ray` in §2.3. No ID buffer — traverse the mesh against the ray directly. See §11 for the spatial-index plan.

## 4. Interactive probes (hover) *(post-MVP)*

A hover-over scalar probe is a nice UX feature but risks flooding the server, and it depends on the ID-buffer cache (§3.2) to avoid re-rendering on every mouse move. Both pieces are post-MVP. Full design preserved for when this lands:

- Client hovers → sends `pick_at` with `mode=any` + `hover=true` flag, throttled to ≤5 Hz.
- Server answers from the cached ID buffer only. If the cache is invalid, return `miss` (do **not** re-render — that would be a hover-induced 100 ms stall).
- Client paints the probe label inside its own viewport overlay (not server-rendered). Since this is text on top of the streamed frame, it does not violate **I4** — the client is painting UI chrome, not mesh.

## 5. Selection state

**MVP scope:** §5.1–§5.3 — server-owned selection with `picked` + `highlighted`, lifecycle driven by `pick_at` / `hilite` / `clrhil`. Truncation paging (§5.4) is *post-MVP* (§11) because MVP limits in-flight selections to whatever a single point pick produces (at most one id).

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

### 5.4 Truncation *(post-MVP)*

Payload cap: 1024 inline entries, `selection_count` + `truncated: true` when exceeded. Paged access via `q_selection_all?offset=N&limit=M`. Lands alongside box select (§2.2) — single-id point picks don't need it.

## 6. Metadata queries

Independent of picking — any time the UI wants detail for an id without having to go through selection state.

**MVP scope:** `q_node <id>` and `q_element <id>` only (§6.1) — enough to populate a "what did I just click" panel. `q_material`, `q_probe`, and the batch form `q_elements` are *post-MVP* (§11). All three unblock richer UX (material browser, hover labels, multi-row tables) that doesn't exist in MVP.

### 6.1 Query catalog

| Command | Returns | MVP? |
|---------|---------|------|
| `q_node <id>` | `{id, coords, displacement, result_value, attached_elements: [...]}` | **yes** |
| `q_element <id>` | `{id, type (hex/brick/shell/…), material, connectivity: [nodes], result_value_per_int_pt: [...]}` | **yes** |
| `q_material <id>` | `{id, label, color, visible, enabled, n_elements}` | post-MVP |
| `q_probe <kind> <id>` | Compact subset used by hover labels; cheap. | post-MVP (depends on §4) |

All read-only, go through `server_try_query()` in [03-server](03-server.md) §2.3, bypass `parse_command()`. Their responses land in `data`.

### 6.2 `q_element` detail

Element-level queries are the heavy ones. A `q_element 99821` on a large mesh should not trigger a full array scan — rely on the existing Griz indexing (`Mesh_data->element_tables`, `analy->mesh_qty`). Implementation stays close to whatever `hilite brick <id>` already uses for its lookup.

### 6.3 Batch *(post-MVP)*

For common UI patterns (selecting 200 elements and populating a table), a batch form `q_elements 99821 99822 ...` is useful. Bounded at 512 per request; beyond that the client pages. Lands with box select (§2.2) — MVP's single-id point pick doesn't need it.

## 7. Multi-select operations *(post-MVP)*

All of §7 is post-MVP. These are server-side derivations over an existing multi-id selection; with MVP limited to single-id point pick, there's no "existing selection" to derive over. Full catalog preserved below for when box select (§2.2) and truncation paging (§5.4) land:

| Command | Effect |
|---------|--------|
| `select_by_material <mat>` | Set `picked` to all elements of material `mat` |
| `select_connected` | Grow `picked` by adjacency once |
| `select_boundary` | Set `picked` to boundary elements of current set |
| `invert_selection` | `all_elements \ picked` |
| `clear_selection` | Empty `picked` |

Each mutates server-side selection state and emits `state_changed`. The UI dock exposes these as buttons / context-menu items (see [04-client](04-client.md) §7).

## 8. Performance budget

**MVP targets** (bold):

- **Point pick requiring ID-buffer re-render:** bounded by the render time of the mesh at current viewport. Target **<300 ms** for interactive feel on large meshes.
- **q_element / q_node:** O(1) via existing indexing; **<10 ms** target.
- **Pick RPC ceiling:** exceeding 1 s is a bug.

Post-MVP targets (tracked with their features in §11):

- Point pick on cached ID buffer: <1 ms (just a pixel read).
- Box select: same as point pick for ID buffer cost; O(pixels in box) for the set extraction. ≤300 ms target.
- Hover probe: must not re-render. Either cache hit (<1 ms) or "miss" (also <1 ms).

## 9. Relationship to existing Griz commands

Where possible, new pick/query commands reuse existing interpret.c handlers under the hood:

- `pick_at x y node` → resolve via ID buffer → call the same code path as `hilite nod <id>`.
- `pick_at x y element mode=material` → resolve → `hilite mat <id>` path.
- `box_select` → enumerate via ID buffer → call `hilite` on each.

This preserves invariant **I1** and means the highlight re-render uses the already-well-tested hilite code in `Src/draw.c`.

## 10. Implementation order

Under [03-server](03-server.md) §9 steps 8–9.

**MVP path (proof-of-concept):**

1. **ID-buffer second pass.** Add a `draw_mode=DRAW_IDS` global flag in `Src/draw.c` + per-primitive colour override inside the draw loop — estimated ~100 lines of surgical changes (<1% of 18k). Either bind a separate OSMesa `ID_BUFFER` context or reuse the primary one and re-render with the flag set. Transparent materials are skipped in this pass (§3.3).
2. **`pick_at x y mode` command.** Trigger the pass, read the pixel under `(x, y)`, decode `(id, kind)`, and dispatch through the existing `hilite <kind> <id>` code path in `interpret.c`. Return `{kind, id, material, coords_world, result_value}`.
3. **Selection state + `q_selection`.** `picked` (multi-select, MVP max size 1) and `highlighted` (singleton). Emit `state_changed` on mutation. Populate the `selection` block per the [`../shared/query-commands.md`](../shared/query-commands.md) schema.
4. **Metadata queries `q_node`, `q_element`.** Read-only, route through `server_try_query()`, reuse the existing Griz indexing.

That's the MVP. Everything below is post-MVP (§11).

**Post-MVP order (for reference):**

5. `q_material` query.
6. Box select + truncation paging + `q_elements` batch.
7. `pick_ray` + spatial-index audit / BVH.
8. Multi-select derivations (§7).
9. ID-buffer caching (§3.2).
10. Hover probe throttling + cache (§4).

## 11. Post-MVP / out of scope

MVP target: one click on a mesh element produces a highlighted re-render and a populated "what did I click" panel showing `q_element` / `q_node` data. Items below are deliberately deferred so MVP scope stays focused on that single round-trip; each links back to its originating section.

**Polish (deferred, will land in a later phase):**

- **Box select** (§2.2) and the selection-payload truncation + paging it depends on (§5.4). Ships with `q_elements` batch (§6.3).
- **Ray pick `pick_ray`** (§2.3, §3.4). Only matters for picking through clip planes or transparent materials. Before implementing: audit `Src/` for any existing BVH / octree (Mili or Griz may already have one for other queries) — reuse beats writing a new one. If nothing exists, brute-force iterate at first; build a lazy BVH only if click-to-hit latency exceeds the §8 target.
- **ID-buffer caching** (§3.2). Zero-cost repeat picks on unchanged state. Invalidation hooks plug into `notify_state` in [03-server](03-server.md) §5.
- **Hover probes** (§4). Depends on ID-buffer cache above; isolated client-side UI chrome on top.
- **Multi-select derivations** (§7): `select_by_material`, `select_connected`, `select_boundary`, `invert_selection`, `clear_selection`. All block on having multi-id selections in the first place (box select).
- **`q_material` and `q_probe` queries, `q_elements` batch** (§6.1, §6.3). `q_material` unblocks a material browser UI; `q_probe` is the cheap subset for hover labels; `q_elements` batches large selections.
- **Transparent-material picking.** MVP skips transparent materials in the ID buffer (§3.3). If user feedback shows this matters, add a second ID pass with depth write enabled, or fall back to ray picking for transparent hits.
- **Selection versioning / client-side undo.** Nice-to-have UX; no current Griz workflow demands it.

**Phase 2+ (dependent on other work landing first):**

- **Multi-mesh picking.** Griz supports multiple meshes per DB; current selection schema is single-mesh. Revisit when the multi-mesh schema lands (tracked as an open question in [`../shared/query-commands.md`](../shared/query-commands.md)).
- **Lasso select** (§2.4). Not present in the current Motif UI, so deferring it is a no-regression. Only worth adding if user demand appears.

## Open questions

*(All picking/queries open questions resolved as of 2026-04-19; resolutions are: ID encoding lives in `Src/draw.c` as a `draw_mode=DRAW_IDS` global flag + per-primitive colour override, ~100 lines of surgical changes (§10 step 1); transparent materials are skipped in the MVP ID buffer and revisited only if user feedback demands it (§3.3, §11); selection versioning / client-side undo is post-MVP (§11); multi-mesh picking blocks on the shared multi-mesh schema and is deferred to phase 2 (§11, tracked in [`../shared/query-commands.md`](../shared/query-commands.md)); and spatial-index reuse is bundled with the post-MVP `pick_ray` work — audit `Src/` for an existing BVH/octree before writing a new one, brute-force iterate if nothing exists (§11).)*
