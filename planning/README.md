# Griz Planning

This folder holds design plans for in-flight Griz initiatives. It is living documentation: each plan is expanded and refined before any code work begins on its behalf. Code goes in `Src/`; narrative and decisions live here.

## Contents

### Top-level plans

- [`UI.md`](UI.md) — replace the legacy Motif / X11 / GLw frontend with a Qt 6 native client driving a headless `griz-server` over an RPC transport, with image streaming, server-side picking, and VisIt-style SSH+SLURM launch.
- [`MCP.md`](MCP.md) — expose Griz to Python and MCP-compatible clients (including AI assistants) through the same `griz-server` binary over a stdio JSON transport, plus a public `griz` Python package and a thin `griz-mcp` adapter.

### Detailed design folders

- [`ui-design/`](ui-design/) — per-topic implementation design for the UI effort (architecture, protocol, server, client, rendering, picking, launch, build, testing, migration). See [`ui-design/README.md`](ui-design/README.md) for status and reading order.

### Shared components

- [`shared/`](shared/) — pieces used by **both** the UI and MCP plans. Split out here so they aren't implemented twice or allowed to diverge.

  | Doc | Covers |
  |-----|--------|
  | [`shared/server-binary.md`](shared/server-binary.md) | One `griz-server` binary; `--transport={stdio,rpc}`; build target. |
  | [`shared/command-protocol.md`](shared/command-protocol.md) | Shared request / response / event envelope; handshake; versioning; typed errors. |
  | [`shared/output-capture.md`](shared/output-capture.md) | `griz_out()` / `griz_err()` sink indirection. |
  | [`shared/query-commands.md`](shared/query-commands.md) | `q_*` query commands and the canonical state schema. |
  | [`shared/results-map.md`](shared/results-map.md) | Single-source-of-truth `(field, component) → griz name` mapping. |

## How the pieces relate

```
                  ┌──────────────────┐            ┌──────────────────┐
                  │       UI.md      │            │      MCP.md      │
                  │  (Qt UI effort)  │            │  (Python + MCP)  │
                  └────────┬─────────┘            └────────┬─────────┘
                           │                               │
                           │          both reference       │
                           ▼                               ▼
                  ┌────────────────────────────────────────────┐
                  │                 shared/                    │
                  │  server-binary · command-protocol ·        │
                  │  output-capture · query-commands ·         │
                  │  results-map                               │
                  └────────────────────────────────────────────┘
                           ▲
                           │
                  ┌────────┴─────────┐
                  │   ui-design/     │  Detailed UI implementation docs.
                  │   (UI-specific)  │  RPC framing, launch, packaging, etc.
                  └──────────────────┘
```

If a UI-specific doc needs something that also concerns MCP, promote it into `shared/` rather than duplicating. If a shared doc grows a detail that only one side cares about, push that detail back into the relevant plan or `ui-design/` doc.

## Conventions

- One topic per file.
- Each doc starts with a **Scope** section and a **Related** section linking sibling / parent docs.
- Open questions live in a trailing **Open questions** section until resolved in the body.
- Nothing in this folder is code. Code follows doc review.
- File paths in prose are repo-relative.

## Status at a glance

| Plan | Status |
|------|--------|
| `UI.md` | Drafted. All eleven `ui-design/` docs now drafted with references to the shipped server and Python layer. Implementation not started. |
| `MCP.md` | Drafted; MVP shipped (see `MCP.md` § 0 Implementation Status). |
| `shared/` | Drafted. Each doc carries a **Current state** section that tracks what's shipped in `Src/server_*.c` and the Python layer vs. still aspirational. |
