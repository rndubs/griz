# Contributing to Griz

This guide covers getting a working developer setup on LLNL TOSS hosts. For
deeper architecture context see `CLAUDE.md` and the design docs under
`planning/`.

## Prerequisites (TOSS / Lmod)

The repo expects an Lmod-based HPC environment with the LLNL `mdg` apps tree
on the standard path. Load a compiler module before building — the autoconf
probe and the CMake toolchain both pick up whatever `cc`/`c++` resolve to:

```bash
module load intel-classic/2021.6.0-magic   # or another Intel/GCC module
```

A typical login-node module set after this looks like:

```
intel-classic/2021.6.0-magic   mvapich2/2.3.7   StdEnv
```

Other expected paths (no module needed — these are part of the system image):

| Component | Location |
|-----------|----------|
| Mili / Taurus / EPRTF | `/usr/apps/mdg` (headers + libs) |
| OSMesa, GL, GLU, GLw   | `/usr/lib64` + `/usr/include/GL` |
| X11 / Motif            | `/usr/lib64` + `/usr/include` |
| Qt 5.15 headers        | `/usr/include/qt5` |
| `cmake` ≥ 3.16          | `/usr/tce/bin/cmake` (default 3.23.1 is fine; `module load cmake/3.26.3` also works) |
| `autoconf`              | `/usr/bin/autoconf` |

If `Src/configure.ac` has been edited since the last build, run
`autoconf -f` in `Src/` before invoking `./build.sh`.

## Building the server (`griz-server`)

From the repo root:

```bash
./build.sh server
```

Targets:

- `./build.sh batch` — only `batchopt` (`griz4s.linux_opt_batch`).
- `./build.sh server` — only `serveropt` (`griz-server`).
- `./build.sh` / `./build.sh all` — both.

Pass alternate configure flags after a `--` separator, e.g.
`./build.sh server -- --with-mili=/some/path`. libjpeg/libpng are auto-detected
on TOSS, so the inline-screenshot RPC frame works out of the box; pass
`--enable-nojpeg --enable-nopng` after `--` on hosts that lack those libs.

Output:

- `Src/GRIZ4-<arch>/bin_server_opt/griz-server`
- `Src/GRIZ4-<arch>/bin_batch_opt/griz4s.linux_opt_batch` (when building `batch`)

`<arch>` is set by the autoconf probe — on TOSS 4 it looks like
`toss_4_x86_64_ib-<HOSTNAME>`.

`griz-server` speaks two transports selected at launch:

- `--transport=stdio` — newline-delimited JSON for the MCP / pygriz bridge.
- `--transport=rpc`   — length-framed JSON over a loopback TCP socket plus a
  rendezvous-file token handshake, for the Qt UI client.

## Building the Qt client

```bash
./build_client.sh release          # → client/build/linux-release/src/griz-client
./build_client.sh debug            # → client/build/linux-debug/src/griz-client
```

The wrapper drives CMake; pass extra CMake args after `--`. The MVP targets
**Qt 5.15** because TOSS login nodes don't ship Qt 6 — see the Phase 5
preamble in `planning/UI.md` for the Qt 6 port plan.

## Smoke-testing the wiring

Before launching the GUI, verify the spawn → rendezvous → hello handshake:

```bash
cd client/build/linux-release
ctest --output-on-failure
```

The `worker_smoke` test spawns a real `griz-server --transport=rpc`, drives
`hello` → `hello_ack` → `ready`, and round-trips a `q_state`. It auto-discovers
the server binary under `Src/GRIZ4-*/bin_server_opt/` and the `bar71.pltA`
fixture under `Src/test/image/bar71/`. Override either with:

```bash
GRIZ_BIN=/path/to/griz-server GRIZ_TEST_DB=/path/to/foo.pltA ctest --output-on-failure
```

The test skips cleanly (rather than failing) if either artifact is missing —
so a passing-with-skip result means you forgot to build the server.

## Running the UI

```bash
./client/build/linux-release/src/griz-client
```

The client spawns its own `griz-server --transport=rpc` via `QProcess`,
polls `$HOME/.griz/rendezvous/griz-<8hex>.json` (mode 0600), connects over
TCP, and presents the token in its first hello frame. No manual server
launch required. The rendezvous file is removed on clean exit; if a previous
run crashed, clearing `$HOME/.griz/rendezvous/` is harmless.

The UI requires an X display — use VNC or X11 forwarding from the login node.

## Python tooling (`pygriz`, MCP bridge)

Use [`uv`](https://docs.astral.sh/uv/) for all Python work. `uv --help` lists
the sub-commands. If TLS verification fails on LLNL-internal mirrors, see
`CERTS.md` for the corporate CA setup.

### Installing the MCP server (`pygriz_mcp`)

```bash
cd pygriz_mcp
unset SSL_CERT_FILE          # TOSS cert workaround if uv refuses the LLNL mirror
uv sync --extra test
```

This installs `llnl-griz-mcp` (the FastMCP entry point, `griz-mcp`) plus its
dependency `llnl-griz` (editable, from the sibling `pygriz/` directory) and
the `pytest` extras for the smoke tests.

### Two ways to run the MCP layer

`pygriz_mcp` has two operating modes selected by an env var:

| Mode    | Trigger                                      | Behavior |
|---------|----------------------------------------------|----------|
| Spawn   | `GRIZ_MCP_ATTACH_RENDEZVOUS` unset (default) | `griz-mcp` spawns its own `griz-server --transport=stdio`. Each MCP client gets a fresh, isolated session. No Qt UI involved. |
| Attach  | `GRIZ_MCP_ATTACH_RENDEZVOUS` set (any value)  | `griz-mcp` connects to a `griz-server` that the Qt client is already running, as a second RPC client. Both UIs see the same scene; mutations from MCP repaint the Qt viewport in near-real-time via `state_changed` + JPEG auto-push frames. |

Attach-mode resolution (per `pygriz_mcp/src/griz_mcp/session.py`):

1. If the env value names a live rendezvous (file exists and the
   embedded `server_pid` is in `ps`), attach to it.
2. Otherwise scan `$HOME/.griz/rendezvous/ui-*.json` and pick the
   most recently modified live one.
3. If nothing live is found, raise an error pointing at the misuse.

Practical consequences:

- `GRIZ_MCP_ATTACH_RENDEZVOUS=auto` (or any non-existent value) is a
  valid way to opt into attach mode without pinning a specific PID —
  the discovery step in (2) finds whichever Qt client is currently
  running.
- Restarting the Qt UI does **not** require restarting `claude`. The
  next `open_database()` call re-runs the resolver, finds the new
  rendezvous, and reattaches.

In attach mode the LLM's `open_database(path)` call is treated as
"attach + return current state" — the path argument is ignored because
the session is shared with whatever the UI already loaded.

The full demo walk-through (motivation, architecture, troubleshooting) is in
`planning/DEMO.md`.

### Configuring an MCP client

The repo ships a project-scoped **`.mcp.json`** at the root that registers
`griz-mcp` for Claude Code. It uses a relative `./pygriz_mcp` path so it
works for any teammate who clones the repo. To switch between spawn and
attach mode, control the env var in the shell that launches your MCP
client — the variable is inherited by the spawned `griz-mcp` subprocess:

```bash
# Spawn mode (default — no UI)
unset GRIZ_MCP_ATTACH_RENDEZVOUS
claude               # or your IDE plugin

# Attach mode (UI is already running)
export GRIZ_MCP_ATTACH_RENDEZVOUS=$HOME/.griz/rendezvous/ui-<PID>.json
claude
```

For **Claude Desktop**, the equivalent stanza in
`~/.claude/claude_desktop_config.json` is:

```json
{
  "mcpServers": {
    "griz": {
      "command": "uv",
      "args": ["--directory", "/abs/path/to/pygriz_mcp", "run", "griz-mcp"],
      "env": {
        "GRIZ_MCP_ATTACH_RENDEZVOUS": "/home/you/.griz/rendezvous/ui-<PID>.json"
      }
    }
  }
}
```

(Claude Desktop doesn't inherit the launching shell's env on every platform,
so the `env` block is the reliable place to set the rendezvous path.)

### MCP smoke tests

```bash
cd pygriz_mcp
uv run pytest tests/test_smoke.py -v       # stdio path (spawn mode)
uv run pytest tests/test_smoke_rpc.py -v   # RPC envelope-parity gate
```

Both auto-skip if `griz-server` isn't built. The multi-client gate that
proves attach mode works lives one directory over:

```bash
cd pygriz
uv run pytest tests/test_rpc_multi_client.py -v
```

## Non-TOSS sandboxes

The autoconf probe hard-codes `/usr/lib64`, `/usr/lib`, and `/usr/X11R6/lib`
for OSMesa, and the Mili build check requires a real `libmili.a` with current
symbols — neither resolves on Debian/Ubuntu multiarch
(`/usr/lib/x86_64-linux-gnu`) or stub-library setups, so a full `serveropt`
link cannot be produced there.

For structural / syntax validation of the server TUs in such environments:

```bash
./sandbox.sh all
```

This runs install → stub generation → `gcc -fsyntax-only` against every
shipped server TU. Required Debian packages beyond `APT_PACKAGES` are
`libglu1-mesa-dev` and `libgl-dev`. Keep the `FILES=` list in `sandbox.sh`
in sync when you add a new server TU.

## Adding a new server TU

1. Drop the `.c` / `.h` into `Src/`.
2. Add the object to `SERVER_OBJS` in `Src/Makefile.Library` so it links
   into `griz-server`.
3. Append the source filename to the `FILES=` list in `sandbox.sh` so the
   syntax-only check picks it up.
4. If it changes the dispatcher or transport, add or extend a test under
   `pygriz_mcp/tests/` — the stdio/RPC envelope-parity gate
   (`test_smoke.py` ↔ `test_smoke_rpc.py`) catches transport-specific drift.
