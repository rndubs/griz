# Griz MCP Server — Design & Implementation Plan

## 1. Goal

Expose Griz's visualization capabilities to AI assistants (and any other
MCP-compatible client) via the Model Context Protocol, so a client can drive
Griz with natural-language-mediated tool calls: open a Mili database, change
the view, select a result variable, step through time, and capture rendered
images.

The target architecture mirrors the VisIt MCP bridge already in use:

```
┌──────────────┐    MCP/stdio     ┌──────────────┐     stdio+JSON    ┌──────────┐
│  MCP client  │ ───────────────▶ │  griz-mcp    │ ────────────────▶ │  griz4b  │
│ (Claude etc) │ ◀─────────────── │   server     │ ◀──────────────── │ (server  │
└──────────────┘                  │  (Python)    │                   │  mode)   │
                                  └──────────────┘                   └──────────┘
```

The MCP server is a small Python process that speaks MCP on one side and a
simple JSON-over-stdio protocol to a Griz subprocess on the other. Images are
written to disk by Griz (it already does this) and returned to the client as
`Image` content.

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

## 5. MCP Server (Python)

### 5.1 Layout

```
Src/mcp/
├── griz_mcp_server.py      # MCP entrypoint; registers tools
├── griz_worker.py          # subprocess driver (spawn, read/write, JSON framing)
├── tools.py                # @mcp.tool() definitions (one per Griz capability)
├── pyproject.toml
└── README.md
```

(`Src/mcp/` keeps the code with Griz; alternative is a separate repo.)

### 5.2 Worker / bridge

`GrizWorker` owns the subprocess and serializes calls:

```python
class GrizWorker:
    def __init__(self, griz_bin: str, database: str, width=1024, height=1024):
        self.proc = subprocess.Popen(
            [griz_bin, "-server", "-i", database, "-w", str(width), str(height)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1,
        )
        self._await_event("ready")
        self._lock = asyncio.Lock()

    async def cmd(self, griz_cmd: str, *, timeout=30.0) -> dict:
        async with self._lock:
            self.proc.stdin.write(griz_cmd + "\n"); self.proc.stdin.flush()
            line = await asyncio.wait_for(
                asyncio.to_thread(self.proc.stdout.readline), timeout)
            return json.loads(line)
```

Notes:
- A single lock serializes commands — Griz has global state; pipelining buys
  nothing and invites ordering bugs.
- A watchdog task monitors `self.proc.stderr` for unexpected output and
  surfaces it on the next tool response.
- Shutdown path: send `quit`, `proc.wait(timeout=5)`, then `proc.kill()`.

### 5.3 Tool inventory (initial cut)

Map each Griz command to an MCP tool. One-to-one at first; higher-level
conveniences are easy to layer on later.

**Data / session**
- `load_database(path)` → `load <path>`
- `reload()` → `reload`
- `get_state_info()` → `q_state`
- `list_results()` → `q_results`

**Time / animation**
- `set_state(n)` → `state <n>`
- `set_time(t)` → `time <t>`
- `animate(start?, end?)` → `anim …`

**Result selection**
- `set_result(name)` → `res <name>`
- `show(kind)` → `show <kind>`

**View / camera**
- `rotate(axis, degrees)` → `rx|ry|rz <deg>`
- `translate(axis, distance)` → `tx|ty|tz <dist>`
- `scale(factor)` → `scale <f>` (or `scalax x y z`)
- `zoom(factor)` → `zf <f>` / `zb <f>`
- `reset_view()` → `rview`
- `center_on_node(id)` → `vcent <id>`

**Selection / highlighting**
- `select(class, ids[])` → `select <class> <ids>`
- `unselect(class, ids[])` → `unselect <class> <ids>`
- `highlight(class, id)` → `hilite <class> <id>`
- `clear_picks()` → `cap`

**Display / rendering**
- `toggle(element, on)` → `on|off <element>` (coord, time, cmap, minmax, …)
- `set_render_mode(mode)` → `switch solid|hidden|wf|wft`
- `set_material_visibility(ids, visible)` → material commands

**Output**
- `screenshot(filename?)` → `outpng <tmpfile>`; MCP server returns `Image`.
- `save_text(...)` → `savtxt …` / `endtxt`
- `dump_result(...)` → `dumpresult …`

**Escape hatch**
- `raw_command(cmd)` → passes `cmd` through verbatim. Keeps the server useful
  while we grow the explicit tool surface.

Each tool returns the JSON envelope from Griz so errors surface cleanly.

### 5.4 Image return

`screenshot()` writes to a tempfile in a server-owned directory, then reads
the bytes and returns MCP `ImageContent` (base64 PNG). This matches the
ParaView MCP pattern (`Image(path=img_path)`) and keeps the client protocol
standard.

### 5.5 Configuration

Environment variables / CLI flags for the MCP server:
- `GRIZ_BIN` — path to `griz4b` (batch binary).
- `GRIZ_DEFAULT_WIDTH`, `GRIZ_DEFAULT_HEIGHT`.
- `GRIZ_WORKDIR` — where screenshots and transient files land.

The MCP server does *not* pre-launch Griz. The first `load_database` call
spawns the worker; subsequent calls reuse it. A `restart()` tool tears it
down.

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

### Phase 3 — Tool surface

- Implement the full tool inventory in section 5.3.
- README with example prompts.

### Phase 4 — Polish

- Timeouts, watchdog, clean shutdown.
- `restart()`, `status()` tools.
- Optional: OSMesa-free diagnostic build that returns dummy images, for CI.
- Optional: Unix-socket transport (section 3, option D) for multi-client.

---

## 7. Testing Strategy

- **Unit (Python):** mock `GrizWorker` → exercise every tool's argument
  serialization and response parsing.
- **Integration:** a tiny Mili fixture database checked into
  `Src/mcp/tests/fixtures/`. Test harness spawns real `griz4b -server`,
  loads the DB, runs a scripted sequence (rotate, set state, screenshot),
  asserts the PNG is non-empty and the JSON envelopes parse.
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

- Patches to `Src/viewer.c`, `Src/interpret.c` (and friends) adding
  `-server` mode, JSON framing, output sinks, and `q_*` query commands.
- New directory `Src/mcp/` with the Python MCP server, tool definitions,
  worker, tests, and README.
- Updated `Src/Makefile.Library` / build docs noting the new mode.
- An end-to-end example in `Src/mcp/README.md`: launch the MCP server,
  connect a client, load a sample DB, rotate, screenshot.
