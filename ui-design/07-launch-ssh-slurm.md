# 07 — Launch (SSH + SLURM)

## Scope

How a user goes from "double-click the client" to "connected to a running server on the HPC." Covers host profiles, SSH integration, SLURM submission, rendezvous with the spawned server, and tunnel lifecycle. Modeled on VisIt's approach.

Out of scope: what happens after the connection is up (see [01-architecture](01-architecture.md) for session topology, [11-migration](11-migration.md) for rollout concerns).

## Related

- `UI.md` §6 (Launch model)
- [01-architecture](01-architecture.md), [03-server](03-server.md)

## Sections to fill

- **Host profiles.**
  - Fields: nickname, hostname, username, auth hint (key path, agent, prompt), launch method, default SLURM settings, environment module loads, paths to `griz-server`.
  - Storage location on the client (per-user config file). Format (TOML / JSON / YAML — pick one).
  - Import/export so site admins can ship starter profiles.
- **Launch methods.**
  - Direct on login node: `ssh <host> <env-setup> && griz-server --stdout-endpoint`.
  - SLURM interactive: `ssh <host> sbatch --wrap "srun griz-server ..."` with rendezvous.
  - SLURM pre-allocated: user has an existing allocation; connect into it.
- **SSH integration.**
  - Use system `ssh` by default (honors user's `~/.ssh/config`, agent, MFA). Spawn as a child process.
  - Alternative: embedded `libssh` for environments without system SSH (Windows especially). Decision and rationale.
- **Rendezvous.** How the client learns the server's listen endpoint.
  - Stdout handoff: server prints endpoint on its first line, launcher relays it.
  - File-based: server writes endpoint to a shared path; client polls via SSH.
  - Callback: server dials back to the client. Rarely works on HPC — include only as a note.
- **Tunneling.** Setting up the local port forward. Cleaning it up on disconnect.
- **SLURM UI.** Client-side form for partition, account, walltime, nodes, tasks, GPUs, extra `sbatch` args. Remembered per host.
- **Session reporting.** Show queue state (pending → running), remaining walltime, live logs (tail of the server's stderr over the SSH channel).
- **Reconnect and extend.** If the SLURM job is still running but the tunnel dropped, client should reconnect without re-submitting. Extending walltime is out of scope unless SLURM allows it trivially.
- **Credential handling.** No credential caching by the client. Everything through the existing SSH trust chain.

## Open questions

- Do we need to support sites that require ssh jump hosts / bastions? Probably yes — confirm.
- MFA prompts: can the system-ssh path always surface them, or do we need an in-app prompt dialog?
- Is there a site that uses LSF / PBS / Flux instead of SLURM among the target users? If so, abstract the scheduler interface.
