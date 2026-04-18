# 05 — Rendering and streaming

## Scope

How pixels get from the existing `draw.c` pipeline to the client window. Server-side OSMesa setup, frame capture, encoding, streaming policy, and level-of-detail behavior during interaction.

Out of scope: protocol framing (see [02-protocol](02-protocol.md)), picking reads (see [06-picking-and-queries](06-picking-and-queries.md)).

## Related

- `UI.md` §3.1 (Server rendering), §5 (Large-mesh priority)
- [01-architecture](01-architecture.md), [02-protocol](02-protocol.md), [04-client](04-client.md)

## Sections to fill

- **OSMesa setup.** Context creation, buffer allocation, format (RGBA8 vs. float). Resize handling — allocate once at max, or re-alloc on client resize?
- **Frame capture.** Where in the existing draw loop the grab happens. Any glFinish / fence cost to account for. Whether captures can overlap with the next render.
- **Encoding.**
  - v1: JPEG per frame. Simple, no state across frames, trivial to debug.
  - v2: H.264 (or AV1) with a delta stream for bandwidth on WAN links.
  - Quality controls: fixed quality vs. adaptive. Lossless mode for screenshots.
- **Level of detail policy.**
  - "Interacting" vs. "idle" state inferred from input stream.
  - During interaction: lower render resolution, lower JPEG quality, possibly coarser mesh rendering (clip, subsample).
  - On idle: full-res, high-quality render posted once.
- **Frame cadence.** Target framerate during interaction. How back-pressure from the client affects it. Dropping stale frames.
- **Client-driven re-render.** When the client needs a new frame with no state change (e.g., window resize, quality toggle), how it asks.
- **Screenshot / animation export.** Same render path at higher quality; PNG / MP4 output on the client or server side — pick one.
- **Existing `draw.c` modifications.** Minimum touch set to support a server-driven loop. Retain compatibility with the current GUI build during transition.

## Open questions

- Is the fixed-function pipeline in `draw.c` a blocker for OSMesa at high resolution, or does current batch mode already demonstrate it's fine?
- Hardware-accelerated encoding on HPC compute nodes: available? worth it? or stick with libjpeg-turbo / software x264?
- Do we need a separate thumbnail/low-res preview stream (e.g., for minimaps or reconnect)?
