# 03 — Server

## Scope

What `griz-server` needs to become for the UI effort: a three-thread process exposing the same command surface `batch_opt` always had, over an RPC transport, with state-event emission and server-side picking. Emphasis on **what must change from today's shipped stdio server** rather than a blank-slate redesign.

Out of scope: wire protocol (see [02-protocol](02-protocol.md)), frame generation (see [05-rendering-and-streaming](05-rendering-and-streaming.md)), pick mechanics (see [06-picking-and-queries](06-picking-and-queries.md)).

## Related

- `../UI.md` §3.1 (Server), §9 (Decoupling `gui.c`)
- [`../shared/server-binary.md`](../shared/server-binary.md), [`../shared/output-capture.md`](../shared/output-capture.md), [`../shared/query-commands.md`](../shared/query-commands.md)
- [01-architecture](01-architecture.md) §§2, 6 — process inventory and threading contract.
- [02-protocol](02-protocol.md) — RPC framing and auth.

## 1. Starting point (what already exists)

The `griz-server` binary ships today and powers the MCP Python bridge. The UI effort is **additive** on top, not a rebuild. Useful as foundation:

| Piece | Where | Reuse verbatim for RPC? |
|-------|-------|-------------------------|
| `main()`, flag parser | `Src/server_main.c` | Yes; extend with `--bind`, `--rendezvous`, `--protocol-version` flags and wire the `rpc` branch (today a stub at `:96–100`). |
| Startup: Analysis struct, DB open, OSMesa context, initial render, ready event | `Src/viewer.c:3118–3204` (`process_server_mode_stdio`) | Yes; factor out into `server_core_startup(Analysis **out, const char *db, int w, int h)`. |
| JSON envelope: parse / emit / error / hello | `Src/server_core.c` (521 lines) | Yes; transport-neutral already. |
| Output capture (fd `dup2` + tmpfile) | `Src/server_core.c:310–446` | Yes. Wired per-command in the dispatch loop. |
| Query dispatcher (`q_*` before `parse_command`) | `Src/viewer.c:3082–3116` (`server_try_query`) | Yes; factor out of viewer.c. |
| Query builders | `Src/viewer.c:2955–3076` (build_q_*) | Yes; extend (see §4). |
| Compile gate | `-DGRIZ_SERVER_BUILD`, `serial_batch_mode = TRUE` | Yes; keep the gate, rename the mode flag in a follow-on. |

The stdio **dispatch loop** itself (`Src/viewer.c:3205–3279`) is the template for the RPC dispatcher — read frame, handshake, terminator, query, capture/begin, `parse_command`, capture/end, emit. Porting this to RPC is mostly mechanical once the transport layer exists.

## 2. Target shape

### 2.1 File layout (post-refactor)

```
Src/
  server_main.c           # entry, flag parse, transport select (shipped; extend)
  server_core.c / .h      # envelope, handshake, output capture, error recording,
                          #   state-event helpers (shipped; extend with
                          #   notify_state, state_seq)
  server_core_startup.c   # NEW — factored out of viewer.c:process_server_mode_stdio
  server_stdio.c          # NEW — lift viewer.c:3205–3279 into its own TU
  server_rpc.c            # NEW — bind, accept, framed I/O, thread dispatch
  server_query.c          # NEW — q_* builders, today embedded in viewer.c
  server_events.c         # NEW — notify_state(), diff coalescer, seq counter
```

Rationale for extracting from `viewer.c` (which is 3634 lines and touches every analysis concern): keep the server TUs small, focused, and greppable. Link into `SERVER_OBJS` in `Src/Makefile.Library` alongside the existing `server_main.o` + `server_core.o` + `cJSON.o` (see current target at `Src/Makefile.Library:241–285`).

### 2.2 Thread model

[01-architecture](01-architecture.md) §6 pins three threads. Today's stdio server is **single-threaded** and that is fine for MCP (a Python client holds a lock and never pipelines). RPC brings three threads:

- **Command thread.** Owns `Analysis *analy` globals. Serial `parse_command()` calls exactly as in today's stdio loop. Pulls request objects from an MPSC queue fed by the I/O thread. Emits responses and `state_changed` events onto the send queue.
- **Render thread.** Owns the OSMesa context (`Src/offscreen.c:OffscreenContext`). Owns all GL calls (invariant **I6**). Pulls render-request slots (single-slot mailbox, latest-wins) fed by the command thread after mutations and fed directly by the I/O thread for user interaction drags. Produces encoded frames onto the send queue.
- **I/O thread.** Owns the TCP socket. Reads framed messages, demuxes into the command queue or the pick queue (pick requests go to the render thread because they need the ID buffer), writes responses/events/frames in order arrived from the send queue.

All inter-thread queues are bounded MPSC with explicit drop policies (see §5 back-pressure). No thread blocks on another's queue full: writers either drop or flip a latest-wins slot.

### 2.3 Dispatcher as an RPC

The invariant **I1** is unchanged: all state mutation routes through `parse_command()` in `Src/interpret.c`. An RPC `RunCommand(text)` resolves to exactly `parse_command(text, analy)` on the command thread, inside `server_capture_begin/end` and `server_clear_error/peek_error` guards — identical to today's stdio loop at `Src/viewer.c:3254–3277`.

Query RPCs (`q_state` et al.) bypass `parse_command` and hit the builders directly, same as today's `server_try_query` at `Src/viewer.c:3082–3116`.

Pick RPCs (new, see [06-picking-and-queries](06-picking-and-queries.md)) do **not** go through `parse_command`; they hit a new `server_pick()` helper on the render thread that reads from the ID buffer.

Input RPCs (camera rotate/translate/zoom from mouse drag) are translated into Griz text commands at the client (e.g. `rx 15`, `tx 0.1`) and fed through `RunCommand`. No parallel mutation path (**I1**).

## 3. Output capture

[`../shared/output-capture.md`](../shared/output-capture.md) describes the shipped fd-`dup2` mechanism. No changes for RPC: the per-command `begin → parse_command → end` sequence in `Src/viewer.c:3254–3277` lifts directly into the new `server_stdio.c` and into a matching call on the command thread in the RPC path. The captured text goes into `response.stdout` / `response.stderr` exactly as today.

## 4. Query commands — gap close

[`../shared/query-commands.md`](../shared/query-commands.md) § Current state documents what's shipped vs. aspirational. For the UI effort:

**Required for v1 of the Qt client:**

- `q_state`, `q_time`, `q_view`, `q_materials`, `q_results` — already shipped; extend payloads to full schema.
- `q_selection` — required for the selection panel. Implement against `analy->selected_elems` / equivalent; see [06-picking-and-queries](06-picking-and-queries.md) for selection state ownership.
- `q_render` — required for the render-options panel.
- `q_database` — populate the document title and the database info dialog.

**Payload completeness:**

- `q_view`: today returns only `viewport: {w, h}` (`Src/viewer.c:2980–2990`). Extend to include `rotate`, `translate`, `scale`, `zoom` from `analy->view_angle`, `analy->trans`, `analy->view_scale`, `analy->zoom_scale`.
- `q_materials`: today returns `{id, visible, enabled}` (`Src/viewer.c:3020–3045`). Add `label` (from `MESH_P(analy)->material_names`), `color` (from `v_win->material_properties`).
- `q_results`: today returns terse griz names (`sx`, `seff`). Option A: add a human-readable translation server-side by loading `Src/data/results_map.yaml` (see [`../shared/results-map.md`](../shared/results-map.md) § Server (optional)). Option B: leave translation client-side, ship the YAML with the Qt client. **Recommend option B** — keeps the server free of YAML deps, mirrors the Python layer.

## 5. State-event emitter

This is the largest net-new subsystem and touches `interpret.c`. Shipped today: nothing. The shape of events is pinned in [`../shared/command-protocol.md`](../shared/command-protocol.md) § State event and [`../shared/query-commands.md`](../shared/query-commands.md) § State-changed event shape.

### 5.1 Instrumentation

Every command handler in `Src/interpret.c` that mutates viewer state gets a post-success call to `notify_state(key, value)` (or `notify_state_object(key, obj)` for sub-object updates). The set of handlers to touch, ordered by frequency of user action:

| Source of change | Mutation site | Event key |
|------------------|---------------|-----------|
| Camera rotation (`rx`, `ry`, `rz`, `rxyz`, `vcent`) | `Src/interpret.c` view handlers | `view.rotate` |
| Camera translation (`tx`, `ty`, `tz`) | same | `view.translate` |
| Camera scale / zoom (`scale`, `zoom`) | same | `view.scale` / `view.zoom` |
| Viewport resize (`switch` / window size) | `Src/interpret.c` switch handler + `Src/viewer.c` window size | `view.viewport` |
| Time state (`state`, `next`, `prev`, `animate`) | `Src/interpret.c` time handlers | `time` |
| Result selection (`show <name>`) | `Src/results.c` and `Src/interpret.c` show/ show-field | `results.active` |
| Material visibility (`hide`, `vis`, `disable`, etc.) | `Src/interpret.c` material handlers | `materials[...]` |
| Render mode (`wire`, `solid`, `mat`, ...) | `Src/interpret.c` render-mode handlers | `render.mode` |
| Toggles (`ontime`, `oncmap`, ...) | same | `render.toggles.*` |
| Selection mutation (pick, box-select, clear) | new pick subsystem + `Src/interpret.c:hilite`/`clrhil` | `selection` |

Count is ~30–50 call sites. Each change is ≤3 lines. A central helper keeps naming consistent:

```c
/* In server_events.c, compiled only under GRIZ_SERVER_BUILD. */
void notify_state(const char *key, cJSON *value);          /* value steals */
void notify_state_path(const char *dotted, cJSON *value);  /* e.g. "view.rotate" */
```

In non-server builds these are `#define`d to `(void)0` so `interpret.c` stays compilable in the legacy GUI and batch builds (invariant **I8**).

### 5.2 Event emission and sequencing

- **`state_seq`** is a `static uint64_t` in `server_events.c`, incremented on each emitted event.
- **Coalescing.** The I/O thread, not `notify_state`, emits the actual event. `notify_state` writes into a "pending diff" dict protected by a mutex. The I/O thread drains and emits after each command response, **before** the next command begins (the ordering guarantee in [`../shared/command-protocol.md`](../shared/command-protocol.md) § Pipelining).
- **Overflow.** If the client is slow and the send queue is full, the I/O thread drops the oldest pending `state_changed` events and sets an `overflow` flag. When the queue drains, emit a single `{"type":"event","event":"state_overflow","state_seq":N}` sentinel. The client MUST respond with a fresh `q_state` request; the server replies with a full snapshot, and `state_changed` events resume from the new `state_seq`. No per-key buffering on the server, no delta replay — single sentinel + full snapshot is the only resync path. Rationale: `q_state` is cheap and overflow is rare by definition; per-key buffering would add a second pending-diff dict, lifetime management, and a delta-vs-snapshot decision tree on the client for a path that fires roughly never. Revisit only if measurement shows `q_state` payloads >100 KB or response time >50 ms.

### 5.3 Event flow and invariants

- Monotonic **state_seq** (invariant **I3**).
- Event-for-N is emitted before response-for-N+1 is dispatched.
- `state_changed` carries only changed keys, always with absolute (not delta) values (see the schema in [`../shared/query-commands.md`](../shared/query-commands.md)).

## 6. Decoupling from `gui.c`

`Src/gui.c` is 9928 lines and untouched by the server build — the compile gate already takes care of not linking it. What remains is the ways **engine code** historically assumed the GUI was present:

- `popup_dialog(dtype, fmt, ...)`: already handled. Under `GRIZ_SERVER_BUILD`, calls route to `server_record_error()` (`Src/server_core.c:251–293`). No GUI dependency.
- `wrt_text(fmt, ...)`: goes to stdout in batch/server; captured by the `dup2` output sink. No further work.
- Direct Motif calls (`XmText…`, `XtVaSetValues…`) inside `interpret.c` or `results.c`: **audit required** as part of the stage-1 refactor. Today these paths are gated by `#ifdef X_SERVER_AVAILABLE`-style macros derived from configure and/or `serial_batch_mode` runtime check; a grep pass under `-DGRIZ_SERVER_BUILD` should confirm nothing slipped through. Concretely:

```
grep -n 'XtV\|XmText\|XmCreate\|XtAppAddWorkProc' \
     Src/interpret.c Src/results.c Src/draw.c Src/offscreen.c
```

Any hit inside code reachable from the server dispatch loop needs the same treatment as `popup_dialog` — either gated out, or routed through a `griz_*` sink.

- Global mutable state: `Src/viewer.c` and `Src/interpret.c` share many globals (`analy_ptr`, `env`, `session`). The server reuses these as-is; single-command-thread dispatch preserves the single-writer assumption. Moving more state under `Analysis *` is orthogonal and not blocking.

## 7. Session state, lifecycle, shutdown

### 7.1 Session model

One session per `griz-server` process (invariant: one DB, one analysis, one OSMesa context). Rationale in [01-architecture](01-architecture.md) Open questions.

### 7.2 Startup

As already shipped (`Src/viewer.c:3118–3204`): zero env → NEW Analysis → NEW Session → `serial_batch_mode = TRUE` → `open_analysis()` → `OffscreenContext()` → `init_mesh_window()` → initial `update_display()` → emit `ready`.

Extensions for RPC:
1. Bind on `127.0.0.1:0`, record assigned port.
2. Generate 32-byte token, write rendezvous JSON (`$HOME/.griz/rendezvous/{session-id}.json`, mode 0600).
3. `listen()`, `accept()` (once; v1 is single-connection).
4. Validate client token (first frame).
5. Enter dispatch loop.

### 7.3 Shutdown

Today (`Src/viewer.c:3279–3301`): `quit`/`exit`/`end` command breaks the loop, history file is cleaned up, Analysis/OSMesa teardown is **intentionally skipped** — the comment at `:3292–3299` explains that `write_image_file()` already released the render buffer, so the batch cleanup path double-frees. This is fine on OS-managed exit.

Additions for RPC:
1. **SIGTERM handler.** Install in `server_core.c`. On receipt: emit `{"type":"event","event":"session_ending","reason":"signal_term","seconds_remaining":N}`, flush send queue with a short grace (≤2 s), exit. SLURM sends SIGTERM before SIGKILL at walltime, so a handler is the right spot.
2. **Peer close.** I/O thread's read returns 0/EOF → treat as `quit`, same cleanup. The server exits; reconnect-to-live-server is **out of scope for v1** (see [01-architecture](01-architecture.md) Invariant I12). A user whose SSH tunnel drops must relaunch and reload the analysis. Revisit post-MVP once disconnect frequency is observed in practice.
3. **Rendezvous file.** Delete on clean exit; leave on crash (the client sweeps on successful connect anyway).
4. **History file.** Already cleaned up (`Src/viewer.c:3281–3290`); reuse.

### 7.4 Logging

`stderr` in the server process is captured per-command by the output sink, which routes it into `response.stderr`. What doesn't fit this model:

- Startup errors before the first `ready` — today go to the raw `stderr` fd (`Src/server_main.c:44–89`). Keep that.
- Long-lived diagnostics the operator wants (thread-state, queue depth, frame-drop rate) — new, send to a rotating log file `$HOME/.griz/logs/{session-id}.log`. Not correlated to any single command. Cheap; no blocking.

## 8. Resource footprint

Primary sizing inputs:

- **OSMesa buffer:** `W × H × 4 bytes` (RGBA8) + an equal-sized ID buffer for picks. 1024² → 8 MiB total. 2K → 32 MiB. 4K → 128 MiB.
- **Analysis struct:** dominated by mesh state. Scales linearly with element count. Large runs target sessions at hundreds of MiB to a few GiB.
- **Result caches:** derived results are LRU-cached in the existing `analy->derived` paths; no server-specific bloat.
- **Per-session overhead** (threads, queues, send buffers): sub-10 MiB. Not a factor.

SLURM defaults in [07-launch-ssh-slurm](07-launch-ssh-slurm.md) should guide partition/memory requests on this basis. A rule of thumb: `mem = 4 × Mili_DB_size` is conservative for typical runs.

CPU:
- **Render thread** is the hot one during interaction. OSMesa fixed-function rendering is CPU-bound at large mesh sizes. A server node with 32+ cores helps if the fixed-function pipeline can exploit them — today it can't, so single-thread speed matters most.
- **Command thread** is idle most of the time.
- **I/O thread** is idle most of the time.

## 9. Remaining gaps list

Concrete ordered TODOs from the current server to a UI-ready server:

1. Extract `server_core_startup.c` and `server_stdio.c` from `Src/viewer.c:3118–3302`. No behavior change.
2. Extract `server_query.c` from `Src/viewer.c:2955–3116`. No behavior change.
3. Flesh out `q_state`/`q_view`/`q_materials` payloads to match the full schema (§4).
4. Implement `q_selection`, `q_render`, `q_database` (§4).
5. Implement `notify_state` + `server_events.c` + `state_changed` emission (§5).
6. Implement `Src/server_rpc.c`: length-framed I/O, bind/rendezvous/accept, token auth.
7. Split single dispatch into three threads (§2.2).
8. Wire SIGTERM → `session_ending` (§7.3).
9. Install picking (`Src/server_pick.c`), per [06-picking-and-queries](06-picking-and-queries.md).
10. Install frame-push pipeline, per [05-rendering-and-streaming](05-rendering-and-streaming.md).
11. Rename `serial_batch_mode` → `griz_server_mode` (or similar). Precondition: steps 1–2 landed so the rename is mechanical under the compile gate. Cosmetic; non-blocking.

Steps 1–5 unblock the MCP side too (richer state, push events) and do not depend on RPC. Steps 6–10 are the RPC-specific critical path.

## Open questions

*(All server-level open questions resolved as of 2026-04-19; resolutions are captured in §5.2 (overflow), §7.3 (no reconnect in v1), §9 step 11 (`serial_batch_mode` rename), and the cascading I12 update in [01-architecture](01-architecture.md).)*
