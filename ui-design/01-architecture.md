# 01 — Architecture

## Scope

This document fixes the top-level shape of the Griz client/server split: processes, where they run, how they discover each other, and the invariants the rest of the design must respect. It does not specify the wire protocol (see [02-protocol](02-protocol.md)), the server command surface ([03-server](03-server.md)), or the client UI composition ([04-client](04-client.md)). It is the contract the sibling documents compile against.

## Related

- `UI.md` §§3, 4, 6
- [02-protocol](02-protocol.md), [03-server](03-server.md), [04-client](04-client.md), [05-rendering-and-streaming](05-rendering-and-streaming.md), [06-picking-and-queries](06-picking-and-queries.md), [07-launch-ssh-slurm](07-launch-ssh-slurm.md)

## 1. Component diagram

```
  workstation (Linux / macOS)                  HPC login node                     HPC compute node
 +-------------------------------+        +-----------------------+         +----------------------------+
 |                               |        |                       |         |                            |
 |   griz-client (Qt 6)          |        |   sshd                |         |   griz-server              |
 |   +-----------------------+   |  ssh   |   +---------------+   |  srun   |   +--------------------+   |
 |   | Qt UI thread          |   +------->+   | bash / sbatch +---+-------->+   | cmd thread         |   |
 |   | network thread        |<--tunnel---+                   |   |         |   | render thread (GL) |   |
 |   | decode thread         |   |    |   |   rendezvous file |   |         |   | I/O thread (Mili)  |   |
 |   +-----------------------+   |    |   |   on shared FS    |   |         |   +--------------------+   |
 |                               |    |   |                   |   |         |           |                |
 |   hosts.toml, session state   |    |   +---------------+---+   |         |   OSMesa + fixed-func GL   |
 +-------------------------------+    |                   ^       |         |   Mili FE DB on Lustre/GPFS|
              ^                       |                   |       |         +----------------------------+
              |   TCP inside SSH tunnel (client-initiated, forwarded to compute node host:port)
              +-----------------------+-------------------+-------+-----------------^
                                                                                   |
                                                                    frames / events / pick replies
```

Arrows indicate initiation direction. All network edges cross the SSH tunnel. The rendezvous file on the cluster shared filesystem is the only out-of-band channel and carries only `{host, port, token}` written by the server and read by the client through an `ssh cat` after sbatch submission.

## 2. Process inventory

| Name | Where it runs | Owner | Lifetime | Purpose |
|------|---------------|-------|----------|---------|
| `griz-client` | Workstation | User login session | Interactive | Qt 6 GUI, host/session manager, SSH/SLURM driver, frame decoder, input encoder. |
| `griz-server` | HPC compute node (normally) or login node (interactive variant) or workstation (dev) | User SLURM job or user shell | Bound to session | Derivative of `batchopt`; loads Mili DB, owns OpenGL via OSMesa, exposes `interpret.c` command surface over RPC. |
| `sshd` | HPC login node | Site | Persistent | Auth and tunnel carrier. Not Griz code. |
| `slurmctld` / `slurmd` | HPC | Site | Persistent | Allocation. Not Griz code. |

There is deliberately no separate `griz-launcher` process. Launch logic — `ssh`, `sbatch`, rendezvous read, tunnel setup, server spawn — is a component inside `griz-client` that drives the local `ssh` binary as a child process. Rationale: a separate launcher would need its own IPC, persistence, and crash handling for no win; the only state it would hold (host profiles, active sessions) is already UI state the client owns. A launcher daemon can be reconsidered if and only if multi-window reattach becomes a requirement, which it is not for v1.

`griz-server` is produced by extending the existing `batchopt` build target rather than introducing a new target, to keep the autoconf/gmake build graph untouched for site packagers. The RPC listener is a new translation unit that wraps `interpret.c` and `offscreen.c`; everything else in `Src/` is linked unchanged.

## 3. Naming conventions

Binaries: `griz-client` (installed on workstations), `griz-server` (installed on HPC, alongside the current `griz` and `griz_batch` binaries — do not replace them). The legacy `griz` binary remains the authoritative entry point for batch scripts; `griz-server` is strictly the RPC-speaking variant.

Client config root: `$XDG_CONFIG_HOME/griz` with fallback `~/.config/griz`. Files:

- `~/.config/griz/hosts.toml` — VisIt-style host profiles (hostname, username, SSH options, SLURM bank/partition/walltime defaults, `griz-server` install path, module-load preamble).
- `~/.config/griz/sessions.toml` — last-session reconnect hints (host, job id, rendezvous path).
- `~/.config/griz/ui.toml` — window geometry, recent databases, colormap preferences.

Client cache root: `$XDG_CACHE_HOME/griz` for decoded thumbnails and the known_hosts shim used by the bundled SSH driver.

Session id: `griz-{uuid4-short}` where `uuid4-short` is the first 8 hex chars of a UUIDv4 generated client-side. Collisions are handled by regenerating; the id is not a secret. Used in rendezvous filename, tunnel local port label, and log prefixes on both sides.

Rendezvous file: `$HOME/.griz/rendezvous/{session-id}.json` on the HPC shared filesystem. Created 0600 by the server immediately after bind, deleted by the server on clean shutdown, swept by the client after successful connect. Contents: `{version, session_id, host, port, token, server_pid, started_at}`. The token is a 32-byte base64 secret the client must present on the first RPC; it authenticates the client to the server even though both are the same UID.

Ports: the server binds to `127.0.0.1:0` on the compute node (kernel-assigned ephemeral) and writes the actual port into the rendezvous file. The client forwards an arbitrary local ephemeral to that port through SSH. No fixed port numbers anywhere.

## 4. Data flow

Five message classes cross the tunnel. All travel over the single RPC channel established at session start; none use side channels except the rendezvous bootstrap.

- **Commands.** Origin: client UI (menu actions, console line, hotkey). Path: Qt UI thread enqueues onto network-thread send queue, serialized as a command RPC, dispatched server-side to the command thread, which calls into `interpret.c` exactly as if the text came from the legacy console. Ordering: strict FIFO per session; the server processes one command at a time against viewer state. A command completion event is emitted before the next command is dispatched.
- **Frames.** Origin: server render thread after any state change that dirties the viewport, or on explicit redraw, or on client-driven interaction (drag, zoom). Path: render thread produces a framebuffer via OSMesa, encodes (see [05-rendering-and-streaming](05-rendering-and-streaming.md)), hands to the I/O thread, which writes to the RPC stream. Destination: client decode thread, then Qt UI thread for blit. Ordering: per-frame sequence number; client drops stale frames rather than queueing.
- **State events.** Origin: server, after any command mutates viewer state (camera, time state, visible materials, colormap, selection). Path: command thread posts to the event stream. Destination: client UI thread updates inspector widgets. Ordering: monotonic `state_seq` counter; client reconciles by comparing to its last-seen seq and requests a full snapshot on gap.
- **Picks.** Origin: client, as a pick RPC containing viewport x/y and modifier flags. Path: server render thread performs ID-buffer lookup and, on miss, a ray cast against the current mesh view (see [06-picking-and-queries](06-picking-and-queries.md)). Destination: client, as a pick reply with element id, material, coords, and any queried scalars. Ordering: request/reply pair, out-of-band relative to frames.
- **Metadata queries.** Origin: client (populate material lists, state-variable menus, time-step slider bounds). Path: RPC call served by the command thread reading cached descriptors from `init_io.c`-loaded structures. Ordering: reply is a pure function of current DB; may be concurrent with frames.
- **File/DB ops.** Origin: client (open database, save image, save session). Path: open/close flow through the command thread and re-enter `interpret.c` semantics; image save is served from the render thread's last encoded frame or a re-render at requested resolution. The client never reads the Mili DB directly.

## 5. Deployment topology

Four supported variants. All four speak the same protocol; the client selects the variant via host profile.

- **(A) Local dev, all-in-one.** Both `griz-client` and `griz-server` run on the developer workstation; the server binds to `127.0.0.1` and the client connects without SSH. Purpose: protocol development, UI work, reproducing bugs with known DBs. No SLURM, no tunnel.
- **(B) Interactive login-node session.** `griz-server` runs on the HPC login node, launched by the client over SSH as a plain background process. Intended for quick inspection of small DBs. Site policy permitting; the host profile carries a `forbid_login_node_compute: true` guard for sites that disallow it.
- **(C) SLURM compute-node session (primary).** Client SSHes to the login node, submits an sbatch/srun job that execs `griz-server` on a compute node, then reads the rendezvous file and opens an SSH tunnel whose remote endpoint is the compute node host (via the login node's SSH ProxyJump). This is the default for any profile with a SLURM bank configured.
- **(D) Pre-existing allocation.** User already holds a salloc; the client is given a job id and runs `srun --jobid=…` to start `griz-server` inside the existing allocation. Same rendezvous and tunnel flow as (C) afterward. Useful for debugging sessions where walltime was pre-negotiated.

Process placement summary: `griz-client` is always on the workstation; `griz-server` is on workstation (A), login node (B), or compute node (C, D). The rendezvous file is on the HPC shared filesystem for B/C/D and on `/tmp` for A.

## 6. Threading model per process

`griz-server` runs three threads. The **render thread** exclusively owns the OSMesa context and all OpenGL calls; it pulls work from a single-slot render request queue (latest wins) and pushes encoded frames to the I/O thread. The **command thread** owns `interpret.c` state and the viewer globals; it serializes all command execution and all metadata reads. The **I/O thread** owns the RPC socket, demultiplexes inbound RPCs onto the command queue or pick queue, and multiplexes outbound frames and events. Cross-thread communication is via bounded MPSC queues; the render and command threads never block on the network. Pick requests are handled by the render thread because they require a rendered ID buffer; the command thread passes them through.

`griz-client` runs three threads. The **Qt UI thread** owns all widgets and the event loop and never performs network or decode work. The **network thread** owns the RPC socket, the SSH child process handle, and the reconnect state machine. The **decode thread** pulls frame payloads from the network thread, decodes to RGBA, and posts to the UI thread via a Qt queued connection. Input events are encoded on the UI thread and dropped into the network thread's send queue without blocking.

Key constraint: fixed-function OpenGL as used by `Src/draw.c` is single-threaded and context-bound. Any refactor that would introduce a second GL-touching thread is out of scope.

Non-blocking requirements: the UI thread must never stall on a network read; the render thread must never stall on a socket write; the command thread must never stall on GL. Queue depths and backpressure are specified in [02-protocol](02-protocol.md) and [05-rendering-and-streaming](05-rendering-and-streaming.md).

## 7. Failure domains and recovery

- **Client crash.** Server detects RPC peer close, finishes any in-flight command to a consistent state, then exits after a short grace window. The SLURM job ends; walltime is released. Session is not auto-resumable in v1 (reopen from scratch).
- **Server crash.** Client surfaces the RPC error, captures the tail of the server log via `ssh cat` on the known log path, and offers relaunch in the same host profile. No attempt to reuse the old allocation unless variant D.
- **SLURM walltime kill.** SLURM sends SIGTERM then SIGKILL. The server installs a SIGTERM handler that emits a final `session_ending` event with reason and seconds-until-kill, flushes the current frame, and exits. The client shows a modal and offers relaunch.
- **SSH drop.** Network thread detects tunnel death; UI shows a reconnect banner. The client attempts tunnel re-establishment up to a configurable cap (default 3 over 30 s). The underlying server keeps running so long as the SLURM job lives; on successful reconnect, the client re-authenticates with the rendezvous token and resyncs state by requesting a snapshot.
- **Login node reboot.** Equivalent to SSH drop plus sshd unavailability. Reconnect fails; session is declared lost. If variant C/D, the compute-node server exits when its controlling SSH dies only if `srun` was the parent — which it is by default — so walltime is returned.
- **Compute node failure.** RPC error surfaces as server crash; SLURM marks the job failed. Same flow as server crash.
- **Protocol mismatch.** Handshake at session start exchanges semantic-version ranges. On incompatibility, server refuses with a typed error naming its version; client shows actionable message ("server is X, client needs ≥Y") and does not retry. No silent degradation.

## 8. Invariants

These are enforced by design and must hold across all sibling documents.

- **I1. `interpret.c` is the sole command mutator.** All state changes — camera, time state, material visibility, colormap, selection, view options — route through the text-command grammar in `interpret.c`. New RPCs either emit a command string into the same dispatcher or call a helper that shares its locking. No parallel mutation path is allowed.
- **I2. Selection and camera live server-side.** The client may cache for display but the server is authoritative. A client restart with a surviving server recovers both via snapshot request.
- **I3. State events are monotonic.** Each event carries `state_seq` strictly greater than its predecessor within a session; gaps are repairable only by snapshot request. Clients must not reorder.
- **I4. The client never needs mesh data.** Geometry, topology, and field arrays stay on the server. Picking, queries, and overlays are server-rendered or server-computed. Violating this invariant would defeat the streaming design.
- **I5. The server never initiates connections.** It only binds, writes rendezvous, and accepts. No outbound DNS, no phone-home, no callback sockets. This keeps site security review tractable.
- **I6. OpenGL is single-threaded.** One OSMesa context, one render thread, no exceptions. Any library added to the server must tolerate this.
- **I7. The protocol is negotiated at session start.** Version, feature flags, codec set, and max message sizes are agreed once before the first command. Mid-session renegotiation is not supported in v1.
- **I8. Existing scripts work unchanged.** The legacy `griz` and `griz_batch` binaries, their command files, and their environment expectations remain valid. `griz-server` is additive.

## Open questions

- **Multi-session server?** Should one `griz-server` process host multiple concurrent client sessions against different DBs, or is it strictly one-DB-one-process? One-per-session is simpler and matches SLURM accounting; multi-session would need a fuller session manager and tighter isolation in `interpret.c`'s global state. Recommend one-per-session unless operations teams request otherwise.
- **Offline local-server mode for v1?** Is variant A a supported user-facing mode, or developer-only? If user-facing, the installer must ship `griz-server` for macOS, which changes the macOS build story.
- **Log tail in client?** Should the client surface a live tail of server stderr in a diagnostics pane, or only fetch it on demand after a crash? Live tail costs a second RPC stream and raises questions about log-line rate limiting.
- **Rendezvous on sites without shared home?** A minority of clusters do not expose `$HOME` on compute nodes. Fallback via `ssh cat` on the login node after a server-side `scp` of the rendezvous file is possible but adds latency. Needs site survey.
- **Token rotation on reconnect?** After an SSH drop and reconnect, do we reuse the original rendezvous token or issue a new one via an event? Reuse is simpler; rotation is stricter. No strong reason yet to prefer rotation.
