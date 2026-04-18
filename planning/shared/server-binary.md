# Shared — Server Binary

## Scope

Fixes the name, build target, and invocation surface of the new headless Griz process that both the Qt UI client and the MCP Python bridge drive. Defines how a single binary supports two transports.

## Related

- [`../UI.md`](../UI.md) §3.1, §7 (phased roadmap)
- [`../ui-design/01-architecture.md`](../ui-design/01-architecture.md) §§2–3
- [`../MCP.md`](../MCP.md) §4.1, §4.2
- [`command-protocol.md`](command-protocol.md)

## Decision

**One new binary: `griz-server`.** Produced from an extension of the existing `batchopt` build target (OSMesa, no X, no Motif). Selected at runtime between two transports:

```
griz-server --transport=stdio  [griz options...]   # used by MCP worker subprocess
griz-server --transport=rpc    [griz options...]   # used by Qt client over TCP/SSH tunnel
```

`stdio` and `rpc` share the entire C code path above the transport layer: the same dispatcher wraps `parse_command()`, the same output-sink indirection ([`output-capture.md`](output-capture.md)) feeds the response envelope, and the same `q_*` query commands ([`query-commands.md`](query-commands.md)) produce the same state payload.

## Conflict resolution

The two plans originally named this differently:

| Plan | Original | Resolution |
|------|----------|------------|
| `UI.md` / `ui-design/01-architecture.md` | New binary `griz-server`, alongside legacy `griz`/`griz_batch`. | **Keep.** The architecture doc pinned this first; it reads cleanly alongside the existing binaries and is already referenced throughout `ui-design/`. |
| `MCP.md` | `griz4b -server` (add a flag to today's batch binary). | **Change.** Adopt `griz-server --transport=stdio`. MCP.md and its code sketches are updated to match. |

Rationale:

- All the C-side wrappers that MCP needs (command dispatcher, output sink, query commands) are the same ones the UI server needs. Putting them in a separate binary `griz4b` with a flag would either (a) duplicate the C patches or (b) entangle the legacy batch binary with a new RPC layer we don't want it to ship. A new binary cleanly isolates the new code path.
- One new binary keeps the autoconf/gmake graph tidy: `griz-server` is one target, not two, and packagers ship one extra artifact.
- Legacy `griz` and `griz_batch` stay unchanged, preserving UI invariant I8 and the MCP.md §7 regression guarantee.

## Build

- New target `griz-server` in `Src/Makefile.Library`, built from the same object set as `batchopt` plus a new translation unit that provides `main()` for the server (or wraps the existing `main()` behind a `#ifdef SERVER_MODE`).
- The transport layer is two small files: `server_stdio.c` and `server_rpc.c`. They both call into a common `server_core.c` that owns the dispatcher loop, the per-command buffer, and the state-change notifier.
- Dependencies: OSMesa (already a batch requirement). RPC mode additionally links whatever TCP / framing library `02-protocol.md` chooses. stdio mode has no extra deps — it is pure libc.
- Compile-time gating: the whole `griz-server` target is gated on `SERIAL_BATCH`-style configury (renamed to `GRIZ_SERVER` or simply reused). GUI and legacy batch builds are untouched.

## Invocation

Flags accepted by `griz-server`:

| Flag | Meaning | Default |
|------|---------|---------|
| `--transport={stdio,rpc}` | Selects transport. Required. | — |
| `--bind=HOST:PORT` | RPC only. Listen address; `0` means kernel-assigned. | `127.0.0.1:0` |
| `--rendezvous=PATH` | RPC only. Path to write `{host,port,token}` after bind. | off |
| `-i PATH` | Initial Mili database to open. | none |
| `-w WIDTH HEIGHT` | Initial framebuffer size. | `1024 1024` |
| `--protocol-version=N` | Override for testing. | auto |
| `-b`, `-f`, etc. | **Not accepted.** The legacy batch flags belong to the legacy `griz_batch`. |

Examples:

```
# MCP worker spawns:
griz-server --transport=stdio -i runs/blast.plt -w 1024 1024

# Qt client launches on a compute node (via sbatch/srun):
griz-server --transport=rpc --bind=127.0.0.1:0 \
            --rendezvous=$HOME/.griz/rendezvous/griz-$SESSION.json \
            -i runs/blast.plt
```

## Lifecycle

- Both transports follow the same startup ordering: argument parse → open DB (if `-i`) → init OSMesa → init mesh window → emit a single `ready` event (see [`command-protocol.md`](command-protocol.md)) → enter dispatch loop.
- stdio mode exits when stdin closes or a terminator command is received. RPC mode exits when the peer closes the connection or a terminator command is received, after a short grace window for final frame flush.
- SIGTERM is caught in both modes: flush final response / frame, emit `session_ending`, exit.

## Open questions

- **Shared-object vs. static link for the transport halves.** Current plan links both into one binary; a build option to strip RPC (and the protobuf/gRPC dependency) for MCP-only deployments may be useful on hosts where those libs are unavailable. Revisit after [`command-protocol.md`](command-protocol.md) picks a framing library.
- **Do we keep `-b` / `-f` working in `griz-server`?** Currently: no. The legacy `griz_batch` remains the entry point for file-driven batch. Revisit only if there's demand.
