# Demo: LLM drives MCP, Qt UI shows the result

## Running the demo (for colleagues)

**One-time setup on an LLNL TOSS host with Mili + OSMesa** (a compiler
module loaded, e.g. `intel-classic/2021.6.0-magic`):

```bash
# From the repo root — builds griz-server (headless) and the Qt client.
./build.sh server
./build_client.sh release

# Python side: MCP + Griz packages.
cd pygriz_mcp
unset SSL_CERT_FILE            # TOSS cert workaround
uv sync --extra test
cd ..
```

**Live run — three terminals:**

1. **Terminal 1 — launch the Qt UI.** It spawns `griz-server` and owns
   its lifetime; the server's rendezvous file is written to a stable,
   guessable path so MCP can find it.

   ```bash
   client/build/linux-release/src/griz-client --db Src/test/image/bar71/bar71.pltA
   ```

   Look at the console pane inside the Qt window (or this terminal's
   stderr). Near the top you'll see two lines:

   ```
   [rendezvous] /home/you/.griz/rendezvous/ui-123456.json
   [attach] export GRIZ_MCP_ATTACH_RENDEZVOUS=/home/you/.griz/rendezvous/ui-123456.json
   ```

   Copy that `export …` line.

2. **Terminal 2 — start the MCP server in attach mode.** Paste the
   `export` line, then launch `griz-mcp` the way your MCP client (Claude
   Desktop, Claude Code, etc.) would. For a direct smoke check:

   ```bash
   export GRIZ_MCP_ATTACH_RENDEZVOUS=/home/you/.griz/rendezvous/ui-123456.json
   cd pygriz_mcp
   uv run griz-mcp              # stdio MCP server
   ```

   For an actual LLM session, add this to your MCP client's config
   (e.g. `~/.claude/claude_desktop_config.json` or
   `.claude/settings.json` under `mcpServers`):

   ```json
   {
     "mcpServers": {
       "griz": {
         "command": "uv",
         "args": ["--directory", "/abs/path/to/pygriz_mcp", "run", "griz-mcp"],
         "env": {
           "GRIZ_MCP_ATTACH_RENDEZVOUS": "/home/you/.griz/rendezvous/ui-123456.json"
         }
       }
     }
   }
   ```

3. **Terminal 3 (or the LLM chat) — drive it.** The LLM still calls
   `open_database(path)` as its entry point, but in attach mode the
   `path` argument is ignored — MCP attaches to whatever the UI already
   has open and returns the current `q_state`. Then:

   ```
   open_database("anything")        # path ignored; returns live state
   rotate_view(y=45)                # UI viewport repaints
   set_time_state(10)
   show_field("stress", "von_mises")
   screenshot()
   ```

   Each mutating call broadcasts a `state_changed` event plus an
   auto-push JPEG frame to every connected client, so the Qt viewport
   repaints in near-real-time as the LLM drives the scene.

**Teardown:** close the Qt window (or Ctrl-C in Terminal 1). The
server shuts down and broadcasts `session_ending` to MCP; the MCP
server exits cleanly on its next tool call. MCP detaching on its own
(e.g. Claude Desktop restarted) does **not** kill the server — the Qt
UI keeps running.

**Troubleshooting:**

- *"rendezvous file did not appear"* in MCP → the Qt client hasn't
  finished spawning yet, or you're pointing at a stale `ui-<pid>.json`
  from a previous run. Grab a fresh path from the current Qt console.
- *"protocol_mismatch: invalid rendezvous token"* → two different
  `griz-server` instances are using the same path. Only one UI at a
  time per rendezvous file.
- *All 4 slots in use* → bump `RPC_MAX_CLIENTS` in `Src/server_rpc.c`;
  the MVP cap is 4 concurrent clients.

---

## Goal

An LLM agent drives `griz-mcp` and the Qt UI user sees the mesh update in
real time. Today each front-end spawns its own `griz-server` process
(stdio for MCP, rpc for the UI) and the two sessions have no shared
state. This plan closes the loop with a single shared server.

```
┌───────────┐                           ┌───────────┐
│ Qt client │ ─── RPC (kind=0x01) ───▶  │ griz-     │
└───────────┘ ◀── events / frames ───   │ server    │
                                        │ (rpc,     │
┌───────────┐ ─── RPC (kind=0x01) ───▶  │ 2 clients │
│ griz-mcp  │ ◀── events / frames ───   │ sharing   │
└───────────┘                           │ 1 scene)  │
     ▲                                  └───────────┘
     │
   LLM / agent
```

The Qt client owns the server lifetime (it spawned it, it reaps it on
quit). `griz-mcp` attaches as a second client by reading the same
rendezvous file and completing the existing token handshake.

## Why "option 3" (attach) over the alternatives

- **Option 1 (one server, two transports in one process)** would need
  `server_main.c` to run stdio and rpc loops concurrently against the
  shared dispatcher. Doable, but it bloats the non-demo path and forces
  users to remember to launch with both flags.
- **Option 2 (UI as MCP proxy)** would turn the Qt client into a
  second-level IPC hub. Heavier C++ surface area and a weird coupling.
- **Option 3 (MCP attaches as a second RPC client)** keeps each
  front-end simple. The only architectural change is relaxing the
  server's "single connection per lifetime" invariant — which the
  design doc already calls a v0 simplification (see
  `planning/ui-design/02-protocol.md §7`).

## Scope of changes

Four touch points, roughly in order of risk:

| # | Area                          | Size   | Risk |
|---|-------------------------------|--------|------|
| A | Qt client rendezvous path     | tiny   | low  |
| B | `RpcWorker` attach mode       | small  | low  |
| C | MCP session attach plumbing   | small  | low  |
| D | Multi-client `server_rpc.c`   | medium | med  |

A + B + C are independently useful (an attach-capable `RpcWorker` is a
nice debugging hook even without multi-client on the server). D is the
load-bearing change that makes the demo work.

### A. Qt client: stable rendezvous path

Today `client/src/net/Worker.cpp` writes its rendezvous to a
`QTemporaryDir` local to the client process. MCP can't find it.

Change: pass `--rendezvous=$HOME/.griz/rendezvous/ui-<pid>.json` to the
spawned `griz-server`, cleaned up on `shutdown()`. Log the path at
startup (stderr + status bar tooltip) so the user can copy-paste it
into MCP config. Falls back to the old temp-dir behavior when
`$HOME/.griz/rendezvous/` isn't writable.

### B. `RpcWorker` attach mode

Add an `attach_rendezvous: str | os.PathLike | None = None` kwarg to
`RpcWorker.__init__`. When set:

- skip subprocess spawn, skip database-path existence check
- read the rendezvous JSON directly
- connect + handshake as today, but identify as `client:
  "griz-python-rpc-attach"` so server logs distinguish it
- `cleanup()` closes the socket **only** — no process kill, no
  rendezvous unlink (we don't own those)

Existing spawn-mode callers stay untouched; this is purely additive.

### C. MCP session attach plumbing

Env var `GRIZ_MCP_ATTACH_RENDEZVOUS`. When set, `session.open_database`:

- uses `RpcWorker(attach_rendezvous=…)` via a factory instead of the
  default `Worker` (stdio)
- ignores the `path` arg (the session is shared — the UI already
  opened the database); returns the current `q_state` so the LLM sees
  what's loaded

No new MCP tools for the MVP demo; the LLM still calls
`open_database(path)` as the entry point, but in attach mode we treat
that as "attach and return current state" regardless of `path`.

### D. Multi-client `server_rpc.c`

The big one. Today the server `accept()`s exactly one client, closes
the listen socket, and runs a single `poll()` loop over that one fd.

Changes to `Src/server_rpc.c`:

1. Keep the listen socket open after first accept (cap at e.g.
   `MAX_CLIENTS = 4`).
2. Replace the single `g_rpc.fd` with an array of client slots, each
   with its own `last_inbound_ms` / `last_outbound_ms` / `token_ok`.
3. `poll()` over listen_fd + every connected client_fd.
4. New connection on listen_fd → add slot, require hello+token within
   the idle window, reject cleanly on auth failure (close that fd, keep
   serving the others).
5. Dispatch: `server_core_dispatch_line()` is already transport-neutral
   and synchronous. Record a "current client" pointer in a thread-local
   before calling it, so the emitter knows where to send the *response*.
   Events (`state_changed`, `session_ending`) and binary auto-push
   frames broadcast to **all** connected clients.
6. Heartbeats: per-client cadence (each client tracks its own
   outbound-idle), but the interval check fires inside the single
   dispatch loop.
7. Idle detection is per-client: only that client's socket is torn down
   on peer-idle; the others stay connected.
8. `session_ending(reason="signal_term")` on SIGTERM still broadcasts
   and exits — a signal to the process applies to every client.
9. Last-client-disconnect policy: keep the server running; only SIGTERM
   / SIGINT (or the Qt client's `quit` command) brings it down.
10. Rendezvous file: unchanged. Both clients present the same token.
    Constant-time compare still applies per client.

The emitter glue (`server_core.{c,h}`'s `rpc_line_emitter` /
`rpc_binary_emitter`) needs one small extension: a "broadcast vs
response" hint. Today the emitter writes to `g_rpc.fd`; it becomes a
loop over connected clients, skipping the ones that aren't the current
responder when the hint says "response".

### Tests

- **RpcWorker attach unit test** (`pygriz/tests/`): spawn a server
  externally, attach via `RpcWorker(attach_rendezvous=…)`, run
  `q_state`, tear down the attach without killing the server.
- **Two-client smoke test** (`pygriz/tests/test_rpc_multi_client.py`):
  spawn the server, attach client A, attach client B, have A mutate
  state (`rx 10`), assert B receives a `state_changed` event with the
  same `state_seq` and a new auto-push binary frame.
- **MCP attach integration** (`pygriz_mcp/tests/`): run `griz-mcp` with
  `GRIZ_MCP_ATTACH_RENDEZVOUS` pointing at a live server, assert the
  MCP tools observe the server's state.

### Manual demo script

1. Build: `./build.sh server && ./build_client.sh release`
2. Launch UI: `client/build/linux-release/src/griz-client
   Src/test/image/bar71/bar71.pltA`
   → note the rendezvous path in the status bar / stderr
3. Point MCP at it: `export
   GRIZ_MCP_ATTACH_RENDEZVOUS=$HOME/.griz/rendezvous/ui-<pid>.json`
4. Start MCP (standalone for debugging, or via Claude Desktop config)
5. Drive from the LLM: `show_field("stress", "von_mises")`,
   `rotate_view(y=45)`, `set_time_state(10)` — watch the UI viewport
   repaint after each.

## Non-goals

- Remote / SSH / SLURM launch — out of scope for this demo. Variant A
  (localhost) only.
- Hardening beyond 2 simultaneous clients. `MAX_CLIENTS=4` is a safety
  cap; we don't stress-test it.
- Making MCP drive the UI's dock widgets directly — the UI consumes
  `state_changed` events and its own `q_state` refresh; MCP just
  mutates server state.
- Rewriting stdio-mode MCP. That path stays as-is for users who don't
  want to run the UI.

## Rollout order

1. **A** (Qt rendezvous path) — trivial, unblocks manual testing.
2. **B** (`RpcWorker` attach) — purely additive, has its own test.
3. **D** (multi-client server) — gated behind `D.1` (keep listen_fd
   open) → incrementally validated with the 2-client smoke test.
4. **C** (MCP plumbing) — last, since it needs A/B/D to land.
5. End-to-end demo per the manual script above.
