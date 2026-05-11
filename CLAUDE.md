# Griz

## MCP Implementation

Whenever working on the MCP layer, ensure that you update the ./planning/MCP.md file with our upated status after doing any work.
Keep the MCP.md file tidy.

The top-level checklist lives in `## 0. Implementation Status` at lines 1–84 of `./planning/MCP.md` — read just that range (`Read` with `offset=1, limit=84`) to check/update progress without pulling the full design doc.

### Typed APIs vs `raw_command`

Plot-label toggles (`title`, `time`, `cmap`, `minmax`, `coord`, `bbox`,
`edges`) have a typed surface — prefer it over the raw escape hatch:

| | Typed | Escape hatch |
|---|---|---|
| pygriz | `g.render.show("title", "time")` | `g.raw("on title time")` |
| MCP | `show_plot_labels(["title", "time"])` / `set_plot_labels(title=True, ...)` | `raw_command("on title time")` |

Same vocabulary in both directions; reads back via `g.render.state()`
or `get_state().render.toggles`. Vocabulary expansion lives in
`planning/render-toggles.md` (R7).

## UI Implementation

Planning checklist lives in `## 0. MVP Implementation Tracker` at lines 1–88 of `./planning/UI.md` — read just that range to check/update status. Update it after landing UI work.

### Qt client (`client/`)

Sibling tree to `Src/`; separate CMake build, no autoconf. MVP targets **Qt 5.15** because TOSS login nodes don't ship Qt 6 — see the Phase 5 preamble in `planning/UI.md` for the Qt 5→6 port plan. Layout: `client/src/{main.cpp,App.*,net/,model/,ui/}` with one static-library CMake target per subtree (`griz-client-net`, `griz-client-model`, `griz-client-ui`) rolled up into the `griz-client` executable.

Build driver: `./build_client.sh [release|debug] [-- <cmake args>]` at the repo root. Requires `cmake >= 3.16` (default TOSS `cmake/3.23.1` works; `module load cmake/3.26.3` is fine) and Qt 5.15 system headers (already present at `/usr/include/qt5`). Output: `client/build/linux-<type>/src/griz-client`.

Tests: `cd client/build/linux-<type> && ctest --output-on-failure`. Current suite: `client/tests/net/test_worker_smoke.cpp` spawns a real `griz-server --transport=rpc`, runs the hello/hello_ack/ready handshake, and round-trips `q_state`. The test auto-discovers the server binary under `Src/GRIZ4-*/bin_server_opt/griz-server` and the `bar71.pltA` sample database under `Src/test/image/bar71/`; override with `GRIZ_BIN` / `GRIZ_TEST_DB` env vars. Skips cleanly if either artifact is missing (`./build.sh server` produces the binary).

Network layer: `client/src/net/` — `Framing` (big-endian 5-byte header + 16 MiB cap per 02-protocol.md §2.2), `Rendezvous` (parses `$HOME/.griz/rendezvous/*.json`), `Worker` (QProcess spawn → rendezvous poll → QTcpSocket → hello-with-token → hello_ack → ready). Worker runs single-threaded on the creating thread's event loop; the three-thread split from 04-client.md §9 is a later `moveToThread()` refinement that keeps this public API.

### Server transports

`griz-server` speaks **two transports** selected by `--transport`:
- `--transport=stdio` — newline-delimited JSON on stdin/stdout (MCP bridge).
- `--transport=rpc` — length-framed JSON over a loopback TCP socket with a rendezvous-file token handshake (Qt UI client).

Both transports share `server_core_dispatch_line()` in `Src/server_core.c`. All responses, events, and hello_acks route through a pluggable emitter (`server_set_line_emitter`) so the dispatcher is transport-neutral. Stdio uses the built-in stdout+newline emitter; RPC installs a framed-socket emitter in `Src/server_rpc.c`.

### RPC server TU layout

| File | Role |
|------|------|
| `Src/server_main.c` | flag parser, transport select, argv for `--bind`, `--port`, `--rendezvous`. |
| `Src/server_core.{c,h}` | JSON envelope, output capture, error recording, emitter hook, `server_core_dispatch_line`. |
| `Src/server_core_startup.{c,h}` | Analysis / OSMesa / DB open — shared by both transports. |
| `Src/server_stdio.c` | Thin `fgets` loop → `server_core_dispatch_line`. |
| `Src/server_rpc.{c,h}` | `bind → rendezvous (0600 JSON) → accept (multi-client up to RPC_MAX_CLIENTS=4) → token hello per client → dispatch`. Single-threaded v0. Response/hello_ack emits route to `current_client_idx`; events and auto-push binary frames broadcast to every authenticated client. Dispatch loop drives per-client 20 s heartbeat cadence + 60 s peer-idle detection via `poll()` over listen_fd + all client fds; peer-idle closes only that client slot. SIGTERM/SIGINT → broadcast `session_ending(signal_term)` + socket shutdown on every slot. |
| `Src/server_query.{c,h}` | `q_*` builders (state/view/time/materials/results/selection/render/database). |
| `Src/server_events.{c,h}` | `state_changed` emitter with monotonic `state_seq`. |

Server objects linked via `SERVER_OBJS` in `Src/Makefile.Library` — add new TUs there.

### RPC framing (02-protocol.md §2.2)

5-byte header: `uint32 N` (big-endian, payload length, 16 MiB cap) + `kind` (`0x01`=JSON, `0x02`=binary, `0x03`=heartbeat). JSON payload is UTF-8 with no trailing newline.

### Rendezvous file

`$HOME/.griz/rendezvous/<session-id>.json` (mode 0600, atomic rename), contents:
`{version, session_id: "griz-<8 hex>", host, port, token: <32 bytes base64>, server_pid, started_at}`. The client must present `token` in its first hello frame; the server does a constant-time compare. The file is deleted on clean exit.

### RPC Python client

`pygriz/src/griz/rpc_worker.py` is the reference client for
`--transport=rpc`. Public API is drop-in compatible with
`griz.worker.Worker` (same `cmd()` / `cleanup()` / `server_info` /
`send_command()`), so `Griz(worker_factory=RpcWorker)` works without
any changes in `griz.session`. Spawn flow: launch the server with
`--rendezvous=<tempfile>`, poll for the file, TCP connect, send
`hello` carrying the token, then consume `hello_ack` + `ready` before
returning.

### RPC envelope-parity gate

`pygriz_mcp/tests/test_smoke_rpc.py` mirrors `test_smoke.py` case-for-
case but swaps the session factory to use `RpcWorker`. Any test that
passes on stdio and fails on RPC (or vice versa) is a sign that a
transport has grown envelope-specific behavior — fix that in the
shared core, not in one transport.

## Build

Run `./build.sh` from the repo root to configure and build. Target
selection: `./build.sh batch` builds only `batchopt`, `./build.sh server`
builds only `serveropt`, and `./build.sh` (or `all`) builds both. Pass
alternate configure flags after a `--` separator (e.g.
`./build.sh server -- --with-mili=/some/path`). No configure flags are
passed by default — libjpeg and libpng are auto-detected, so `outpng`,
`outjpeg`, and the RPC inline-screenshot binary frame (`kind=0x02`
subtype `0x02` codec `0x02`) all work out of the box on TOSS. On hosts
that lack libjpeg/libpng, pass `--enable-nojpeg --enable-nopng` after
`--` to compile them out (the inline-screenshot endpoint will fail at
runtime in that configuration).

Output binaries:
- `Src/GRIZ4-*/bin_batch_opt/griz4s.linux_opt_batch`
- `Src/GRIZ4-*/bin_server_opt/griz-server`

Prereqs on LLNL TOSS: Mili at `/usr/apps/mdg`, system OSMesa/X11/Motif in
`/usr/lib64` + `/usr/include/GL`, an Intel or GCC compiler module loaded
(e.g. `intel-classic/2021.6.0-magic`), and `autoconf` on `PATH`.

If `configure.ac` changes, run `autoconf -f` in `Src/` before `build.sh`.

**Non-TOSS sandboxes** (no `/usr/apps/mdg`, Debian/Ubuntu multiarch libs): the
autoconf probe hard-codes `/usr/lib64`, `/usr/lib`, `/usr/X11R6/lib` for the
OSMesa search and the Mili build check requires a real libmili.a with current
symbols — neither resolves on multiarch (`/usr/lib/x86_64-linux-gnu`) or
stub-library setups. A full `batch_opt` / `server_opt` link cannot be produced
in such environments.

For **structural/syntax validation** of the server TUs, run
`./sandbox.sh all` (install → stubs → `gcc -fsyntax-only`). The stubs
step writes Mili/GAHL/griz_config stubs plus a `GL/` subdir that
re-exports the vendored Mesa headers (`Src/ext/Mesa/include/*.h`) at
the `<GL/xxx.h>` paths Griz expects. The check step runs
`gcc -fsyntax-only` against every shipped server TU, so regressions
in includes/typing land before CI. Required Debian packages beyond
those in `APT_PACKAGES` are `libglu1-mesa-dev` + `libgl-dev` (pulls
in `/usr/include/GL/glu.h`). Keep the `FILES=` list in `sandbox.sh`
in sync when you add a new server TU.

## Python

Any python work should use `uv` from Astral.
Call `uv --help` to access the full list of `uv` sub-commands when needed.

If you run into certs issues, read the CERTS.md file for context.

## Session Summaries

Do not write NEW Markdown files at the end of a session unless asked.
Reuse existing markdown files to update status.
