# Griz MCP Server — Design & Implementation Plan

## 0. Implementation Status

Top-level progress tracker. Detailed design for each topic lives in
[`mcp/`](mcp/); phase breakdown and milestone criteria live in
[`mcp/08-phasing.md`](mcp/08-phasing.md).

### MVP polish — high priority before first user test

Must fix (users will hit these immediately):

- [ ] **Implement `q_results` server-side** — `list_fields` MCP tool and `field.list()` / `field.info()` all error with `unknown_command` today. A user asking "what fields are available?" gets a crash instead of an answer.
- [ ] **Implement `q_materials` server-side** — `materials.list()` errors the same way. Users can `hide`/`show` by id but can't discover what materials exist.
- [ ] **Screenshot format usable by MCP clients** — `outrgb` produces SGI RGB files; most MCP clients (Claude Desktop, etc.) can't display that. Either enable PNG in the build (`--enable-nopng` removed) or add Python-side conversion (PIL/`rgb→png`). Without this, `screenshot` is effectively broken for MCP users.
- [ ] **Basic worker command timeout** — if `griz-server` hangs (bad DB, GL stall), the Python side blocks forever with no recovery. Add a default timeout (e.g. 30s) to `Worker.cmd()` with a clear `TimeoutError`.

Should fix (rough edges that erode trust):

- [ ] **Clear error when `griz-server` not on PATH** — today this surfaces as a cryptic `FileNotFoundError` from `subprocess.Popen`. Catch it and tell the user what to do.
- [ ] **End-to-end smoke test through the MCP protocol** — verify the full flow (MCP client → `griz-mcp` → `griz` → `griz-server`) with a real Mili database, not just unit tests with mocks.
- [ ] **MCP tool descriptions tuned for LLM consumption** — tool docstrings should list available field names, explain what a "state" is, etc. so the LLM can use the tools without guessing.
- [ ] **Update `shared/output-capture.md`** — the doc describes source-level `griz_out()`/`griz_err()` sinks but the implementation uses fd-level `dup2` redirect. Align the doc to what shipped.

### Planning documents (design complete when checked)

- [x] [mcp/01-architecture.md](mcp/01-architecture.md) — system overview & component layering
- [x] [mcp/02-server-binary.md](mcp/02-server-binary.md) — `griz-server` C implementation
- [x] [mcp/03-python-api.md](mcp/03-python-api.md) — `griz` Python package design
- [x] [mcp/04-mcp-adapter.md](mcp/04-mcp-adapter.md) — `griz-mcp` tool surface
- [x] [mcp/05-protocol.md](mcp/05-protocol.md) — JSON envelope, handshake, error taxonomy
- [x] [mcp/06-results-mapping.md](mcp/06-results-mapping.md) — YAML-backed field name map
- [x] [mcp/07-testing.md](mcp/07-testing.md) — unit/integration/perf test strategy
- [x] [mcp/08-phasing.md](mcp/08-phasing.md) — phase breakdown & milestone criteria

### Phase 1 — Foundation & smoke test ([02](mcp/02-server-binary.md), [03](mcp/03-python-api.md), [08 §2.1](mcp/08-phasing.md))

- [x] `griz-server` target builds from `batchopt` objects
- [x] Server accepts plain-text commands via stdin (`process_server_mode_stdio()` in `Src/viewer.c`)
- [x] OSMesa rendering works headlessly in server mode (verified via `outrgb` producing a valid SGI image at the requested dimensions)
- [ ] `outpng` produces valid PNG files from server mode *(blocked: default configure uses `--enable-nopng`; needs a build with PNG support)*
- [x] Minimal Python `Worker` spawns server and sends commands *(uv-managed package `llnl-griz` at `pygriz/`, importable as `griz`; `Worker` waits for the `READY` sentinel, drains stdout/stderr in background threads, sends plain-text commands, and shuts down via `quit`. Verified by `pygriz/tests/test_worker.py` — 5 passing)*
- [x] Clean shutdown with no resource leaks on the command-loop exit path *(was blocked by a `double free or corruption` abort in `outrgb`; root cause: `ImageLib` typedefs `SIGNED_4BYTE`/`UNSIGNED_4BYTE` as `long` on Linux, which expands to 8 bytes on 64-bit, and `cvtimage()` in `Src/ImageLib/open.c` used hard-coded `buffer+26` offsets that assumed 4-byte values. That write landed past the end of the `IMAGE` struct and corrupted the malloc heap, aborting in the next `free()`. Fixed in `Src/ImageLib/image.h` by pinning those typedefs to `int32_t`/`uint32_t` on `__linux`; batch + server both exit cleanly now.)*
- [x] End-to-end smoke test passes for stdin command loop + state navigation + RGB screenshot; full image-format coverage gated on the two items above

### Phase 2 — JSON protocol & output capture ([02](mcp/02-server-binary.md), [05](mcp/05-protocol.md), [08 §2.2](mcp/08-phasing.md))

- [x] JSON request/response envelope (cJSON integration) *(cJSON 1.7.19 vendored at `Src/ext/cJSON/`; `server_core.{c,h}` wraps request parsing + response emission; `process_server_mode_stdio` accepts both raw lines and `{"type":"request",...}` and emits one response line per command, with populated `stdout`/`stderr` fields via the fd-level output capture below.)*
- [x] Handshake sequence (`ready` → `hello` → `hello_ack`) *(startup emits `{"type":"event","event":"ready","version":"1.0",...}`; `server_try_hello` consumes inline `{"type":"hello",...}` frames and emits `hello_ack` with `compatible` flag. Hello is optional — non-hello lines fall through to request processing.)*
- [x] `griz_out()` / `griz_err()` sink indirection implemented *(done via fd-level redirect: `server_capture_begin` / `server_capture_end` in `server_core.c` dup2 stdout/stderr to tmpfile()s around each parse_command, then drain them into the response's `stdout`/`stderr` fields. Capture is capped at 256 KB per stream with an `…[truncated]` sentinel. The fd-level approach subsumes a source-level `griz_out`/`griz_err` wrapper — every `printf`/`fprintf(stdout,…)`/`write(1,…)` routes through the redirect automatically.)*
- [x] Audit & replace `printf` / `fprintf(stdout,…)` on batch paths *(subsumed by the fd-level capture above — no source-level audit required. Verified by `test_stdout_is_captured_into_response`: the `help` command's multi-line output lands in `response.stdout` instead of interleaving with the JSON stream.)*
- [x] Structured error taxonomy with `code` field *(popup_dialog now calls `server_record_error` in GRIZ_SERVER_BUILD; the command loop translates captured diagnostics into `{"status":"error","error":{"code":...,"message":...}}`. Codes: `invalid_syntax` (USAGE_POPUP), `command_error` (WARNING_POPUP / generic), `unknown_command` (INFO_POPUP matching "not valid"). INFO_POPUP notices without error keywords do not raise.)*
- [x] Query commands: `q_state`, `q_view`, `q_time` *(dispatched before parse_command in viewer.c's server loop; return `data` field with time_state / max_time_state / state_count / time_value / max_time_value / viewport / current_field. Camera/materials/colormap still TODO.)*
- [x] Python worker parses JSON and translates errors to exceptions *(Worker uses a reader thread to route responses/events; `cmd(command)` sends a JSON request with an auto-generated id, blocks for the matching response, raises `GrizCommandError(code, message)` on status:error. `send_command(raw)` retained for fire-and-forget / crashy paths.)*
- [x] Protocol edge cases covered by integration tests *(14 tests in `pygriz/tests/test_worker.py`: handshake populates server_info, cmd round-trip, raw+JSON envelope paths, malformed JSON → invalid_request, unknown command → unknown_command, q_state/q_view/q_time shapes, stdout-capture round-trip.)*

### Phase 3 — Python API package ([03](mcp/03-python-api.md), [06](mcp/06-results-mapping.md), [08 §2.3](mcp/08-phasing.md))

- [x] `Griz` class with context manager (`__enter__` / `__exit__`) *(in `pygriz/src/griz/session.py`; lazy worker spawn on first `open()`, `close()` / `__exit__` tear down; `reload()` closes + reopens; `worker_factory` kwarg lets tests inject a mock.)*
- [x] `field` namespace (`show`, `list`, `info`) *(`field.show` resolves through `ResultsMap` and drives `show <griz-name>`. `field.list` / `field.info` issue `q_results` / `q_result_info` — both surface `GrizCommandError(code="unknown_command")` until server-side queries land, see Phase 4 gating below.)*
- [x] `view` namespace (`rotate`, `translate`, `scale`, `zoom`, `reset`) *(wraps `rx`/`ry`/`rz`, `tx`/`ty`/`tz`, `scale`, and `rview`; `zoom` is an alias for `scale`.)*
- [x] `time` namespace (`set_state`, `set_time`, `animate`) *(also `next` / `prev`; `animate()` walks the state range locally with an optional delay and returns per-frame state dicts, supports negative step.)*
- [x] `materials` namespace (`hide`, `show`, `list`) *(uses `vis` / `invis` commands; `list` / `show_only` depend on `q_materials` — not yet implemented server-side, will raise until it lands.)*
- [x] Top-level: `select`, `highlight`, `clear_picks`, `screenshot`, `state`, `raw` *(`select`/`hilite`/`clrhil` wrappers; `screenshot()` uses `outrgb` and returns path or bytes; `state()` calls `q_state`; `raw()` returns the full response dict and passes through the optional timeout.)*
- [x] `Src/data/results_map.yaml` and YAML loader *(canonical YAML lives in `Src/data/`; `pygriz/src/griz/data/results_map.yaml` is a symlink so hatch ships the real file in the wheel. Loader lives in `griz/results_map.py` with `default_map()` as a lazy singleton; `GRIZ_RESULTS_MAP` env var overrides the bundled copy for testing.)*
- [x] Unit tests with mocked worker (>90% coverage) *(non-worker coverage ≥93% per module: `__init__` 100%, `exceptions` 100%, `field` 100%, `view` 100%, `selection` 100%, `materials` 97%, `time_` 97%, `results_map` 94%, `session` 93%. 51 new unit tests in `tests/test_results_map.py` + `tests/test_session.py`; mock worker at `tests/mock_worker.py` is a no-subprocess stand-in used for behavioral coverage.)*
- [x] `pyproject.toml` and pip-installable from `pygriz/` *(package name `llnl-griz`, imports as `griz`. `pyproject.toml` adds `pyyaml>=6.0` and a `test` extra with `pytest-cov`. `tool.hatch.build.targets.wheel.force-include` maps the symlinked YAML into the wheel. Note: lives at `pygriz/` rather than `Src/python/griz/` to keep Python out of the C autotools tree; promoting to `Src/python/griz/` is a Phase 5 packaging concern.)*

Gated on Phase 2 server work (tracked under shared/query-commands.md): `field.list`, `field.info`, `materials.list`, `materials.show_only` all depend on server-side `q_results` / `q_result_info` / `q_materials` which are not yet implemented. They are wired through so the method surface matches the design; they raise `GrizCommandError(code="unknown_command")` until the queries land.

### Phase 4 — MCP adapter ([04](mcp/04-mcp-adapter.md), [08 §2.4](mcp/08-phasing.md))

- [x] `griz-mcp` MCP server bootstraps and registers tools *(FastMCP 3.x at `pygriz_mcp/`; `mcp = FastMCP("griz-mcp")` with 14 `@mcp.tool` functions in `server.py`; entry point `griz-mcp` via `[project.scripts]`; module-level session singleton in `session.py` with factory injection for tests. 36 tests, 91% coverage.)*
- [x] Database tools: `open_database`, `close_database`
- [x] Field tools: `show_field`, `list_fields`
- [x] View tools: `rotate_view`, `reset_view`
- [x] Time tools: `set_time_state`, `animate`
- [x] Material tools: `hide_materials`, `show_materials`
- [x] `screenshot` returns MCP `ImageContent` *(returns `fastmcp.utilities.types.Image(data=bytes, format="rgb")`; format is SGI RGB since default build uses `--enable-nopng`)*
- [x] `get_state`, `restart_session`, `raw_command`
- [x] End-to-end MCP client transcript in README *(initialize → tools/list → open_database → show_field → rotate + screenshot → animate → close; see `pygriz_mcp/README.md`)*
- [x] Package publishable from `pygriz_mcp/` *(pip-installable via `uv sync`; lives at repo root parallel to `pygriz/` following the same convention — promotion to `Src/python/griz_mcp/` is a Phase 5 packaging concern)*

### Phase 5 — Polish & production readiness ([08 §2.5](mcp/08-phasing.md))

- [ ] Timeout & watchdog mechanisms
- [ ] Session restart / recovery
- [ ] Performance benchmarks meet targets (§6.1)
- [ ] Stress tests run 24h+ without leaks
- [ ] MCP prompt templates for common workflows
- [ ] CI pipeline across supported Python versions
- [ ] User guide and tutorial docs published

### Shared with UI effort ([shared/](shared/))

- [x] [shared/server-binary.md](shared/server-binary.md) — `griz-server` target & transports *(design complete; stdio transport implemented and working. RPC transport is a future UI concern.)*
- [x] [shared/command-protocol.md](shared/command-protocol.md) — envelope & handshake *(design complete; envelope, handshake, and error taxonomy all implemented in Phase 2. Open questions on back-pressure and cancellation are deferred to later phases.)*
- [x] [shared/output-capture.md](shared/output-capture.md) — `griz_out` / `griz_err` plumbing *(design complete; implementation uses fd-level `dup2` redirect rather than source-level sinks — functionally equivalent, doc update tracked in MVP polish above.)*
- [x] [shared/query-commands.md](shared/query-commands.md) — `q_*` commands & state schema *(design complete; `q_state`/`q_view`/`q_time` implemented. Remaining commands `q_results`/`q_materials`/`q_selection`/`q_render`/`q_database` tracked in MVP polish above.)*
- [x] [shared/results-map.md](shared/results-map.md) — `results_map.yaml` as single source of truth *(design complete; YAML file and Python loader implemented. Server-side generated header deferred until `q_results` lands.)*

---

## 1. Goal

Expose Griz's visualization capabilities from Python, so that:

1. AI assistants (and any other MCP-compatible client) can drive Griz via the
   Model Context Protocol — open a Mili database, change the view, select a
   result variable, step through time, capture rendered images.
2. Users can write their own Python scripts and notebooks that control Griz
   directly, without the MCP layer.

Both use cases are served by a single Python package (`griz`) that wraps a
new Griz "server mode." The MCP server is a thin adapter on top of that
package — not where the API lives.

```
  MCP client (Claude, …)            User's Python script / notebook
        │                                       │
        ▼                                       │
  ┌───────────────┐                             │
  │   griz-mcp    │  @mcp.tool() wrappers,      │
  │   (Python)    │  image formatting, etc.     │
  └───────────────┘                             │
        │                                       │
        └────────────┬──────────────────────────┘
                     ▼
              ┌────────────┐
              │    griz    │  Public Python API.
              │  (Python   │  `Griz` class with .field, .view, .time,
              │  package)  │  .materials, .select, .screenshot, …
              └────────────┘
                     │
                     ▼    stdio + line-delimited JSON
              ┌──────────────────────┐
              │  griz-server         │  New binary (shared with the Qt UI
              │  --transport=stdio   │  effort), produced from batchopt.
              └──────────────────────┘
```

> **Shared with the UI effort.** The C-side server binary, the JSON envelope,
> the output-capture plumbing, the `q_*` query commands, and the results-name
> map are all used by the Qt UI effort ([`UI.md`](UI.md)) as well. To avoid
> building any of this twice, those pieces are specified in [`shared/`](shared/)
> and referenced throughout the relevant sections below:
>
> - [`shared/server-binary.md`](shared/server-binary.md) — one `griz-server`
>   binary, `--transport={stdio,rpc}`. MCP uses `stdio`.
> - [`shared/command-protocol.md`](shared/command-protocol.md) — envelope,
>   handshake, error taxonomy.
> - [`shared/output-capture.md`](shared/output-capture.md) — `griz_out()` /
>   `griz_err()` sink indirection.
> - [`shared/query-commands.md`](shared/query-commands.md) — `q_state`,
>   `q_results`, `q_view`, `q_materials`, `q_selection`, plus the canonical
>   state schema.
> - [`shared/results-map.md`](shared/results-map.md) — YAML-backed
>   `(field, component) → griz name` mapping.

Key properties of this layering:

- **One public API, two front ends.** The `griz` package is what a user
  `pip install`s and `import`s. The MCP server is a separate, small package
  that imports `griz` and exposes its methods as MCP tools.
- **Out-of-process Python.** Griz does not embed a Python interpreter. The
  `griz` package runs under whatever CPython the user has installed and
  talks to the unmodified Griz C core over stdio. (Contrast VisIt/ParaView,
  which embed Python — more powerful, much more invasive to build.)
- **The wire protocol is an implementation detail.** Users never see raw
  Griz commands unless they ask for them via an explicit escape hatch.
  Clean, typed, namespaced Python is the API.

---

## 2. What Griz Has Today (Reference)

Relevant entry points identified during scoping:

- **Main:** `Src/viewer.c:231` — `main()`
- **Arg parsing:** `Src/viewer.c` — `scan_args()` handles `-i`, `-b/-batch`,
  `-f`, `-w`, etc.
- **Command interpreter:** `Src/interpret.c:639` — `parse_command()` (top-level
  parser, handles aliases and semicolon-separated compound commands).
- **Command dispatcher:** `Src/interpret.c:769` — `parse_single_command()`
  (the ~5,000-line if/else chain that actually executes commands).
- **Batch loop (current):** `Src/viewer.c:2823` — `process_serial_batch_mode()`,
  guarded by `#ifdef SERIAL_BATCH`. Reads a file char-by-char; calls
  `parse_command()` per line; exits on `quit`/`exit`/`end`.
- **Offscreen rendering:** OSMesa context in `Src/offscreen.c:69`
  (`OffscreenContext()`), used by batch builds.
- **Image output:** `outpng <file>`, `outpnga <file>`, `outjpeg <file>` in
  `Src/interpret.c:7087–7132`; `write_png_file()` in `Src/draw.c:16845`.
- **Build targets:** `debug`, `opt`, `batchdebug`, `batchopt` — batch builds
  link OSMesa and define `SERIAL_BATCH`.

What Griz **does not** have, and what we therefore need to add:

1. Any way to receive commands from a live process (no stdin reader, no FIFO,
   no socket).
2. Any way to report structured results back to a caller (commands today
   write free-form text via `popup_dialog`, `wrt_text`, etc.).
3. A non-terminating interactive mode in the headless build.

---

## 3. Options Considered

| Option | Summary | Verdict |
|---|---|---|
| **A. Wrap file-based batch mode per call** — MCP server writes a temp cmd file, spawns Griz, parses a PNG. | Works without any C changes. | Rejected — losing all session state between calls (view, loaded DB, selected result) makes this unusable for multi-turn interaction. |
| **B. Add stdin reader to batch mode** — one small C change so `griz -b -` reads newline commands from stdin. | Minimal code delta; matches VisIt bridge pattern. | **Yes, phase 1.** |
| **C. Add a line-delimited JSON protocol** (command+response framing) on top of B. | Gives the MCP server reliable per-command completion signals and structured errors. | **Yes, phase 2.** |
| **D. Embed a TCP/Unix-socket server in Griz.** | Enables multi-client and remote. | Deferred. More invasive; stdio is sufficient for a local MCP bridge. |
| **E. Link Griz as a library into a Python extension.** | Most powerful. | Deferred. Griz is not structured for in-process embedding (`main()` calls `exit()`; global state pervasive; Motif/Xt pulled in even for batch via shared headers). |

We commit to **B + C** (stdin reader + JSON framing) as the Griz-side change,
and a Python MCP server as the bridge.

---

## 4. Griz-Side Changes (C)

The C-side changes in this section are **shared with the UI effort**. Each
subsection points at the authoritative shared doc; what remains here is
MCP-specific wiring and context. If a decision in this section disagrees with
a `shared/` doc, the shared doc wins — please file an update.

### 4.1 New binary: `griz-server`

See [`shared/server-binary.md`](shared/server-binary.md) for the full
decision. Summary:

- The server-mode build artifact is a **new** binary, `griz-server`, produced
  from the existing `batchopt` object set. It is **not** a flag on today's
  `griz4b` / `griz_batch` — the legacy batch binary stays unchanged.
- `griz-server` accepts `--transport={stdio,rpc}`. MCP uses `stdio`. The Qt UI
  effort uses `rpc`. Everything above the transport (command dispatcher,
  output capture, query commands) is identical between the two.
- MCP invocation:

  ```
  griz-server --transport=stdio -i <database> -w 1024 1024
  ```

In the C source, the stdio dispatch loop lives in a new
`process_server_mode_stdio()` function alongside the existing
`process_serial_batch_mode()`, selected by `scan_args()` when
`--transport=stdio` is present. It is built whenever the `griz-server` target
is built (see [`shared/server-binary.md`](shared/server-binary.md) §Build).

### 4.2 Server loop

New function in `Src/viewer.c` (next to the existing batch loop):

```c
#ifdef SERIAL_BATCH
static void
process_server_mode( Analysis *analy )
{
    char line[MAX_STRING_LENGTH];

    init_mesh_window( analy );
    analy->update_display( analy );

    /* Signal readiness to the MCP server. */
    fputs("{\"event\":\"ready\"}\n", stdout);
    fflush(stdout);

    while ( fgets(line, sizeof line, stdin) != NULL )
    {
        strip_newline(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        if (is_terminator(line)) break;   /* quit/exit/end */

        server_dispatch(line, analy);     /* see 4.3 */
    }
}
#endif
```

Key properties:

- **Line-delimited**: one command per line, same grammar Griz already parses.
- **Blocking read**: `fgets` naturally pauses between commands — no polling,
  no threads.
- **Ready handshake**: a single `{"event":"ready"}` line so the MCP server
  knows startup (DB load, OSMesa init) finished before sending commands.
- **Flush after every write**: critical — without `fflush`, Python will block
  forever on `readline()` because stdout is block-buffered when not a TTY.

### 4.3 JSON response framing

**Authoritative doc:** [`shared/command-protocol.md`](shared/command-protocol.md).

`server_dispatch()` wraps `parse_command()` and emits one JSON object per
command, using the shared envelope. For stdio (MCP), framing is one JSON
object per newline (a.k.a. JSONL).

Request (either a bare command line for interactive debugging, or the JSON
form for the Python worker):

```json
{"type": "request", "id": "42", "cmd": "rx 30"}
```

Response (success / error, with `data` present for `q_*` queries and typed
error codes for failures — see the shared doc for the full taxonomy):

```json
{"type": "response", "id": "42", "status": "ok", "stdout": "...", "stderr": "", "data": null}
{"type": "response", "id": "42", "status": "error",
 "error": {"code": "unknown_command", "message": "unknown command: foo"}}
```

Two things changed from earlier MCP.md drafts to align with the shared
protocol: (1) errors now carry a `code` from a closed taxonomy instead of a
flat `message` field; (2) a one-time versioned handshake (`ready` →
`hello` → `hello_ack`) is required before the first request. Trivial overhead
for the Python worker; required by the UI RPC transport. See the shared doc.

### 4.4 Capturing Griz's text output

**Authoritative doc:** [`shared/output-capture.md`](shared/output-capture.md).

Summary: introduce `griz_out()` / `griz_err()` helpers. In GUI / legacy
batch builds they forward to the existing console helpers; in `griz-server`
they append to per-command buffers that `server_dispatch()` flushes into the
response's `stdout` / `stderr` fields. An audit pass replaces direct
`printf` / `fprintf(stdout,…)` / `puts(…)` calls on batch-reachable paths.
This is needed by both the MCP stdio transport and the UI RPC transport;
leakage through stdout / stderr would corrupt either one.

### 4.5 Structured query commands

**Authoritative doc:** [`shared/query-commands.md`](shared/query-commands.md).

A small set of new read-only commands (`q_state`, `q_time`, `q_view`,
`q_materials`, `q_results`, `q_selection`, `q_render`, `q_database`) expose
viewer state that today is only readable by a human. Each populates the
response's `data` field with a subset of the canonical state schema defined
in the shared doc.

MCP consumes these by calling them on demand from `Griz.state()` and the
sub-API methods. The UI effort consumes the same schema through its
`state_changed` event stream and uses `q_state` for initial snapshot and
event-gap recovery — so the commands themselves, and the dict they return,
must be identical across the two front ends.

Phase plan: `q_state`, `q_view`, `q_time`, `q_materials`, `q_results` are
phase-2 deliverables (landing with JSON framing). `q_selection`, `q_render`,
`q_database` can follow in phase 3 as the Python API grows.

### 4.6 Image capture

No new rendering code needed. The MCP server calls the existing `outpng
<path>` command. Because `griz-server` is built from the batch/OSMesa object
set, rendering goes through OSMesa and does not require an X display. The MCP
server reads the resulting file from disk and returns it as MCP `Image`
content.

(The UI RPC transport streams frames inline over the protocol rather than
going through `outpng` + disk. Both mechanisms are fine for their respective
transports; the MCP file-based path is simpler and kept for phase-1 stdio.
An inline-screenshot command may be added later — see
[`shared/command-protocol.md`](shared/command-protocol.md) § Open questions.)

### 4.7 Build system

See [`shared/server-binary.md`](shared/server-binary.md) § Build. Summary:
`griz-server` is a new target in `Src/Makefile.Library` built from the
`batchopt` object set plus `server_core.c` / `server_stdio.c` /
`server_rpc.c` (and their headers). stdio mode pulls no extra deps beyond
libc. Existing `griz` and `griz_batch` targets are untouched.

---

## 5. Python Layer

Two packages: `griz` (the public API, does the real work) and `griz-mcp`
(a thin adapter that turns `griz` into MCP tools).

### 5.1 Repository layout

```
Src/python/
├── griz/                          # Public Python API — "GrizAPI"
│   ├── __init__.py                # exports Griz, exceptions
│   ├── session.py                 # class Griz: lifecycle, raw() escape hatch
│   ├── worker.py                  # subprocess driver, stdio JSON framing
│   ├── field.py                   # g.field.show(), list(), info()
│   ├── view.py                    # g.view.rotate(), reset(), zoom()
│   ├── time.py                    # g.time.set_state(), set_time(), animate()
│   ├── selection.py               # g.select(), highlight()
│   ├── materials.py               # g.materials.hide(), show()
│   ├── results_map.py             # hand-curated (field, component) → griz cmd
│   ├── exceptions.py
│   └── tests/
└── griz_mcp/                      # MCP adapter — imports griz
    ├── __init__.py
    ├── server.py                  # MCP entrypoint
    ├── tools.py                   # @mcp.tool() wrappers around griz.Griz
    └── tests/
```

Separate top-level packages, each with its own `pyproject.toml`, both
publishable. `griz-mcp` depends on `griz`.

### 5.2 The `griz` package — public API

This is what users `import` in their own scripts. Target shape:

```python
from griz import Griz

with Griz("runs/blast.plt", width=1024, height=1024) as g:
    g.time.set_state(42)
    g.field.show("stress", component="xx")     # or component="von_mises"
    g.view.rotate(x=30, y=0, z=0)
    g.materials.hide([3, 7])
    g.screenshot("frame0042.png")
    info = g.state()                           # structured dict
```

Class sketch:

```python
class Griz:
    def __init__(self, database: str | None = None, *,
                 griz_bin: str | None = None,
                 width: int = 1024, height: int = 1024): ...

    # Lifecycle
    def open(self, path: str) -> None: ...
    def reload(self) -> None: ...
    def close(self) -> None: ...
    def __enter__(self) -> "Griz": ...
    def __exit__(self, *exc) -> None: ...

    # Namespaced sub-APIs (each is a small helper object bound to self._worker)
    field:     "FieldAPI"
    view:      "ViewAPI"
    time:      "TimeAPI"
    materials: "MaterialsAPI"

    # Flat top-level actions (things that don't fit a namespace)
    def select(self, kind: str, ids: list[int]): ...
    def highlight(self, kind: str, id: int): ...
    def clear_picks(self): ...
    def screenshot(self, path: str | None = None) -> bytes | str: ...
    def state(self) -> dict: ...           # current time/state/frame info

    # Escape hatch
    def raw(self, command: str) -> dict: ... # send a bare Griz command
```

Namespaces are plain Python attribute objects (no magic) — they exist
purely for discoverability and docstring grouping.

### 5.3 Worker — the stdio bridge (internal)

`griz._worker.Worker` owns the subprocess and serializes calls. This is an
implementation detail; it is not re-exported.

```python
class Worker:
    def __init__(self, griz_bin, database, width, height):
        self.proc = subprocess.Popen(
            [griz_bin, "--transport=stdio",
             "-i", database, "-w", str(width), str(height)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1,
        )
        ready = self._await_event("ready")
        self._handshake(ready)                # hello / hello_ack (shared/command-protocol.md)
        self._lock = threading.Lock()

    def cmd(self, griz_cmd: str, *, timeout: float = 30.0) -> dict:
        with self._lock:
            req = json.dumps({"type": "request", "cmd": griz_cmd})
            self.proc.stdin.write(req + "\n"); self.proc.stdin.flush()
            line = _readline_with_timeout(self.proc.stdout, timeout)
            return json.loads(line)
```

Notes:
- **Synchronous, locked.** Griz has global state; pipelining buys nothing
  and invites ordering bugs. Threading lock, not asyncio, so the library
  is usable from plain sync scripts and notebooks. An `asyncio`-friendly
  wrapper (`to_thread`) can live in `griz_mcp` where async matters.
- A background thread drains `stderr` and surfaces unexpected output on
  the next command's result.
- Shutdown: `quit` → `proc.wait(timeout=5)` → `proc.kill()`.

### 5.4 The results mapping

**Authoritative doc:** [`shared/results-map.md`](shared/results-map.md).

Griz encodes result names tersely (`sx`, `sy`, `exx`, `temp`, …). The Python
layer hides this via a YAML-backed mapping shipped as a data file at
`Src/data/results_map.yaml` — the single source of truth shared with the Qt
UI client, which loads the same file to populate its menus.

`griz/results_map.py` loads the YAML at import time and exposes a
`resolve(field, component=None) -> griz_name` helper. Lookup semantics are
unchanged from earlier drafts: `g.field.show("stress", component="xx")`
resolves to `res sx; show result`. Unknown `(field, component)` pairs raise
`UnknownFieldError` with a suggestion list built from the YAML. For names
not in the table, users can fall through with `g.raw("res <name>")`.

Docs for supported field / component pairs are generated from the same
YAML, so there is one place to add a field and one place to read about it.

### 5.5 The `griz_mcp` package — MCP adapter

Small, mostly declarative. One module of `@mcp.tool()` functions that hold
a single `Griz` session and delegate:

```python
# griz_mcp/tools.py
from griz import Griz
_session: Griz | None = None

def _sess() -> Griz:
    global _session
    if _session is None:
        _session = Griz(griz_bin=os.environ.get("GRIZ_BIN", "griz4b"))
    return _session

@mcp.tool()
def open_database(path: str) -> dict:
    _sess().open(path); return _sess().state()

@mcp.tool()
def show_field(name: str, component: str | None = None) -> dict:
    return _sess().field.show(name, component=component)

@mcp.tool()
def rotate_view(x: float = 0, y: float = 0, z: float = 0) -> dict:
    _sess().view.rotate(x=x, y=y, z=z); return _sess().state()

@mcp.tool()
def screenshot() -> Image:
    path = _sess().screenshot()          # writes to a temp file
    return Image(path=path)              # MCP Image content

@mcp.tool()
def raw_command(cmd: str) -> dict:
    return _sess().raw(cmd)              # escape hatch
```

Design points:
- **Flat tool names** (`show_field`, `rotate_view`, `set_state`) — easier
  for LLMs to discover. The namespacing lives inside the `griz` library
  where Python IDEs benefit from it.
- All state-mutating tools return a structured `state()` dict so the
  client has feedback for its next decision.
- `screenshot` returns MCP `Image` content (base64 PNG) — same pattern as
  ParaView MCP.

### 5.6 Initial tool / method inventory

Grouped by what the user sees. Each row is one `Griz` method **and** one
`griz-mcp` tool.

**Database / session** — `open`, `reload`, `close`, `state`
**Time** — `time.set_state`, `time.set_time`, `time.animate`
**Fields** — `field.show(name, component=…)`, `field.list()`, `field.info(name)`
**View** — `view.rotate(x,y,z)`, `view.translate(x,y,z)`, `view.scale`, `view.zoom`, `view.reset`, `view.center_on_node`
**Selection** — `select`, `unselect`, `highlight`, `clear_picks`
**Materials** — `materials.hide(ids)`, `materials.show(ids)`, `materials.list()`
**Rendering** — `set_render_mode("solid"|"wireframe"|"hidden"|"wft")`, `toggle("coord"|"time"|"cmap"|"minmax", on)`
**Output** — `screenshot`, `save_text`, `dump_result`
**Escape** — `raw(command)`

### 5.7 Configuration

Environment variables (consumed by `griz.Griz.__init__` via
`os.environ.get`):
- `GRIZ_BIN` — path to `griz-server`. Default: `griz-server` on PATH.
- `GRIZ_DEFAULT_WIDTH`, `GRIZ_DEFAULT_HEIGHT`.
- `GRIZ_WORKDIR` — where screenshots and transient files land.

The MCP server does *not* pre-launch Griz. The first `open_database` tool
call spawns the worker; subsequent calls reuse it. A `restart` tool tears
it down.

---

## 6. Phased Implementation

### Phase 1 — Smoke test, no JSON (1–2 days)

- Stand up the `griz-server` target (minimal skeleton, stdio transport only).
  Implement `process_server_mode_stdio()` reading plain newline-delimited
  commands from stdin.
- No response framing yet — just run commands.
- Python side spawns the subprocess and sends text; screenshot via
  `outpng` + file read.
- Verifies: stdio wiring, OSMesa rendering, stdin buffering/`fflush`.

### Phase 2 — JSON framing + output capture

- Add response envelope (section 4.3).
- Add the `griz_out()` / `griz_err()` sink indirection (section 4.4).
- Add `q_state`, `q_results`, `q_view` query commands.
- Python worker parses JSON; tool errors propagate as MCP errors.

### Phase 3 — Public `griz` package

- Build out the `Griz` class, namespaced sub-APIs (`field`, `view`, `time`,
  `materials`), and the results mapping table.
- Unit tests against a mock worker.
- Publishable from `Src/python/griz/`.

### Phase 4 — `griz-mcp` adapter

- `@mcp.tool()` wrappers around the `Griz` class (section 5.5).
- README with example prompts and end-to-end transcript.
- Screenshot tool returns MCP `Image` content.

### Phase 5 — Polish

- Timeouts, watchdog, clean shutdown.
- `restart()`, `status()` tools.
- Optional: OSMesa-free diagnostic build that returns dummy images, for CI.
- Optional: Unix-socket transport (section 3, option D) for multi-client.

---

## 7. Testing Strategy

- **Unit (Python, `griz`):** mock the internal `Worker` → exercise every
  `Griz` method's command serialization, results-map lookups, error paths.
  No subprocess, fully hermetic.
- **Unit (Python, `griz_mcp`):** monkeypatch `griz.Griz` with a fake →
  verify each `@mcp.tool()` wires arguments through and packages
  responses correctly (including `Image` content for screenshots).
- **Integration:** a tiny Mili fixture database checked into
  `Src/python/griz/tests/fixtures/`. Test harness spawns real
  `griz-server --transport=stdio`, drives it with the real `Griz` class
  through a scripted sequence (rotate, set state, screenshot), asserts the
  PNG is non-empty and the JSON envelopes parse.
- **Regression:** the existing file-based `-b` mode must keep working
  unchanged; a single smoke test covers that path.
- **CI:** runs only the Python unit tests by default; integration tests
  gated on presence of `GRIZ_BIN` and OSMesa.

---

## 8. Risks & Open Questions

1. **Stdout contamination.** Every `printf`/`fprintf(stdout,...)` reachable
   from a batch-mode command path must be rerouted, or it corrupts the JSON
   stream. An audit pass (grep + run-time assertion in server mode that
   rejects non-JSON writes to stdout) mitigates this.
2. **Long-running commands** (e.g. `anim` over many states). Need a
   cancellation story — probably a second write channel or a sentinel
   command. Defer to phase 4; phase 1–3 assume bounded commands.
3. **OSMesa availability** on target hosts. Server mode inherits the
   existing batch-build requirement; document it.
4. **Error semantics in `parse_single_command()`.** Many paths call
   `popup_dialog(USAGE_POPUP, ...)` and return without a status code. We
   need either to surface those messages as the `stderr` field, or to
   thread a return code. Sink indirection (4.4) handles the former
   without API changes.
5. **Concurrency.** Griz is single-threaded, globally stateful. The MCP
   server must serialize. No plan to change this.
6. **Session / aliases.** Griz's `.griz_session` and `alias` system should
   keep working unchanged in server mode; worth a test.

---

## 9. Deliverables

Shared with the UI effort (owned by whichever effort lands first; see the
shared docs for detail):

- **New `griz-server` binary target** with stdio transport
  ([`shared/server-binary.md`](shared/server-binary.md)).
- **Shared JSON envelope and handshake** in `server_core.c`
  ([`shared/command-protocol.md`](shared/command-protocol.md)).
- **`griz_out()` / `griz_err()` sink indirection** and audit of batch-reachable
  `printf` / `fprintf(stdout,…)` sites
  ([`shared/output-capture.md`](shared/output-capture.md)).
- **`q_*` query commands** and the state schema they return
  ([`shared/query-commands.md`](shared/query-commands.md)).
- **`Src/data/results_map.yaml`** as the single source of truth
  ([`shared/results-map.md`](shared/results-map.md)).

MCP-specific:

- **`Src/python/griz/`** — the public `griz` Python package with the
  `Griz` class, namespaced sub-APIs (`field`, `view`, `time`, `materials`),
  a YAML-loader wrapper around `Src/data/results_map.yaml`, unit tests, and
  user-facing README with script examples.
- **`Src/python/griz_mcp/`** — the MCP adapter: `@mcp.tool()` wrappers
  around `Griz`, tests, and an MCP-focused README with example transcripts.
- **Build docs** — `Src/Makefile.Library` update for the `griz-server`
  target; `README.md` pointer to the new Python packages.
- **End-to-end examples** — a Jupyter-style walkthrough using the `griz`
  package directly, and an MCP client transcript driving the same
  database through `griz-mcp`.
