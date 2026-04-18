# Griz UI Modernization Plan

## 1. Goals

- Replace the legacy Motif + X11 + GLw frontend with a modern, supported UI stack.
- Make Griz usable from a workstation without X11 forwarding or VNC, without degrading performance on large meshes.
- Keep the existing C compute/render core intact; do not rewrite `draw.c`, `interpret.c`, the Mili I/O layer, or the results pipeline.
- Preserve the full existing command vocabulary so scripts and user workflows keep working.
- Support Linux and macOS native clients at v1, with Windows added once the protocol is stable.

## 2. Current state (summary)

- GUI: Motif (Xm) on X11 Xt Intrinsics, concentrated in `Src/gui.c` (~9.9k lines). OpenGL viewport via the Motif `GLwMDrawA` widget, which is effectively unmaintained.
- Rendering: fixed-function OpenGL in `Src/draw.c` (~18k lines). A separate OSMesa path in `Src/offscreen.c` already supports headless rendering for batch mode.
- Command layer: `Src/interpret.c` (~11k lines) is a text command parser. The existing GUI is largely a generator of command strings that it hands to the interpreter. This is the key lever for modernization — the UI and the engine are already loosely coupled through a text protocol.
- Build: autoconf + `gmake`, with `debug`, `opt`, `batchdebug`, `batchopt` targets. The `batch*` targets already produce a GUI-less binary and are roughly 80% of the "server" we need.
- Obsolescence risks on modern HPC: Motif, libGLw, reliable X11 forwarding, reliable remote OpenGL.

## 3. Target architecture

A **native desktop client** that talks to a **headless Griz server** running on the HPC, using **image streaming** as the primary rendering transport.

```
+-------------------------+            SSH tunnel             +----------------------------+
|  Griz Client (native)   |  <---- commands / input ---->     |  Griz Server (headless C)  |
|  Qt 6, runs on user's   |  <---- rendered frames -----      |  existing core + interpret |
|  workstation            |  <---- pick / query results -     |  OSMesa offscreen render   |
+-------------------------+                                   +----------------------------+
            |                                                              |
   local UI chrome, menus,                                      Mili database on parallel FS
   command console, file I/O                                    SLURM-launched when needed
```

### 3.1 Server

- Derived from today's `batchopt` build: the existing C core minus `gui.c`.
- Commands arrive over a socket and are fed directly into `interpret.c`. No duplication of command logic.
- Rendering uses the existing OSMesa path; framebuffer contents are encoded (JPEG / PNG for v1, H.264 or similar later) and streamed to the client.
- Picking and queries are answered server-side (see Section 5).
- Runs unprivileged as the user's Unix account. Launched either via SSH on a login node or as a SLURM job on a compute node.

### 3.2 Client

- Qt 6 / C++. Rationale: matches the ParaView and VisIt precedent, compiles cleanly on Linux, macOS, and Windows, widget set maps cleanly to the existing Motif UI (menus, forms, dialogs, command entry, GL viewport), and the team already writes C.
- Presents a native window with local menus, dialogs, file pickers, and a command console. Only the viewport contents are streamed from the server.
- Maintains a mirror of session state relevant to the UI (current time step, active materials, colormap, etc.) by subscribing to server-side state change events.
- No OpenGL context and no Mili library required on the client.

### 3.3 Transport

- TCP inside an SSH tunnel as the baseline. Two channels (or one multiplexed channel): commands/events upstream, frames/replies downstream.
- Protocol: leaning toward gRPC for schema + bidirectional streaming, with a hand-rolled binary framing as a fallback if gRPC is impractical in the HPC environment. Decision deferred to the prototype phase.

## 4. Why client/server over X/VNC

- Bandwidth for image streaming is bounded by frame size, not mesh size — a 1080p stream costs the same whether the mesh has 1M or 1B elements. This matters because large-mesh viewing is Griz's primary use case.
- Only the viewport round-trips. Menu clicks, dialog interactions, command entry, and file I/O run locally and feel instant. VNC and X forwarding round-trip every UI event.
- Indirect GLX is disabled by default on modern X servers; real OpenGL over X typically requires VirtualGL, which is itself a server-side-render + image-stream solution bolted on at a lower layer. Doing this natively in the application is cleaner, faster, and more portable.
- XQuartz on macOS is unmaintained; Windows requires third-party X servers. Neither is a good user experience.
- VNC remains a viable informal fallback (users can run the old or new Linux binary inside a VNC session) but is explicitly not part of the supported path.

## 5. Large-mesh priority and selection

Large-mesh viewing is the top design priority. Two implications:

1. **No geometry streaming in v1.** The server owns the mesh. Snappy camera on small meshes via local rendering is a nice-to-have, not a requirement; it can be added later as an optimization without changing the protocol.
2. **Selection and picking are server-side.**
   - Point pick: client sends cursor `(x, y)`. Server reads an off-screen ID buffer (one render pass with element/node IDs encoded as color) and returns the hit ID plus metadata.
   - Ray pick: client sends a ray, server intersects against a BVH over the mesh.
   - Box / lasso select: client sends the region, server returns the matching IDs.
   - Metadata queries ("what is the Von Mises stress at node 12345?") are plain RPCs against the loaded dataset.
   - Highlighting: server re-renders with selection applied and streams the new frame. Client does not need geometry to show what is selected.
- All of this is kilobytes per interaction. Large meshes do not change the cost.

## 6. SLURM and SSH launch model (VisIt-style)

The client manages remote launch. Users should not need a separate terminal to start the server.

- **Host profiles.** The client stores named profiles describing each HPC site: hostname, user, authentication, default launch method (login node vs. SLURM), SLURM account/partition/walltime/nodes defaults, and custom launch commands. Profiles are per-user with an optional site-wide template.
- **Launch flow.**
  1. Client opens an SSH connection using the system SSH client (or an embedded library) and honors the user's existing SSH config, keys, and MFA prompts.
  2. On the login node the client starts a small launcher process, which either runs the Griz server directly (for quick interactive sessions) or submits a SLURM job (`sbatch` / `srun`) that starts the server on a compute node.
  3. The launcher reports the server's listening endpoint back to the client — either via stdout on the SSH channel or via a rendezvous file on the shared filesystem.
  4. The client opens a tunnel and connects to the server.
- **Session management.** Client UI shows job state (queued / running / failed), time remaining, and allows the user to extend walltime or cancel. Disconnect is graceful: the server exits cleanly and releases SLURM resources.
- **Authentication.** Rely on the user's existing SSH credentials. No new auth system. Kerberos and MFA prompts pass through.

## 7. Phased roadmap

### Phase 0 — Prototype (feasibility, 1–2 weeks)

- Stand up a minimal TCP server that accepts command strings and forwards them to `interpret.c`.
- Wire OSMesa output through a simple JPEG encoder and dump frames to disk.
- Confirm the existing `batchopt` build can be the starting point without forking the source tree.
- Write a throwaway Qt client that connects, sends commands, and displays frames. Validate end-to-end latency over a real HPC link.

### Phase 1 — v1 client/server (core feature parity subset)

- Freeze the protocol (gRPC or custom).
- Full Qt 6 client on Linux and macOS: menu bar, command console, file/database open, time step control, material manager, colormap, basic result selection, camera controls, picking (point + box), screenshot export.
- Server hardened: clean startup/shutdown, error propagation, state change events, framerate adaptation under load.
- VisIt-style host profiles and SSH launch for login-node sessions.
- CI builds + packaging (tarball on Linux, notarized `.app` on macOS).
- Old Motif GUI remains in the tree behind a build flag for a transition period.

### Phase 2 — HPC integration and polish

- SLURM launch with UI for partition/walltime/node count.
- Rendezvous file + tunnel management for compute-node sessions.
- H.264 (or equivalent) video encoding with level-of-detail during interaction.
- Feature parity with remaining Motif dialogs (surface manager, utility panel, isosurfaces, traction, etc.).
- Documentation and training material.

### Phase 3 — Windows client and optional geometry path

- Windows build: MSVC toolchain, MSI installer, code signing, CI.
- Optional geometry-streaming mode for small/medium meshes for snappier local camera control. Same protocol, new message type. Entirely additive.
- Remove the Motif GUI from the tree once usage telemetry or user feedback confirms migration.

## 8. Scope: in / out

**In scope**

- Qt 6 native client for Linux, macOS, eventually Windows.
- Headless server reusing the existing C core and command interpreter.
- Image streaming as the primary rendering transport.
- Server-side picking, selection, and metadata queries.
- VisIt-style client-managed SSH + SLURM launch.
- Preservation of the existing Griz command vocabulary and script compatibility.

**Out of scope**

- Web browser client.
- Rewriting `draw.c`, `interpret.c`, or the Mili I/O layer.
- Replacing the OpenGL fixed-function pipeline with modern shaders (can be done later, orthogonally).
- Python/Tcl scripting bindings (possible follow-on, not required for this effort).
- Supporting X forwarding or VNC as a first-class path.

## 9. Key risks and open questions

- **Protocol choice.** gRPC simplifies schemas and streaming but adds a dependency and may be awkward inside SSH tunnels on some sites. Prototype will decide.
- **Frame encoding.** JPEG is trivial and good enough for v1, but bandwidth to remote users may demand H.264 sooner than planned. Needs measurement on representative WAN links.
- **Interactive latency.** Acceptable rotation/zoom responsiveness over SSH+WAN is the single biggest UX risk. Needs a real-world measurement by the end of Phase 0 with an existing dataset and a realistic client location.
- **State synchronization.** The existing GUI reads a lot of global state directly. Building a clean state-change event stream from the server to the client may require a modest refactor of `viewer.c`.
- **Decoupling `gui.c` from the core.** The GUI is tightly coupled to engine state in places. A short audit early in Phase 0 should enumerate every place engine code assumes the Motif GUI is present, so the server build can stub or remove those cleanly.
- **Packaging and signing.** macOS notarization and eventual Windows code signing are non-technical but real ongoing costs.
- **User training.** Long-standing users have Motif muscle memory. The new client should stay close to current menu structure and command behavior to minimize retraining.

## 10. Success criteria

- A user on a laptop can open a native Griz client, connect to an HPC, load a multi-billion-element Mili dataset, rotate/zoom interactively, pick individual elements, step through time, and export a screenshot — without X11, without VNC, and without a terminal.
- The server binary is a close cousin of today's `batchopt` build and reuses the existing command interpreter unchanged.
- Existing Griz command scripts continue to run against the new server.

## 11. Next steps

Detailed design is underway in [`ui-design/`](ui-design/). The folder contains one markdown file per implementation area, each in a common skeleton (Scope / Related / body / Open questions) that we are expanding one at a time and reviewing before any code is written. See [`ui-design/README.md`](ui-design/README.md) for the full index and suggested reading order.

### Status of design docs

| # | Doc | Status |
|---|-----|--------|
| 01 | [Architecture](ui-design/01-architecture.md) | **Drafted.** Pins component names (`griz-client`, `griz-server`), no separate launcher, rendezvous-file bootstrap with 32-byte token, three-thread server and client models, and invariants I1–I8 that anchor every downstream doc. |
| 02 | [Protocol](ui-design/02-protocol.md) | Stub. Next up. |
| 03 | [Server](ui-design/03-server.md) | Stub. Next up. |
| 04 | [Client](ui-design/04-client.md) | Stub. Next up. |
| 05 | [Rendering & streaming](ui-design/05-rendering-and-streaming.md) | Stub. |
| 06 | [Picking & queries](ui-design/06-picking-and-queries.md) | Stub. |
| 07 | [Launch (SSH + SLURM)](ui-design/07-launch-ssh-slurm.md) | Stub. |
| 08 | [Feature parity](ui-design/08-feature-parity.md) | Stub. |
| 09 | [Build, packaging & CI](ui-design/09-build-packaging-ci.md) | Stub. |
| 10 | [Testing](ui-design/10-testing.md) | Stub. |
| 11 | [Migration](ui-design/11-migration.md) | Stub. |

### Immediate next tasks

1. Flesh out `02-protocol.md`, `03-server.md`, and `04-client.md` — all three take their pinned decisions from `01-architecture.md` and together define the v1 contract.
2. Then `05-rendering-and-streaming.md` and `06-picking-and-queries.md`, which depend on the protocol.
3. Then `07-launch-ssh-slurm.md`, followed by the operational docs (`08`–`11`).
4. Open questions accumulated in each doc should be triaged before code work begins.

No implementation work starts until the relevant design doc is reviewed.
