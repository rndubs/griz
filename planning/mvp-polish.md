# MVP Polish — Implementation Plan

Tracks the work items from the MVP checklist in [MCP.md](MCP.md) §0.

## Already done (discovered during review)

- [x] **Basic worker command timeout** — `Worker.cmd()` already has a 30s default timeout with per-call `timeout=` kwarg. Uses `time.monotonic()` deadline + condition variable. Raises `WorkerError` on timeout.
- [x] **Clear error when `griz-server` not on PATH** — `_find_griz_server()` in `worker.py:403-427` already checks `GRIZ_BIN` env var, `PATH`, and repo build dirs, raising `FileNotFoundError` with a message like `"griz-server binary not found. Set GRIZ_BIN or build with ./build.sh server."`.

## 1. `q_results` — server-side query command

**Goal**: Return the list of available result names so `field.list()` and the `list_fields` MCP tool work.

**Approach**: Iterate the static `trans_result[][4]` table in `Src/results.c:3600`. Each entry has `{result_id, title, compute_func, command_name}`. This table is the canonical list of all known Griz result command strings (`sx`, `sy`, `temp`, `dispx`, etc.) with human-readable titles. For each entry with a non-NULL compute function, check if it's computable for the current database using `find_result()`. Return the available subset.

**C changes** (`Src/viewer.c`):
- Add `build_q_results_data(Analysis *analy)` — returns cJSON with:
  - `results` array: each entry is `{name: "sx", title: "X Stress"}` for available results
  - `current` object: `{name, title}` from `analy->cur_result` (or null)
- Add `"q_results"` branch to `server_try_query()` dispatch

**Data shape returned**:
```json
{
  "results": [
    {"name": "sx", "title": "X Stress"},
    {"name": "sy", "title": "Y Stress"},
    {"name": "temp", "title": "Temperature"},
    ...
  ],
  "current": {"name": "seff", "title": "Effective Stress"}
}
```

**Python changes** (`pygriz/src/griz/field.py`):
- `field.list()` already calls `worker.cmd("q_results")` and extracts `data["results"]`. Should work once server-side lands. Verify and adjust key names if needed.

**Key files**: `Src/viewer.c` (dispatch + builder), `Src/results.c` (trans_result table, find_result), `Src/results.h` (Result structs)

## 2. `q_materials` — server-side query command

**Goal**: Return per-material visibility so `materials.list()` works.

**Approach**: The mesh stores materials as simple arrays indexed by material number: `MESH(analy).material_qty` gives the count, `MESH(analy).hide_material[i]` and `MESH(analy).disable_material[i]` give visibility/enable state (0=visible/enabled, 1=hidden/disabled).

**C changes** (`Src/viewer.c`):
- Add `build_q_materials_data(Analysis *analy)` — returns cJSON with:
  - `materials` array: each entry is `{id: N, visible: true/false, enabled: true/false}`
  - `total` count
- Add `"q_materials"` branch to `server_try_query()` dispatch

**Data shape returned**:
```json
{
  "total": 5,
  "materials": [
    {"id": 1, "visible": true, "enabled": true},
    {"id": 2, "visible": true, "enabled": true},
    {"id": 3, "visible": false, "enabled": true}
  ]
}
```

Note: material IDs are 1-based in Griz's user-facing commands (`vis 1 2 3`) but the arrays are 0-indexed. The returned `id` field should use 1-based numbering to match command syntax.

**Python changes** (`pygriz/src/griz/materials.py`):
- `materials.list()` already calls `worker.cmd("q_materials")` and extracts `data["materials"]`. Verify key names match.

**Key files**: `Src/viewer.c`, `Src/mesh.h` (Mesh_data struct with hide_material/disable_material arrays)

## 3. Screenshot format conversion

**Goal**: MCP clients need PNG, not SGI RGB.

**Approach**: Convert SGI RGB → PNG in Python. Two options:

- **Option A (no new deps)**: Parse SGI RGB header with `struct` module, extract pixel data, write PNG using `zlib` + minimal PNG encoder. SGI RGB format is simple (512-byte header, RLE or verbatim scanlines).
- **Option B (optional dep)**: If Pillow is available, use `PIL.Image.open()` which handles SGI RGB natively. Fall back to raw bytes if Pillow is missing.

**Recommendation**: Option A for the core (`session.py`), since adding a dependency just for format conversion is heavy. The conversion function goes in a new `griz/_sgi.py` module.

**Changes**:
- `pygriz/src/griz/_sgi.py` — `sgi_to_png(rgb_bytes) -> png_bytes`
- `pygriz/src/griz/session.py` — `screenshot()` calls `sgi_to_png()` when returning bytes
- `pygriz_mcp/src/griz_mcp/server.py` — `screenshot` tool wraps result as `Image(data=..., format="png")`

## 4. MCP tool descriptions

**Goal**: LLMs need enough context in tool docstrings to use the tools effectively.

**Changes** (`pygriz_mcp/src/griz_mcp/server.py`):
- Expand all `@mcp.tool()` docstrings with:
  - What the tool does and when to use it
  - Parameter descriptions (field names, valid ranges, etc.)
  - What the return value contains
  - For `show_field`: list common field names from results_map.yaml
  - For `open_database`: explain what a Mili database path looks like
  - For `screenshot`: explain the returned image format

## 5. Update `shared/output-capture.md`

**Goal**: Align the doc with what actually shipped.

**Changes** (`planning/shared/output-capture.md`):
- Update §2 "Design" to describe the fd-level `dup2` approach as the primary implementation
- Keep the source-level `griz_out()`/`griz_err()` sink API as a future improvement option
- Note that the fd-level approach subsumes the source-level audit (§3) since all writes to fd 1/2 are captured automatically

## Implementation order

1. `q_materials` in C (simplest — no hash table iteration, just array reads)
2. `q_results` in C (needs trans_result iteration + find_result availability check)
3. Screenshot format conversion (Python-only, self-contained)
4. MCP tool descriptions (text-only, quick)
5. Update output-capture.md (doc-only, quick)
6. Update MCP.md checklist — check off completed items
