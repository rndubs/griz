# 02 — Protocol

## Scope

The wire contract between client and server: transport, framing, message catalog, versioning, error handling. Everything that must be agreed on before either side can be written.

Out of scope: how the server handles commands internally (see [03-server](03-server.md)), how the client renders received frames (see [05-rendering-and-streaming](05-rendering-and-streaming.md)).

## Related

- `UI.md` §3.3 (Transport), §9 (Protocol risk)
- [01-architecture](01-architecture.md), [06-picking-and-queries](06-picking-and-queries.md)

## Sections to fill

- **Transport.** TCP inside SSH as baseline. Single stream or multiple. Keepalives.
- **Serialization choice.** gRPC (protobuf + HTTP/2) vs. hand-rolled framed binary vs. something else. Criteria and recommendation.
- **Message catalog.** At least:
  - Session: `Hello`, `Capabilities`, `Goodbye`, `Error`.
  - Commands: `RunCommand(text)` → result stream.
  - State: `StateEvent(kind, payload)` server-pushed.
  - Rendering: `Frame(format, width, height, bytes, seq)`, `ResizeViewport`, `CameraInput`.
  - Picking: `Pick(x, y, mode)` → `PickResult(ids, metadata)`.
  - Queries: `Query(kind, args)` → `QueryResult`.
  - File: `ListPath`, `OpenDatabase`.
- **Versioning.** How the client and server negotiate. Forward-compat rules. What to do on mismatch.
- **Error model.** Fatal vs. recoverable. How server exceptions propagate. How partial results are signaled.
- **Flow control.** Back-pressure on frames when the client is slow. Dropping vs. queuing.
- **Sequencing.** Ordering guarantees between commands, state events, and frames.

## Open questions

- gRPC inside SSH: any known pain at real HPC sites?
- Protobuf schema location: in-tree `.proto` or separate repo?
- Do we need an out-of-band control channel (e.g., cancel a long-running command)?
