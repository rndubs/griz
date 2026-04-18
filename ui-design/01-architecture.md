# 01 — Architecture

## Scope

Defines the overall shape of the new Griz: which processes exist, what each owns, how they talk, and where they run. This is the anchor document that all other docs refer back to.

Out of scope: message-level protocol details (see [02-protocol](02-protocol.md)), UI layout (see [04-client](04-client.md)), specific rendering pipeline (see [05-rendering-and-streaming](05-rendering-and-streaming.md)).

## Related

- `UI.md` §3 (Target architecture), §4 (Why client/server), §6 (Launch model)
- [02-protocol](02-protocol.md), [03-server](03-server.md), [04-client](04-client.md)

## Sections to fill

- **Component diagram.** Client, server, launcher, Mili store, SLURM, SSH. ASCII box-and-line.
- **Process inventory.** What processes exist at runtime. Parent/child relationships. Lifetimes.
- **Data flow.** Commands, frames, events, picks, state snapshots. Who originates, who consumes.
- **Deployment topology.** Workstation vs. login node vs. compute node. What runs where and why.
- **Threading model (per process).** Server: render loop, command handler, socket I/O. Client: UI thread, network thread, decode thread.
- **Failure domains.** What happens when the client drops, the server crashes, the SLURM job dies, the SSH tunnel breaks.
- **Naming.** Settle on the names of the binaries and components (e.g., `griz-server`, `griz-client`, `griz-launcher`) so later docs don't drift.

## Open questions

- One server process or server + separate render process?
- Does the launcher persist on the login node for the session lifetime, or exit after handing off?
- Single SSH connection multiplexed, or one per channel (commands vs. frames)?
