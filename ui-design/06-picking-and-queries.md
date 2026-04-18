# 06 — Picking and queries

## Scope

How users select geometry (nodes, elements, materials) and retrieve data about it, without the client ever having the mesh. Covers the pick mechanism, selection semantics, and arbitrary metadata queries.

Out of scope: protocol framing (see [02-protocol](02-protocol.md)), the UI affordances for showing selection (see [04-client](04-client.md)).

## Related

- `UI.md` §5 (Selection)
- [02-protocol](02-protocol.md), [05-rendering-and-streaming](05-rendering-and-streaming.md)

## Sections to fill

- **Pick primitives.**
  - Point pick: client sends `(x, y)` in viewport coordinates plus mode (node / element / material / any). Server resolves to an ID.
  - Ray pick: client sends a ray in world space. Server intersects. Useful for hidden geometry or through clipping planes.
  - Box select: client sends a 2D rectangle. Server returns all enclosed IDs of the requested kind.
  - Lasso select: deferred; enumerate as future.
- **ID-buffer implementation.** Server renders an off-screen pass with element/node IDs packed into color bits. Readback pixel under cursor. Size of ID space (32-bit should be enough). Handling of overlapping primitives and transparent materials.
- **Ray / BVH path.** When needed. Reuse any existing spatial structures in the code. Consider whether Mili's indexing helps.
- **Selection state.** Owned by the server. Pushed to the client as `StateEvent`s when changed. Rendering highlights via a re-rendered frame.
- **Metadata queries.** Independent of picking. Examples: `QueryNode(id)` → coords, displacement, field values; `QueryElement(id)` → connectivity, material, per-integration-point results; `QueryMaterial(id)` → summary.
- **Interactive probes.** Hover-style picking that emits lightweight queries at reduced rate; design to avoid flooding the server.
- **Multi-select operations.** Select-by-material, select-connected, expand-to-neighbors. Defined as server-side operations over existing selection.

## Open questions

- Current Griz picking behavior — is it ID-buffer, geometric, or something else? Port or redesign?
- Do we need selection history (undo-style) client-side?
- How are probes shown in the viewport (labels drawn server-side in the frame) vs. overlaid by the client (requires knowing projected coordinates)?
