# Shared — Output Capture

## Scope

Describes the C-side mechanism that captures Griz's user-facing text output (`popup_dialog`, `wrt_text`, `printf`, `fprintf(stdout,…)` etc.) into per-command buffers when the process is running in `griz-server` mode. Without this, stray `printf` output interleaves with the JSON envelope on stdio and corrupts the protocol framing.

## Related

- [`server-binary.md`](server-binary.md)
- [`command-protocol.md`](command-protocol.md) — defines the `stdout` / `stderr` fields on responses that this buffer feeds.
- [`../MCP.md`](../MCP.md) §4.4 (original proposal, folded in here)

## Problem

Griz today prints user feedback through many helpers, mostly in `Src/interpret.c`, `Src/draw.c`, `Src/results.c`, and GUI paths in `Src/gui.c`:

- `popup_dialog(INFO_POPUP|WARNING_POPUP|USAGE_POPUP, fmt, …)` — shows a Motif dialog in GUI builds; writes to `stderr` in batch builds.
- `wrt_text(fmt, …)` — writes to the console scrollback in GUI builds; writes to `stdout` in batch builds.
- `write_start_text()` / `write_end_text()` — banner macros.
- Direct `printf(…)`, `fprintf(stdout, …)`, and `puts(…)` calls scattered through command handlers.

In legacy batch mode this text goes to the controlling terminal and is harmless. In server mode it goes to the transport and breaks it.

## Implementation (as shipped)

### fd-level `dup2` redirect

Rather than auditing and replacing every `printf`/`fprintf` call site with a new `griz_out()`/`griz_err()` API (the originally planned approach), the implementation uses **fd-level redirection** via `dup2`. This captures *all* writes to stdout/stderr automatically, including those from third-party libraries and Mili.

The key functions live in `Src/server_core.c`:

- **`server_capture_begin()`** (`Src/server_core.c:310–400`) — called before each command dispatch. Saves real stdout/stderr fds via `dup()`, then `dup2`s `tmpfile()` descriptors onto fds 1 and 2. All subsequent writes go to the temp files.

- **`server_capture_end()`** (`Src/server_core.c:402–446`) — called after each command returns. Restores the original stdout/stderr via `dup2`, drains the temp files, and returns the captured strings to the caller for embedding into the response envelope. Cap: **256 KB per stream** (`SERVER_CAPTURE_MAX` at `Src/server_core.c:310`); on overflow the captured text is suffixed with `…[truncated]\n`.

Wired into the dispatch loop at `Src/viewer.c:3254–3277`: `clear → begin → parse_command → end → emit`.

### `popup_dialog` integration

In server mode (`GRIZ_SERVER_BUILD`), `popup_dialog` calls `server_record_error()` (`Src/server_core.c:215–293`) to translate dialog types into structured error responses. Only the **first** diagnostic per command is retained (`Src/server_core.c:256`); subsequent calls are ignored. The command loop calls `server_clear_error()` / `server_peek_error()` around each `parse_command()` to pick this up (`Src/viewer.c:3254, 3265`):

| Dialog type | Error code | Behavior |
|-------------|-----------|----------|
| `USAGE_POPUP` | `invalid_syntax` | Response status set to `error` |
| `WARNING_POPUP` / generic | `command_error` | Response status set to `error` |
| `INFO_POPUP` with error keywords | `unknown_command` | Response status set to `error` |
| `INFO_POPUP` without error keywords | (none) | Treated as informational, no error raised |

### Flush discipline

- The transport layer calls `fflush(stdout)` after every envelope write. Python's `readline()` blocks forever otherwise because stdout is block-buffered when not a TTY.
- The `setvbuf(stdout, NULL, _IONBF, 0)` call at server startup disables buffering entirely on the transport fd.

## Design rationale

The original design (MCP.md §4.4) proposed a source-level `griz_out`/`griz_err` sink indirection with an audit pass to replace all direct writes. The fd-level approach was chosen instead because:

| Approach | Pros | Cons |
|----------|------|------|
| **fd-level `dup2` (chosen)** | Zero source-level changes needed; catches all writes including from Mili, ImageLib, and any future libraries; no ongoing audit burden. | Slightly coarser — can't distinguish "info" from "status" level output. |
| Source-level `griz_out`/`griz_err` | Fine-grained control over output levels; explicit API. | Requires auditing ~5k lines in interpret.c plus results.c, draw.c, etc.; any missed call site leaks into the transport; ongoing maintenance burden as code evolves. |

The fd-level approach was the pragmatic choice: it solves the problem completely with no risk of missed call sites.

## Verified behavior

- `test_stdout_is_captured_into_response`: the `help` command's multi-line output lands in `response.stdout` instead of interleaving with the JSON stream.
- All 30 pygriz integration tests pass with capture enabled.
