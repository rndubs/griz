# Griz UI — Implementation Design Docs

This folder holds the detailed design sketches for the Griz UI modernization effort. The top-level plan lives in [`../UI.md`](../UI.md); the files here expand each area of that plan into something concrete enough to build from.

## How to use this folder

- One topic per file. Keep files focused and under a few hundred lines.
- Each doc starts with a **Scope** section (what it covers, what it doesn't) and a **Related** section (links to sibling docs and `UI.md` sections).
- Open questions and pending decisions go in an **Open questions** section at the end. Keep them until they're resolved in the doc body.
- Drafts are expected to iterate. Review happens per-file; don't batch reviews across the whole folder.
- Nothing in this folder is code. Code comes after the relevant doc is reviewed and agreed.

## Index

| # | Doc | Covers |
|---|-----|--------|
| 01 | [Architecture](01-architecture.md) | Processes, components, data flow, deployment topology |
| 02 | [Protocol](02-protocol.md) | Wire protocol, message catalog, framing, transport |
| 03 | [Server](03-server.md) | Server process, `interpret.c` as RPC, decoupling from `gui.c` |
| 04 | [Client](04-client.md) | Qt 6 app structure, views, state model, command console |
| 05 | [Rendering & streaming](05-rendering-and-streaming.md) | OSMesa path, frame encoding, LOD, frame policy |
| 06 | [Picking & queries](06-picking-and-queries.md) | ID buffer, ray pick, box/lasso, metadata RPCs |
| 07 | [Launch (SSH + SLURM)](07-launch-ssh-slurm.md) | Host profiles, launcher, rendezvous, tunneling |
| 08 | [Feature parity](08-feature-parity.md) | Motif dialog audit and new-UI mapping |
| 09 | [Build, packaging & CI](09-build-packaging-ci.md) | Toolchains, installers, signing, CI |
| 10 | [Testing](10-testing.md) | Test pyramid, fixtures, HPC end-to-end |
| 11 | [Migration](11-migration.md) | Coexistence with Motif, deprecation path |

## Status (2026-04)

All eleven docs are drafted. Each opens with a **Current state** section that anchors the design to what's shipped in `Src/` and the Python layer; `Src/server_*.c` and `pygriz/src/griz/worker.py` are the concrete reference points the Qt client builds on. Concretely:

- Docs 01, 02, 03 together define the full v1 server/client contract. Protocol framing (02) and the three-thread server (03) are the critical-path implementation targets.
- Docs 05 and 06 depend on the protocol; both reference the shipped OSMesa and interpret.c paths they extend.
- Docs 07 and 08 are largely independent of the rest — launch and Motif audit can progress in parallel with core work.
- Docs 09, 10, 11 — build/CI, testing, migration — consume and don't block the design work.

Each doc's **Open questions** section flags remaining decisions.

## Order of attack (suggested)

Rough dependency ordering for filling these out:

1. `01-architecture.md` — agree on the shape before fighting over details.
2. `03-server.md` + `09-decoupling` questions — know what the server actually is.
3. `02-protocol.md` — once server and client responsibilities are clear.
4. `05-rendering-and-streaming.md` and `06-picking-and-queries.md` — tied to protocol.
5. `04-client.md` — can be sketched earlier but benefits from protocol clarity.
6. `07-launch-ssh-slurm.md` — largely independent; can be drafted in parallel.
7. `08-feature-parity.md` — big audit, can start anytime, finishes late.
8. `09-build-packaging-ci.md`, `10-testing.md`, `11-migration.md` — after the core is clear.

## Conventions

- Diagrams: ASCII first, Mermaid or image links only if ASCII can't carry it.
- Code samples: only short illustrative snippets. Real code lives in the tree, not here.
- File paths: always repo-relative.
- Cross-references: link explicitly, don't rely on the reader to remember.
