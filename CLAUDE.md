# Griz

## MCP Implementation

Whenever working on the MCP layer, ensure that you update the ./planning/MCP.md file with our upated status after doing any work.
Keep the MCP.md file tidy.

The top-level checklist lives in `## 0. Implementation Status` at lines 1–84 of `./planning/MCP.md` — read just that range (`Read` with `offset=1, limit=84`) to check/update progress without pulling the full design doc.

## UI Implementation

Planning checklist lives in `## 0. MVP Implementation Tracker` at lines 1–76 of `./planning/UI.md` — read just that range to check/update status. Update it after landing UI work.

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
| `Src/server_rpc.{c,h}` | `bind → rendezvous (0600 JSON) → accept → token hello → dispatch`. Single-threaded v0; heartbeats echoed but not proactively sent. SIGTERM/SIGINT → `session_ending(signal_term)` + socket shutdown. |
| `Src/server_query.{c,h}` | `q_*` builders (state/view/time/materials/results/selection/render/database). |
| `Src/server_events.{c,h}` | `state_changed` emitter with monotonic `state_seq`. |

Server objects linked via `SERVER_OBJS` in `Src/Makefile.Library` — add new TUs there.

### RPC framing (02-protocol.md §2.2)

5-byte header: `uint32 N` (big-endian, payload length, 16 MiB cap) + `kind` (`0x01`=JSON, `0x02`=binary, `0x03`=heartbeat). JSON payload is UTF-8 with no trailing newline.

### Rendezvous file

`$HOME/.griz/rendezvous/<session-id>.json` (mode 0600, atomic rename), contents:
`{version, session_id: "griz-<8 hex>", host, port, token: <32 bytes base64>, server_pid, started_at}`. The client must present `token` in its first hello frame; the server does a constant-time compare. The file is deleted on clean exit.

## Build

## Build

Run `./build.sh` from the repo root to configure and build. Target
selection: `./build.sh batch` builds only `batchopt`, `./build.sh server`
builds only `serveropt`, and `./build.sh` (or `all`) builds both. Pass
alternate configure flags after a `--` separator (e.g.
`./build.sh server -- --with-mili=/some/path`); default configure args
are `--enable-nojpeg --enable-nopng`.

Output binaries:
- `Src/GRIZ4-*/bin_batch_opt/griz4s.linux_opt_batch`
- `Src/GRIZ4-*/bin_server_opt/griz-server`

The default `--enable-nojpeg --enable-nopng` flags compile out the
`outpng` and `outjpeg` commands — they return "Command not valid" at
runtime. To smoke-test image output, rebuild with those flags removed
(e.g. `./build.sh server --`).

Prereqs on LLNL TOSS: Mili at `/usr/apps/mdg`, system OSMesa/X11/Motif in
`/usr/lib64` + `/usr/include/GL`, an Intel or GCC compiler module loaded
(e.g. `intel-classic/2021.6.0-magic`), and `autoconf` on `PATH`.

If `configure.ac` changes, run `autoconf -f` in `Src/` before `build.sh`.

**Non-TOSS sandboxes** (no `/usr/apps/mdg`, Debian/Ubuntu multiarch libs): the
autoconf probe hard-codes `/usr/lib64`, `/usr/lib`, `/usr/X11R6/lib` for the
OSMesa search and the Mili build check requires a real libmili.a with current
symbols — neither resolves on multiarch (`/usr/lib/x86_64-linux-gnu`) or
stub-library setups. A full `batch_opt` / `server_opt` link cannot be produced
in such environments. For structural/syntax validation, compile the RPC TUs in
isolation with minimal header stubs — see the stub pattern used during the
Phase 2 RPC work (not checked in; reconstruct from `Src/server_core.h` if
needed).

## Python

Any python work should use `uv` from Astral.
Call `uv --help` to access the full list of `uv` sub-commands when needed.

If you run into certs issues, read the CERTS.md file for context.

## Session Summaries

Do not write NEW Markdown files at the end of a session unless asked.
Reuse existing markdown files to update status.
