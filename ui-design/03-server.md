# 03 — Server

## Scope

The headless Griz server: what it is, how it's built from the existing code, how it integrates `interpret.c`, how it emits state events, and what needs to change in the current source tree to make it possible.

Out of scope: wire protocol (see [02-protocol](02-protocol.md)), rendering details (see [05-rendering-and-streaming](05-rendering-and-streaming.md)).

## Related

- `UI.md` §3.1 (Server), §9 (Decoupling `gui.c`)
- [01-architecture](01-architecture.md), [02-protocol](02-protocol.md)

## Sections to fill

- **Starting point.** How the server derives from the existing `batchopt` build target. What stays, what is removed, what is added.
- **`interpret.c` as RPC.** Where in the call graph an incoming `RunCommand` is injected. Output capture (Griz command feedback currently goes to stderr/stdout and GUI panels). Error paths.
- **Decoupling audit.** Enumerate every dependency from engine code into `gui.c` (globals, function calls, widget assumptions). Strategy for each: remove, stub, or replace with an event.
- **State event emitter.** Where engine state changes originate (time step, active materials, result selection, colormap, camera, view limits). Plan for instrumenting these points to publish `StateEvent`s. Consider an `events.c` translation layer so engine code stays naive.
- **Session state.** One session per process, or multiplexed? Lifecycle of mesh data, OSMesa context, command interpreter state across connect/disconnect.
- **Logging and diagnostics.** Where logs go on the HPC side. Correlation with client session ID.
- **Resource footprint.** Memory and CPU expectations for large-mesh sessions. Relevant to SLURM defaults in [07-launch-ssh-slurm](07-launch-ssh-slurm.md).
- **Signal / shutdown handling.** Graceful exit when SLURM sends SIGTERM at walltime, when SSH drops, when the client disconnects.

## Open questions

- Does any engine code block on GUI callbacks today? If so, those paths need a headless equivalent.
- How intrusive is adding state events — a few dozen sites, or scattered through 30k lines?
- Do we keep `gui.c` compiling alongside the server build during the transition, or fork immediately?
