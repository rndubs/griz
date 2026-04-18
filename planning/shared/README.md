# Shared Components

This folder documents the C-side and data-layer pieces that are needed by **both** the UI modernization effort ([`../UI.md`](../UI.md)) and the MCP / Python effort ([`../MCP.md`](../MCP.md)).

The purpose is to avoid two independent implementations of the same wrapper layer. The UI client and the MCP bridge sit on top of the same server process and the same command surface; only the transport (TCP RPC vs. stdio JSON) differs.

## Index

| Doc | Covers |
|-----|--------|
| [server-binary.md](server-binary.md) | Single `griz-server` binary with `--transport={stdio,rpc}`; build artifact story. |
| [command-protocol.md](command-protocol.md) | Shared request / response / event envelope; handshake; versioning; error shape. |
| [output-capture.md](output-capture.md) | `griz_out()` / `griz_err()` sink indirection so `popup_dialog` / `wrt_text` / `printf` do not corrupt the protocol stream. |
| [query-commands.md](query-commands.md) | Canonical `q_*` query commands and the state-dict schema they return (reused by UI state events). |
| [results-map.md](results-map.md) | Single source of truth for the `(field, component) → griz result name` mapping, shipped as a data file and consumed by both the Python package and the Qt client. |

## Invariants these docs share

- **S1. One server binary.** `griz-server` is the only new binary. Both transports live behind a single `--transport=` flag. Legacy `griz` and `griz_batch` are unchanged.
- **S2. One envelope.** Request, response, state event, and ready event use the same JSON object shape regardless of transport. stdio newline-delimits; RPC length-prefixes.
- **S3. One mutator.** Every state change still routes through `interpret.c`'s text-command grammar, per UI invariant I1. Query commands read state but do not mutate.
- **S4. One state schema.** The dict returned by a `q_*` query is structurally identical to the `fields` payload of a `state_changed` event.
- **S5. One results map.** The human-readable result name table is data, not code. Both front ends load it at startup.

## How to reference these docs

From `UI.md` / `MCP.md` and their implementation folders, link relatively:

```markdown
See [`shared/command-protocol.md`](shared/command-protocol.md) …
```

From `planning/ui-design/*.md`, go up one level:

```markdown
See [`../shared/command-protocol.md`](../shared/command-protocol.md) …
```
