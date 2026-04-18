# Shared — Command Protocol

## Scope

Defines the message envelope, event catalog, handshake, version negotiation, and error shape used by **both** transports of `griz-server`. The stdio transport (used by MCP) newline-delimits JSON objects. The RPC transport (used by the Qt UI client) length-prefixes the same objects — framing details live in [`../ui-design/02-protocol.md`](../ui-design/02-protocol.md).

The rule: every transport-carried message has the same JSON shape. Only the framing differs.

## Related

- [`server-binary.md`](server-binary.md)
- [`output-capture.md`](output-capture.md)
- [`query-commands.md`](query-commands.md)
- [`../ui-design/02-protocol.md`](../ui-design/02-protocol.md) — RPC-specific framing, flow control, compression
- [`../MCP.md`](../MCP.md) §4.3 — stdio-specific examples

## Message types

Five message kinds cross the wire. Every message is a single JSON object with a type discriminator.

### 1. Request (client → server)

```json
{
  "type": "request",
  "id": "42",
  "cmd": "rx 30"
}
```

- `id` (string, optional): echoed back on the matching response. MCP stdio usage may omit it; RPC usage should supply it for correlation under pipelining.
- `cmd` (string, required): a Griz command, the exact text that would otherwise be typed at the legacy console. Semicolon-separated compound commands are allowed — the server returns one response covering the whole compound, matching the existing `parse_command()` semantics.

For stdio convenience, a bare non-JSON line is also accepted and is interpreted as `{"type":"request","cmd":"<line>"}`. This keeps interactive debugging (`echo 'rx 30' | griz-server --transport=stdio`) trivial. RPC does not accept this shortcut.

### 2. Response (server → client)

```json
{
  "type": "response",
  "id": "42",
  "status": "ok",
  "stdout": "rotated 30 degrees about X\n",
  "stderr": "",
  "data": null
}
```

Error form:

```json
{
  "type": "response",
  "id": "42",
  "status": "error",
  "error": {
    "code": "unknown_command",
    "message": "unknown command: fo",
    "detail": null
  },
  "stdout": "",
  "stderr": "Usage: rx <angle>\n"
}
```

Fields:

- `status`: `"ok"` or `"error"`.
- `stdout` / `stderr`: text captured by the output sink ([`output-capture.md`](output-capture.md)) during this command. `stderr` collects `popup_dialog(USAGE_POPUP,…)`, `popup_dialog(WARNING_POPUP,…)`, and anything currently going to `stderr`. `stdout` collects user-facing progress/status messages.
- `data` (object, optional): present when a command produces a structured result — e.g. any of the `q_*` query commands ([`query-commands.md`](query-commands.md)) or the `screenshot` command returning a path / inline blob. Schema is command-specific.
- `error.code` (string): from the closed taxonomy in [§ Error taxonomy](#error-taxonomy). `error.message` is a human-readable rendering. `error.detail` is an optional object for machine-readable context (e.g. nearest command for `unknown_command`).

### 3. State event (server → client)

Emitted after any command mutates viewer state (camera, time state, visible materials, colormap, selection, render mode). MCP stdio clients may ignore these; the Qt UI client consumes them to update inspector widgets without having to re-query.

```json
{
  "type": "event",
  "event": "state_changed",
  "state_seq": 17,
  "fields": { /* same schema as q_state payload; see query-commands.md */ }
}
```

- `state_seq` is monotonic within a session (UI invariant I3). Clients may request a fresh snapshot via `q_state` if they detect a gap.
- `fields` may contain only the diff since the last event, as long as every key present carries the new absolute value. The schema is the one defined in [`query-commands.md`](query-commands.md) § state dict.

Event emission is unconditional; a stdio client that does not care can discard them. The server emits them on its own schedule and never blocks command dispatch on event delivery.

### 4. Ready event (server → client, once per session)

Emitted once after startup has completed (DB loaded, OSMesa context initialized, initial frame drawn). Marks the point at which requests may be sent.

```json
{
  "type": "event",
  "event": "ready",
  "server_version": "4.3.0+server",
  "protocol_version": 1,
  "protocol_features": ["state_events", "screenshot_inline", "q_state"],
  "session_id": "griz-ab12cd34",
  "capabilities": {
    "max_request_bytes": 65536,
    "frame_codecs": ["jpeg", "png"]
  }
}
```

### 5. Session-ending event (server → client)

```json
{
  "type": "event",
  "event": "session_ending",
  "reason": "slurm_walltime" | "signal_term" | "server_shutdown",
  "seconds_remaining": 3
}
```

Gives the client a chance to save state before the process exits. Best-effort; the server does not wait for acknowledgement.

## Handshake

1. Transport connection is established (stdio: subprocess spawned; RPC: TCP accepted after rendezvous).
2. Server emits `ready`. The client reads and inspects `protocol_version`, `protocol_features`, `capabilities`.
3. Client sends `hello`:

   ```json
   { "type": "hello", "client": "griz-mcp/0.1.0", "min_protocol_version": 1, "max_protocol_version": 1 }
   ```

4. Server responds with `hello_ack`:

   ```json
   { "type": "hello_ack", "protocol_version": 1, "negotiated_features": ["state_events"] }
   ```

   If there is no overlap, the server responds with `{"type":"response","status":"error","error":{"code":"protocol_mismatch",…}}` and closes the connection. Per UI invariant I7, renegotiation mid-session is not supported.

5. Client begins sending requests.

MCP stdio clients that don't care about features may send a minimal `hello` with `min = max = protocol_version` from the `ready` message and accept whatever the server returns.

## Error taxonomy

Closed set of error codes. New codes require bumping `protocol_version`.

| Code | Meaning |
|------|---------|
| `unknown_command` | `parse_command()` did not recognize the command. |
| `bad_arguments` | Command recognized but argument parse failed. `stderr` contains the usage string. |
| `no_database` | Command requires a loaded database; none is open. |
| `database_error` | Mili load / read failure. `detail` includes the Mili error code. |
| `render_error` | OSMesa / GL failure during a rendering command. |
| `resource_limit` | Exceeded a quota (image size, request size, pending-events backlog). |
| `not_supported` | Command valid but the server build / feature flag disables it. |
| `protocol_mismatch` | Handshake failure. |
| `internal_error` | Uncategorized. `detail` contains a server log correlation id. |

`status="error"` responses always carry a code in this set. `internal_error` is the fallback; its presence in production is a bug.

## Pipelining and ordering

- Requests are strictly FIFO. The server processes one at a time; `state_changed` events for command N are emitted before the response for N.
- Clients MAY pipeline (send N+1 before the response for N arrives). The server reads greedily but dispatches serially.
- Events interleave with responses on the same stream. Clients must demultiplex by `type`.
- MCP's Python worker ([`../MCP.md`](../MCP.md) §5.3) holds a lock and does not pipeline; the protocol still permits it for future async use.

## Conflict resolution

Two differences between `MCP.md` §4.3 and `ui-design/02-protocol.md` (stub) are resolved here:

| Question | MCP.md original | UI.md stance | Resolution |
|----------|-----------------|--------------|------------|
| Error shape | `{"status":"error","message":"..."}` flat. | typed errors with codes. | **Typed errors win.** MCP.md's flat `message` becomes `error.message`; a `code` from the closed taxonomy is required. |
| Handshake | `{"event":"ready"}` only, no version. | explicit version handshake (I7). | **Version handshake wins.** `ready` now carries `protocol_version` and features; `hello` / `hello_ack` are required before the first request. Trivial overhead for MCP; essential for UI. |
| `data` field | not present. | not yet defined. | **Add `data` field.** `q_*` commands need a structured return; keeping it out of `stdout` means clients don't have to parse free-form text. |

## Open questions

- **Event back-pressure.** What does the server do when a slow client causes the event queue to fill? Current plan: drop older `state_changed` events and emit `state_overflow` so the client does a full `q_state` refetch. Needs validation when UI load-testing starts.
- **Binary data in responses.** Inline screenshots (RPC) and frame deltas will not live in JSON. Both transports need a side-channel story: stdio could base64-encode; RPC uses a separate binary frame type defined in `02-protocol.md`. Revisit when [`../MCP.md`](../MCP.md) §4.6 moves from file-based to inline screenshots.
- **Cancellation.** Long-running commands (`anim`, multi-state sweeps) currently can't be cancelled. Phase-2 question; probably a `type:"cancel"` request referencing an outstanding `id`.
