# Shared — Output Capture

## Scope

Describes the C-side indirection that reroutes Griz's user-facing text output (`popup_dialog`, `wrt_text`, `write_start_text`, `printf`, `fprintf(stdout,…)` etc.) into per-command buffers when the process is running in `griz-server` mode. Both transports need this: without it, stray `printf` output interleaves with the JSON envelope on stdio and corrupts the RPC framing on TCP.

## Related

- [`server-binary.md`](server-binary.md)
- [`command-protocol.md`](command-protocol.md) — defines the `stdout` / `stderr` fields on responses that this buffer feeds.
- [`../MCP.md`](../MCP.md) §4.4 (original proposal, folded in here)
- [`../ui-design/03-server.md`](../ui-design/03-server.md) (stub; will reference this doc)

## Problem

Griz today prints user feedback through many helpers, mostly in `Src/interpret.c`, `Src/draw.c`, `Src/results.c`, and GUI paths in `Src/gui.c`:

- `popup_dialog(INFO_POPUP|WARNING_POPUP|USAGE_POPUP, fmt, …)` — shows a Motif dialog in GUI builds; writes to `stderr` in batch builds.
- `wrt_text(fmt, …)` — writes to the console scrollback in GUI builds; writes to `stdout` in batch builds.
- `write_start_text()` / `write_end_text()` — banner macros.
- Direct `printf(…)`, `fprintf(stdout, …)`, and `puts(…)` calls scattered through command handlers, particularly around debugging output and status messages.

In legacy batch mode this text goes to the controlling terminal and is harmless. In server mode it goes to the transport and breaks it.

## Design

### 1. A single sink API

Introduce two functions that all user-facing text must pass through:

```c
/* src/output.c */
void griz_out(const char *fmt, ...);   /* informational / status  → response.stdout */
void griz_err(const char *fmt, ...);   /* warning / usage / error → response.stderr */
```

Behavior depends on a single flag in `env`:

- **GUI build (`env.server_mode == 0`, interactive GUI running):** forwards to the existing Motif console helpers. Unchanged user-visible behavior.
- **Legacy batch (`env.server_mode == 0`, `-b` / `-f`):** writes directly to `stdout` / `stderr` exactly as today.
- **Server (`env.server_mode == 1`):** appends to the per-command `stdout_buf` / `stderr_buf` owned by `server_core.c`. The dispatcher drains these buffers into the response envelope before releasing the command slot.

Per-command buffers are thread-local on the command thread. The render thread and I/O thread do not write through `griz_out` / `griz_err`; anything they need to surface is a structured `render_error` response or a log line on the server-side log file.

### 2. Redirect existing helpers

`popup_dialog(kind, fmt, …)` in `Src/misc.c` (or wherever it currently lives in batch builds) is updated so its batch / server path funnels through `griz_err` for `WARNING_POPUP` / `USAGE_POPUP` / `ERROR_POPUP` and `griz_out` for `INFO_POPUP`. GUI path is unchanged.

`wrt_text`, `write_start_text`, `write_end_text` similarly funnel through `griz_out`.

### 3. Audit and repair direct writes

An audit pass across the tree finds every direct `printf(…)`, `fprintf(stdout,…)`, `fputs(…, stdout)`, `puts(…)` and replaces it with `griz_out(…)` in batch-reachable paths. Same for `stderr` variants → `griz_err(…)`.

Scope of the audit:

- `Src/interpret.c` — known dense in direct writes (~5k-line dispatcher).
- `Src/results.c`, `Src/draw.c` — moderate density.
- `Src/gui.c` — GUI-only; skip unless a call is reachable from a batch path.
- Third-party libs and Mili FE — out of scope; if they print, we accept that for now and note it as a residual leak.

### 4. Guard rail: stdout assertion in server mode

In server mode, after the command dispatcher has drained the per-command buffers, it reads from a non-blocking duplicate of `stdout` (via `dup2` at startup onto a pipe) to detect any rogue writes that bypassed the sink. Any captured bytes are:

- In dev builds: logged with the command id and a stack tag, response marked `internal_error` with code `stdout_leak`.
- In release builds: logged, appended to `stderr_buf`, response still returned `ok` so users aren't blocked by a cosmetic issue.

This is a development aid; the goal is zero leaks by the time we ship.

### 5. Flush discipline

- Both transports require `fflush()` after every envelope write. stdio especially: Python's `readline()` blocks forever otherwise because stdout is block-buffered when not a TTY.
- The sink itself does not flush. The dispatcher flushes the transport after each response / event.

## Conflict resolution

`MCP.md` §4.4 proposed this as an MCP-only change ("thin output sink indirection"). It is promoted here to a shared component because:

- The UI RPC transport has the same corruption risk. If a random `printf` lands in the middle of an RPC frame, the client disconnects.
- Implementing it twice (once for stdio, once for RPC) guarantees drift.

The two options MCP.md considered:

| Option | Verdict |
|--------|---------|
| Sink indirection through `griz_out` / `griz_err`. | **Chosen.** Localized, survives library additions, works identically for both transports. |
| Pipe redirection of C `stdout`. | Rejected. Fragile across libraries that cache `FILE*`; doesn't help for `stderr` corruption. Kept only as the guard-rail mechanism described in §4 above. |

## Open questions

- **Should `griz_out` carry a level** (debug / info / status)? Current plan: no — `stdout` vs. `stderr` is enough. A level system can be added later with a structured `log` event if needed.
- **What about `wrt_text`-style partial lines?** Griz occasionally writes a line in pieces (`"Loading "`, then `"database..."`, then `"\n"`). The sink concatenates; this is fine for the buffer-per-command model as long as no command relies on mid-line flushing to drive a TTY progress indicator. Audit should flag any such use.
- **Can the Motif console in the legacy GUI also consume `griz_out`?** Cleanup opportunity, not a requirement.
