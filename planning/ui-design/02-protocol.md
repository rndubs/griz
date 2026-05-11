# 02 — Protocol

## Scope

The wire contract between `griz-client` and `griz-server --transport=rpc`: transport carrier, byte-level framing, how the JSON envelope from [`../shared/command-protocol.md`](../shared/command-protocol.md) is carried, binary frames (images, picks), versioning, flow control, and sequencing guarantees.

**Not** redefined here: request/response shapes, hello/hello_ack, error taxonomy, and the `q_*` + `state_changed` catalog — all of that is pinned in [`../shared/command-protocol.md`](../shared/command-protocol.md) and applies verbatim to RPC. Only the framing and the additions needed for image streaming are RPC-specific.

## Related

- `../UI.md` §3.3 (Transport), §9 (Protocol risk)
- [`../shared/command-protocol.md`](../shared/command-protocol.md) — canonical envelope.
- [01-architecture](01-architecture.md) §§4, 6 — data flow and threading.
- [05-rendering-and-streaming](05-rendering-and-streaming.md) — frame codec choices.
- [06-picking-and-queries](06-picking-and-queries.md) — pick request/reply payloads.

## 1. Current state (2026-04)

Stdio transport is shipped; it newline-delimits the same JSON envelope the RPC transport will length-prefix. What's already proven end-to-end and can be reused:

- **Envelope shape.** `{"type":"request"|"response"|"event"|"hello"|"hello_ack", ...}` — see `Src/server_core.c:139–208` (emitters) and `Src/server_core.c:74–116` (parser).
- **Handshake.** `ready` → `hello` → `hello_ack` implemented at `Src/server_core.c:449–519`, driven from `Src/viewer.c:3203, 3221`.
- **Error taxonomy (partial).** `invalid_syntax`, `command_error`, `unknown_command` emitted today; `Src/server_core.c:251–293`.
- **Reference client.** `pygriz/src/griz/worker.py` (427 lines). Does line-oriented framing, request/response matching by `id`, timeouts, and a background reader thread. The Qt client's network thread needs the same state machine with 4-byte length prefix instead of newline delimitation.

Unshipped on the C side and must land with the RPC transport: framing, binary frames, keepalives, flow control, authentication token check.

## 2. Transport and framing

### 2.1 Carrier

- **Baseline:** plain TCP on `127.0.0.1:<ephemeral>` on the compute or login node. The client reaches it through an SSH local-forward tunnel (see [07-launch-ssh-slurm](07-launch-ssh-slurm.md)). No TLS on the wire — confidentiality comes from the SSH tunnel. The server binds to loopback only; never to a public interface.
- **Fallback:** `ssh -L` is the only supported transport in v1. Any non-tunneled deployment is considered misconfiguration and the server should refuse (config-level guard, documented in [07-launch-ssh-slurm](07-launch-ssh-slurm.md)).
- **Single TCP stream** carries all five message classes enumerated in [01-architecture](01-architecture.md) §4. Multiplexing is by message type inside the stream, not by separate channels. Rationale: SSH multiplexing is finicky at some sites; a single stream also avoids head-of-line surprises between command replies and frames (which the protocol orders explicitly anyway).

### 2.2 Framing

Each message is framed as:

```
+---------+--------+-------------------------+
| 4 bytes | 1 byte | N bytes                 |
|   N     | kind   | payload                 |
+---------+--------+-------------------------+

N     : big-endian uint32, length of the payload in bytes (excluding
        the 5-byte prefix). Capped at 16 MiB; frames above this are a
        protocol error and close the connection.
kind  : 0x01 = JSON envelope (UTF-8, no trailing newline required).
        0x02 = binary frame (image / pick-buffer).
        0x03 = heartbeat (payload is empty or a small timestamp).
        0x04–0xFF reserved; unknown kinds are a protocol error.
```

The 4-byte length prefix matches common framing in gRPC length-prefixed messages and is trivial to implement with `read_exact(4)` + `read_exact(N)`. Size cap exists to bound client allocations; large images are split or transported as a sequence of `kind=0x02` frames with an explicit continuation flag inside the binary header (see §3).

**Compression:** none on the JSON kind in v1 (most payloads are small; compression frustrates debugging). Binary frames carry their own codec tag and are already compressed by the codec.

**Why not gRPC / protobuf.** Two reasons the original UI.md §3.3 planned to "decide in prototype" and we now lean against: (a) gRPC's HTTP/2 stack does not play well with some HPC SSH configurations and site audit processes, and (b) the MCP effort has proven that the JSON envelope is expressive enough for every command-surface need and is trivial to mirror in two languages. Keeping RPC JSON-shaped preserves the "one envelope, two transports" invariant. A hand-framed transport wrapping the same JSON is a shorter implementation path and a shorter review.

### 2.5 Decision: hand-framed JSON for v1

The transport shape described in §2.2–§2.4 is the v1 commitment, not a prototype choice. No gRPC revisit is on the v1 roadmap. Resolved 2026-04-19.

**Future revisit triggers** (post-v1; do not block v1 work):

- **Streaming RPC need.** If pick replies, frames, or a future streaming-result command class grow ordering or back-pressure semantics that the §4 rules cannot express cleanly. Current evidence (§4.1) is that frames are explicitly out-of-band and command FIFO is sufficient — no streaming abstraction needed.
- **Second consumer.** If a non-Qt client (e.g. a web UI, a third-party visualization shell) appears and needs the same surface. Mitigation already in place: the JSON envelope is identical across stdio/RPC/MCP, so a thin websocket bridge in Python or Go reaches a browser without rewriting the server.

If neither trigger fires, hand-framed JSON stays. The cost of revisiting later is bounded because the envelope itself is portable.

### 2.3 Authentication

The rendezvous file ([01-architecture](01-architecture.md) §3) carries a 32-byte base64 token. First frame sent by the client after TCP connect must be a JSON envelope of kind `hello` with an added `token` field:

```json
{ "type": "hello",
  "client": "griz-client/0.1.0",
  "min_protocol_version": 1,
  "max_protocol_version": 1,
  "token": "VV5KT2Rj…base64…" }
```

The server validates with a constant-time compare against the token it wrote into the rendezvous file. On mismatch: emit a single error response with `error.code = "protocol_mismatch"` and close the socket after flush. No retry permitted; the client must re-read the rendezvous and reconnect.

Rationale: both ends run as the same UID so this is belt-and-suspenders, but it defends against accidental reuse of a leaked port number by another user on the same host. Constant-time compare prevents trivial timing leaks on a shared login node.

### 2.4 Keepalive

- Client sends a `kind=0x03` heartbeat every 20 s of outbound idle. Payload: 8-byte client-side nanosecond timestamp, echoed verbatim by the server in the next heartbeat so the client can compute RTT.
- Server sends a `kind=0x03` heartbeat every 20 s of outbound idle.
- Either side closing the socket on a 60 s silence is acceptable behavior; the opposite end treats this as `session_ending(reason="peer_idle")`.

Heartbeats carry no command semantics and do not affect `state_seq`.

## 3. Binary frames

`kind=0x02` binary frames carry payloads that would be wasteful or impossible inside JSON: rendered frames, pick ID buffers (when dumped for diagnostics), large screenshot results, future video deltas. A binary frame's payload is:

```
+---+---+---+---+---+--------+-------------------+
| 1 | 1 | 1 | 1 | S | header | body              |
+---+---+---+---+---+--------+-------------------+

byte 0: subtype (0x01=frame, 0x02=screenshot, 0x03=pick_buffer, ...)
byte 1: codec   (0x00=raw RGBA8, 0x01=jpeg, 0x02=png, 0x03=h264_nal, ...)
byte 2: flags   (bit0 = continuation, bit1 = keyframe, bit2 = last)
byte 3: reserved (0)
bytes 4..4+S-1: variable-length header, JSON-encoded (subtype-specific)
                with a leading uint16 len S
bytes 4+S..:    body (opaque to the transport, codec-decoded by consumer)
```

JSON header carries context the consumer needs without decoding the body (e.g. frame width/height/sequence/timestamp, screenshot request id, pick reply id). This keeps the decode thread cheap: it reads the header, demuxes to the right consumer, hands the body down.

Every binary frame is correlated either to a request id (screenshot, pick reply) or to a server-side sequence counter (rendered frames). See [05-rendering-and-streaming](05-rendering-and-streaming.md) for frame-sequence semantics and [06-picking-and-queries](06-picking-and-queries.md) for pick semantics.

**Size-cap enforcement.** Bodies exceeding the §2.2 16 MiB cap MUST be split across continuation frames; no subtype is exempt. The writer chunks the body, sets `flags bit0 = continuation` on all but the final chunk, and sets `flags bit2 = last` on the final chunk. The reader reassembles by `(subtype, request_id)` (or `(subtype, frame_seq)` for rendered frames). Lossless screenshots of large viewports are the motivating case: an 8K RGBA capture is ~256 MiB and streams as ~16 chunks. Keeping the cap uniform avoids leaking subtype-specific allocation policy into the framing layer.

## 4. Flow control and ordering

### 4.1 Ordering guarantees

From [01-architecture](01-architecture.md) §4 and [`../shared/command-protocol.md`](../shared/command-protocol.md) § Pipelining:

- **Command FIFO.** Requests are processed one at a time. Any `state_changed` event caused by command N is emitted before the response for N.
- **Frames are out-of-band.** Rendered frames interleave freely with responses and events. Clients demux by kind.
- **Pick reply after input frame.** When the client sends a pick RPC, the server replies after rendering (or hit-testing) against the most recently committed state. No guarantee the pick hits the very frame the user clicked on — the client must tolerate up to one state step of skew. Practically, the server holds the last ID buffer for the currently-displayed frame and answers from it (see [06-picking-and-queries](06-picking-and-queries.md)).

### 4.2 Back-pressure

Three TCP-level queues and one application queue matter:

1. **Client → server** (commands, heartbeats): tiny, bounded by user input rate. If the server's read is slow, the kernel's SO_SNDBUF applies back-pressure onto the client network thread — acceptable.
2. **Server → client** (responses, events, frames): this is the one that can fill. Policy:
   - Responses and events are written unconditionally; they are small and the client must always be ready to consume them.
   - Rendered frames are **dropped at the source** when the outgoing socket's buffer is full. The render thread produces into a single-slot mailbox consumed by the I/O thread; a new frame replaces the pending one rather than queueing (`latest-wins`). This is what allows a 1 FPS link to remain responsive on a 60 FPS render.
3. **Client decode queue**: single-slot mailbox between network thread and decode thread, same `latest-wins` semantics.

The server signals "I dropped frames" by including a monotonic `frame_seq` on every rendered frame; a gap tells the client it missed frames. The client does not ask for retransmission — frames are ephemeral.

**Future consideration (post-v1, evidence-gated):** TCP-over-SSH can stall on packet loss over a WAN, which the latest-wins drop policy mitigates but does not eliminate during continuous interaction. A separately-authenticated UDP channel for frames (commands staying on TCP) is a known mitigation, but adds a second socket, a second auth handshake, and would not traverse the `ssh -L` tunnel mandated in §2.1 — `ssh -w` or direct routing is a site-policy minefield. Defer until WAN-from-home measurements show >30% frame stalls during interaction; revisit then.

### 4.3 Cancellation (deferred)

Long-running commands (`anim`, state sweeps, full-mesh `outrgb`) cannot be interrupted today. The open question in [`../shared/command-protocol.md`](../shared/command-protocol.md) — a `{"type":"cancel", "id":"<outstanding-id>"}` request — stays open here and is deferred past v1.

## 5. Versioning

Follows [`../shared/command-protocol.md`](../shared/command-protocol.md) § Handshake exactly:

- Protocol version lives at `Src/server_core.c:21` (`GRIZ_PROTOCOL_VERSION "1.0"`). This constant is what the `ready` event reports and what `hello_ack` matches against.
- Client sends `min_protocol_version` / `max_protocol_version`; server picks the highest mutually supported.
- Mismatch → `error.code = "protocol_mismatch"` response, then the server closes the socket. **No downgrade, no feature-flag haggling in v1.**
- A version bump is required for: new error code, new event type, changed schema shape in a `q_*` response (additive optional keys do not require a bump; removing or renaming does).

When the first `state_changed` event or first `q_selection` implementation lands on the server, the protocol version stays at 1.0 provided the additions are advertised via `protocol_features` in the `ready` envelope. If a feature the client **requires** is absent, the client refuses to connect with a clear error.

## 6. Error propagation

Per [`../shared/command-protocol.md`](../shared/command-protocol.md) § Error taxonomy, all errors from commands flow as `response.status="error"` with a typed `error.code`. Transport-level problems (framing errors, size-cap violations, auth failure, protocol-version mismatch) map to one of the existing codes — primarily `protocol_mismatch` for the auth/version cases and `internal_error` for malformed framing — and close the connection.

Partial results: not supported. A command either succeeds with its full `data` payload, or errors. Server-side streaming-result commands (none today; potentially future `animate` with per-state progress) would need a new message type and a protocol-version bump.

### 6.1 Binary payload policy

Two transports, two policies. Resolved 2026-04-19.

- **RPC.** Binary frames per §3 are mandatory. No base64-in-JSON fallback. The Qt client is the only v1 RPC consumer and is being built alongside the protocol; a JSON-only RPC variant would double the screenshot-emission path for a hypothetical client that does not exist. If a third-party JSON-only RPC client ever appears, the §2.5 second-consumer mitigation (websocket bridge) reaches it without changing the wire format.
- **MCP stdio.** Screenshots remain disk-path returns: the `screenshot` tool writes to a file and returns the path. No base64 inlining. Rationale: most MCP hosts cap or truncate multi-megabyte tool responses; base64 inflates ~33%; disk-path lets the agent defer reading the image until it actually needs to look at it. This matches the pattern of other MCP image tools and is what the current MVP already does.

If a vision-enabled MCP host later wants inline images in the same turn, base64-in-JSON can be added then as an opt-in tool variant. The disk-path default does not preclude it.

## 7. Prototype plan

Land RPC as a strict lift of the stdio path. Concrete steps once [03-server](03-server.md) decomposition ships:

1. Extract the current stdio loop body from `Src/viewer.c:3205–3279` into a transport-neutral `server_core_dispatch()` that consumes a `ServerRequest` and writes a `ServerResponse` to an opaque sink.
2. Add `Src/server_rpc.c` that: binds, writes the rendezvous file, accepts, reads framed messages, calls the common dispatcher, and writes framed responses. No threading yet — single-threaded RPC is a valid v0.
3. Add length-prefixed I/O helpers; reuse cJSON for envelope (de)serialization.
4. Port `pygriz_mcp/tests/test_smoke.py` to run against `--transport=rpc` — proves envelope parity.
5. Layer in heartbeats, size caps, token check.
6. Introduce the render/command/I-O thread split from [01-architecture](01-architecture.md) §6 only once single-threaded RPC is solid.

## Open questions

*(All protocol-level open questions resolved as of 2026-04-19; resolutions are captured in §2.5, §3 (size-cap enforcement), §4.2 (WAN UDP future consideration), and §6.1 above.)*
