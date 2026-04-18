# Griz MCP Server — Design & Implementation Plan

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
              ┌────────────┐
              │  griz4b    │  Existing batch binary + a new -server
              │  -server   │  mode (small C addition).
              └────────────┘
```

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

### 4.1 New invocation mode: `-server`

Add a flag to `scan_args()` in `Src/viewer.c` that selects a new
`process_server_mode()` function instead of `process_serial_batch_mode()`.
Only built when `SERIAL_BATCH` is defined (i.e. in the batch/OSMesa build).

The server-mode build artifact is `griz4b` (the existing batch binary) invoked
as `griz4b -server -i <db>`.

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

`server_dispatch()` wraps `parse_command()` and emits one JSON object per
command. Suggested schema (line-delimited JSON, a.k.a. JSONL):

```json
{"id": "<echo>", "status": "ok",    "stdout": "...", "stderr": "..."}
{"id": "<echo>", "status": "error", "message": "unknown command: foo"}
```

Input is either a raw Griz command string (simplest) **or** a JSON object:

```json
{"id": "42", "cmd": "rx 30"}
```

Both forms are accepted. The `id` is optional and is echoed back so the MCP
server can correlate requests and responses (useful once we allow pipelining).

### 4.4 Capturing Griz's text output

Griz today prints user-facing feedback through `popup_dialog()`,
`wrt_text()`, `write_start_text()` and similar helpers. For server mode we
need to intercept that output so it can be packed into the JSON response
instead of interleaving with the protocol.

Two workable approaches; recommend the first:

1. **Add a thin output sink indirection.** In the existing helpers, when
   `env.server_mode` is true, append to a per-command buffer instead of
   writing directly to `stdout`. `server_dispatch()` flushes that buffer into
   the JSON `stdout`/`stderr` fields. This is localized and avoids breaking
   GUI/batch behavior.
2. Redirect C `stdout` to a pipe at startup and drain it after each command.
   Simpler code, but fragile across libraries that cache `FILE*`.

Concretely, a scan for every `fprintf(stdout, …)`, `printf(…)`, `puts(…)` in
`Src/interpret.c`, `Src/draw.c`, `Src/results.c`, and `Src/gui.c` (batch
paths only) is needed; route them through a new `griz_out()` /
`griz_err()` pair that checks `env.server_mode`.

### 4.5 Structured query commands (optional, phase 2+)

A small set of new commands expose state that today is only readable by a
human:

- `q_state` — current state index, time, min/max state indices.
- `q_results` — list of available result variables.
- `q_selection` — currently selected/highlighted objects.
- `q_view` — rotation, translation, scale, zoom.
- `q_materials` — material IDs, visibility, enable flags.

Each writes a single JSON object to the per-command buffer so it flows back
in the response envelope. These are additive commands — no risk to existing
users.

### 4.6 Image capture

No new rendering code needed. The MCP server calls the existing `outpng
<path>` command. Because the `-server` binary is the batch build, rendering
goes through OSMesa and does not require an X display. The MCP server reads
the resulting file from disk and returns it as MCP `Image` content.

### 4.7 Build system

- Add the new source (or the new function in `viewer.c`) to
  `Src/Makefile.Library`'s batch build.
- No new library deps.
- Configure-time check not needed; the feature piggybacks on `SERIAL_BATCH`.

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
            [griz_bin, "-server", "-i", database, "-w", str(width), str(height)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1,
        )
        self._await_event("ready")
        self._lock = threading.Lock()

    def cmd(self, griz_cmd: str, *, timeout: float = 30.0) -> dict:
        with self._lock:
            self.proc.stdin.write(griz_cmd + "\n"); self.proc.stdin.flush()
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

Griz encodes result names tersely (`sx`, `sy`, `exx`, `temp`, …). The Python
layer hides this. A hand-curated table in `griz/results_map.py` owns the
translation:

```python
# griz/results_map.py
RESULTS = {
    "stress": {
        "xx": "sx", "yy": "sy", "zz": "sz",
        "xy": "sxy", "yz": "syz", "zx": "szx",
        "von_mises": "seff",
        "pressure":  "spres",
    },
    "strain": {
        "xx": "exx", "yy": "eyy", "zz": "ezz",
        "xy": "exy", "yz": "eyz", "zx": "ezx",
    },
    "temperature": {None: "temp"},
    "displacement": {
        "x": "ux", "y": "uy", "z": "uz", "magnitude": "umag",
    },
    # ... extended as needed
}
```

`g.field.show("stress", component="xx")` resolves via this table to
`res sx; show result`. Unknown `(field, component)` pairs raise
`UnknownFieldError` with a suggestion list from the table. For names the
table doesn't know, users can fall through with `g.raw("res <name>")`.

This table is also the source of documentation — the docs page is
generated from it, so "what components are valid" and "what Griz command
do they map to" are in exactly one place.

Griz's result vocabulary is stable, so this is hand-maintained; no runtime
metadata query needed.

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
- `GRIZ_BIN` — path to `griz4b` (batch binary). Default: `griz4b` on PATH.
- `GRIZ_DEFAULT_WIDTH`, `GRIZ_DEFAULT_HEIGHT`.
- `GRIZ_WORKDIR` — where screenshots and transient files land.

The MCP server does *not* pre-launch Griz. The first `open_database` tool
call spawns the worker; subsequent calls reuse it. A `restart` tool tears
it down.

---

## 6. Phased Implementation

### Phase 1 — Smoke test, no JSON (1–2 days)

- Add `-server` flag; implement `process_server_mode()` reading plain
  newline-delimited commands from stdin.
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
  `griz4b -server`, drives it with the real `Griz` class through a
  scripted sequence (rotate, set state, screenshot), asserts the PNG is
  non-empty and the JSON envelopes parse.
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

- **C patches** to `Src/viewer.c`, `Src/interpret.c` (and friends): `-server`
  mode, JSON line framing, output sink indirection, and `q_*` query
  commands. Gated on `SERIAL_BATCH` so GUI and legacy batch builds are
  unaffected.
- **`Src/python/griz/`** — the public `griz` Python package with the
  `Griz` class, namespaced sub-APIs (`field`, `view`, `time`, `materials`),
  the hand-curated results mapping, unit tests, and user-facing README
  with script examples.
- **`Src/python/griz_mcp/`** — the MCP adapter: `@mcp.tool()` wrappers
  around `Griz`, tests, and an MCP-focused README with example transcripts.
- **Build docs** — `Src/Makefile.Library` update noting server mode;
  `README.md` pointer to the new Python packages.
- **End-to-end examples** — a Jupyter-style walkthrough using the `griz`
  package directly, and an MCP client transcript driving the same
  database through `griz-mcp`.
