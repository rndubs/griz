# 05 — Rendering and streaming

## Scope

How pixels flow from `Src/draw.c`'s fixed-function pipeline through the OSMesa offscreen context out of `griz-server` to the Qt client's viewport. Covers OSMesa setup (mostly shipped), the frame-capture hook (partly shipped — `outrgb` writes an SGI to disk today), the encoder, the push pipeline, LOD policy during interaction, screenshot/animation export.

Out of scope: wire framing (see [02-protocol](02-protocol.md)), pick passes (see [06-picking-and-queries](06-picking-and-queries.md)), client-side widget lifecycle (see [04-client](04-client.md)).

## Related

- `../UI.md` §3.1 (Server rendering), §5 (Large-mesh priority)
- [01-architecture](01-architecture.md) §§4, 6; [02-protocol](02-protocol.md); [04-client](04-client.md)

## 1. Current state (2026-04)

| Piece | Status | Where |
|-------|--------|-------|
| OSMesa context creation | **shipped** | `OffscreenContext(offscreen, W, H, 0)` at `Src/viewer.c:3175–3183`, inside `process_server_mode_stdio`. Implementation at `Src/offscreen.c` (110 lines). |
| `init_mesh_window()` + first render | **shipped** | `Src/viewer.c:3185–3186` — `init_mesh_window(analy); analy->update_display(analy);` — renders a first frame before emitting `ready`. |
| Screenshot to disk (SGI) | **shipped via `outrgb`** | `outrgb <path>` is a regular interpret.c command; calls `write_image_file()`. Works headlessly today in the server. |
| `outjpeg` / `outpng` | **compiled out by default** | `./build.sh` uses `--enable-nojpeg --enable-nopng`; the commands return "Command not valid" at runtime. Rebuild without the flags to enable. See `/usr/WS1/whitmore/pydev/griz/CLAUDE.md` note. |
| Python-side PNG conversion | **shipped** | `pygriz/src/griz/_sgi.py` — stdlib-only SGI reader; `pygriz/src/griz/session.py:139–180` calls it after `outrgb`. |
| Streaming frames (command-unsolicited frames after any state change) | **not implemented** | Today a frame is only produced when the user explicitly issues `outrgb` / a draw command that writes to disk. |
| Encoder (JPEG/PNG/H.264) | **not implemented on the server side** | `--enable-nojpeg/-nopng` compiles out the commands; no in-memory encode path. |
| Level-of-detail policy | **not implemented** | — |

This is the gap set for the UI effort. The rest of this doc is spec.

## 2. OSMesa setup

### 2.1 Context

Already done (§1). Notes for the UI effort:

- **Buffer format.** Current `OffscreenContext` uses RGBA8. Sufficient for v1 (JPEG/PNG encoders are 8-bit). A float buffer would cost 4× memory and buys us nothing until we want HDR screenshots. Stay at RGBA8.
- **Allocation strategy.** Today OSMesa is allocated once at DB-open time at `init_griz_session`'s window size (default 1024×1024 from `-w`). On viewport resize, the server re-allocates — this needs verification (grep for `OffscreenContext` re-entry). If not re-allocated, add a `server_viewport_resize(w, h)` that releases and re-creates the context and buffer on the render thread. Cost per resize: one allocation, cheap compared to an interactive drag.
- **Max size.** Allocate lazily up to a per-session cap (e.g. 4096²) to bound memory. Reject larger `switch` commands with a typed error (`resource_limit`). [03-server](03-server.md) §8 covers the memory math.

### 2.2 Why OSMesa, not EGL

The existing `Src/offscreen.c` is OSMesa-based and works on every HPC node without a display. EGL-surfaceless would buy hardware acceleration if nodes had working GPU drivers, but that's a separate negotiation with site ops. OSMesa stays the baseline; EGL is a phase-2+ nice-to-have that fits behind the same `OffscreenContext` abstraction.

## 3. Frame capture hook

Render-thread pseudocode:

```
loop:
  wait for render_request (single-slot mailbox; latest wins)
  analy->update_display(analy)        // current call path; see §1
  glFinish()                           // fence: ensure pixels are in the buffer
  read_pixels_from_osmesa(rgba_buf)
  frame_seq = ++global_frame_seq
  encoder->submit(rgba_buf, W, H, frame_seq)
```

Notes:

- `analy->update_display()` is Griz's existing render entry point used by all command handlers that dirty the view. Reuse it; do not rewrite `Src/draw.c`.
- `glFinish` is needed because OSMesa's rendering is CPU-bound software — the driver buffers work otherwise.
- `read_pixels_from_osmesa` is essentially `glReadPixels` on the OSMesa framebuffer. `Src/offscreen.c` already exposes the pointer via `offscreen->buffer`.

### 3.1 When does the render thread fire?

Three triggers:

1. **State-mutating command** (command thread). After `parse_command()` succeeds and `notify_state()` fires, post a render request. Serialize one render per command; additional requests during the render are coalesced (latest wins).
2. **Client-driven interactive input** (I/O thread). During drag, every translated command (e.g. `rx 2`) causes a render. At 50 ms drag rate the render thread is the gating resource on big meshes. *Post-MVP* (§9): LOD policy (§5) reduces interactive cost on big meshes. MVP renders every drag command at full quality and relies on the §5.3 frame-cadence cap to bound load.
3. **Client-requested re-render** (explicit RPC). Viewport resize, LOD toggle, or "refresh at full quality" button. Same path as (1).

Nothing the **client** does outside those paths causes a frame.

## 4. Encoding

### 4.1 v1 codec: JPEG per frame

Small, simple, no inter-frame state, trivial to debug. Plain libjpeg-turbo. One encode per frame. Output as a single `kind=0x02` binary frame, subtype `0x01`, codec `0x01` — [02-protocol](02-protocol.md) §3 defines the byte layout.

JSON header (subtype-specific, inside the binary-frame envelope):

```json
{ "w": 1024, "h": 1024, "seq": 17, "rendered_at": 1.7e9,
  "encode_ms": 14, "fmt": "jpeg", "quality": 85 }
```

Target bitrate: 1080p at 85% quality ≈ 200–400 KB per frame. At 10 FPS that's ~3 MB/s, well within a tunneled SSH channel. At 30 FPS ≈ 10 MB/s, acceptable on-site but possibly borderline over WAN.

**Quality control.** A single integer 1–100 exposed to the client; default 85. Lossless PNG available for screenshots only (§7). No chroma subsampling tweaks in v1.

### 4.2 Post-MVP codec: H.264 (or AV1)

Post-MVP extension (§9). Motivation: WAN latency measurements at the end of phase 0 will tell us whether JPEG per frame is good enough. H.264 with delta frames cuts bandwidth ~10× for typical camera drags. Gate the work on those measurements; if JPEG per frame meets the latency target, this never lands.

Introducing H.264 requires:

- **Encoder.** Software x264 / x265 on CPU is straightforward and portable. Hardware encoders (NVENC, VAAPI) are tempting on GPU nodes but require site-specific configuration — explicit opt-in.
- **Protocol addition.** New codec id (`0x03`) in [02-protocol](02-protocol.md) §3. Inside the frame envelope, flags bit 1 (`keyframe`) already covers the H.264 IDR case.
- **Client decoder.** FFmpeg / libavcodec on the client. Adds a non-trivial dependency to the Qt build — worth bundling.

MVP commits to JPEG only.

### 4.3 What's not the codec

- **Inter-frame optimization inside JPEG.** Skipping a frame when the current matches the last one is a trivial ~2% win; do it on the render thread by hashing the framebuffer header, not on the encode side.
- **Color-profile embedding.** Neither JPEG nor PNG needs an ICC profile for our scientific data — colormaps already encode the mapping.
- **Audio.** No.

## 5. Level of detail

The large-mesh priority from `../UI.md` §5 says: interactive responsiveness on big meshes trumps full-quality everywhere. **MVP scope:** §5.3 (frame cadence cap) only — the rest is *post-MVP* (§9). Rationale: the bar71-class meshes used for proof-of-life render fast enough at full quality on any modern node; LOD complexity is not earning its keep until measurement on a representative large mesh shows interactive lag.

### 5.1 Interaction detection *(post-MVP)*

Client owns this. During a drag/zoom, it sends a `render_mode` hint with each command (e.g. `rx 5 /interactive`) or signals an out-of-band LOD event. Server enters **interacting** state, exits on 200 ms input quiescence back to **idle**, posting one final full-quality frame.

### 5.2 What LOD changes *(post-MVP)*

During **interacting**:

- Render at reduced viewport (e.g. ½ side → ¼ pixels). Client upscales with linear interpolation.
- JPEG quality drops from 85 to ~60.
- No shadow passes, no second-pass overlays — strip anything the legacy `draw.c` can gate via existing options. Candidate toggles: `mat_outline off`, `mesh off`, `edges off` when they would otherwise be on.

On **idle**:

- One re-render at full viewport and quality, streamed as a single high-quality frame (flag bit 2 = `last`).

No mesh subsampling — the Griz fixed-function pipeline doesn't have a cheap mesh-decimation path. Permanently out of scope for this LOD design; if it ever matters, it's a `draw.c`-level project, not a renderer-pipeline change.

### 5.3 Frame cadence *(MVP)*

- Cap the render-thread trigger rate to 30 Hz (configurable). Above that, coalesce drag commands server-side on arrival before enqueuing a render.
- Idle → no frames. No speculative rendering.

## 6. Back-pressure and drop policy

From [02-protocol](02-protocol.md) §4.2:

- Render thread → I/O thread: single-slot mailbox. If the previous frame hasn't been written yet, the new one overwrites — **latest wins**. The dropped frame's seq is still burned (monotonic); the client sees a gap and infers a drop.
- I/O thread → socket: non-blocking write. If the send buffer is full, drop the pending frame (latest-wins again at the socket level) and update stats.
- Client decode → UI: single-slot mailbox on the client side. Same policy.

Server-side statistics (frames rendered, frames encoded, frames dropped to queue, frames dropped to socket) are exposed through a future `q_stats` query and/or the periodic log file (`[03-server](03-server.md) §7.4`). Optional for v1.

## 7. Screenshot and animation export

### 7.1 Screenshot

Two modes:

- **Save to server disk.** The existing `outrgb <path>` command keeps working and is the mechanism the MCP bridge relies on (via `pygriz/src/griz/session.py:139–180`). Server writes SGI; client or MCP converts. No protocol change. **MCP path is MVP-complete today.**
- **Return inline to client (RPC).** New: add an `outpng_inline` (or extend `outrgb` with a `/inline` modifier) that emits a `kind=0x02` binary frame, subtype `0x02`, codec `0x02` (PNG). Use the same encoder plumbing as §4, with quality cranked to lossless PNG. **Required for the Qt client MVP** because [02-protocol](02-protocol.md) §6.1 commits the RPC transport to inline binary screenshots (no base64-in-JSON fallback). MCP stdio keeps the disk-path return per the same section — no base64-in-JSON path is being added there.

**Format consistency.** PNG is the default for screenshot output (lossless, universally decodable, already usable by the MCP `Image` type). SGI-to-PNG runs client-side today (`pygriz/src/griz/_sgi.py`); when a server-side PNG encoder lands, retire the SGI conversion.

### 7.2 Animation

`anim FROM TO` already exists in the interpret.c command set (sweeps the state range). For export:

- **MVP — server-side frame dump.** Existing paths (`anim` + `outrgb` per step) still work. Zero new server work; users assemble the dumped frames into a movie offline (e.g. `ffmpeg`) if they want one.
- **Post-MVP — server-side MP4** (§9). Record each rendered frame into an x264 encoder; on `anim` completion, emit a `kind=0x02` subtype `0x04` binary frame containing the final MP4. Cheap if H.264 is already in the build (§4.2) — both depend on landing the H.264 encoder, so they ship together if at all.

## 8. Minimum changes to `draw.c`

Target: **zero** intrusive changes to `Src/draw.c` (18k lines). The existing `analy->update_display` callback is the only hook needed. The server:

- Calls `update_display` from the render thread. Already thread-neutral; the single-thread GL invariant (**I6**) holds because only this thread ever calls it.
- Reads pixels after `glFinish`. Doesn't touch draw internals.
- Passes through any `draw`-setting commands (`wire`, `solid`, `mat`, etc.) unchanged via the regular command dispatcher.

If a specific draw command mutates state but does not set `analy->update_display_needed` (or whatever the existing dirty flag is called), that's a bug in `draw.c` to be fixed in place, not a reason to add a new dispatch path in the server.

## 9. Post-MVP / out of scope

MVP target is proof-of-life: the server renders a frame on every state-mutating command, JPEG-encodes it, and pushes it as a `kind=0x02` binary frame to the Qt client; the client displays it. Items below are deliberately deferred so MVP scope stays focused; nothing here is blocking and each links back to its originating section.

**Polish (deferred, will land in a later phase):**

- LOD policy (§5.1, §5.2): client-driven `interacting` / `idle` state, reduced viewport during drag, JPEG quality drop, idle full-quality re-render. MVP renders every frame at full quality and relies on the §5.3 30 Hz cadence cap. Gate landing on measurement showing interactive lag on a representative large mesh.
- H.264 / AV1 codec (§4.2): bandwidth-saving codec with delta frames. Gate on phase-0 WAN latency measurements; if JPEG meets the target, this never lands.
- Server-side MP4 export from `anim` (§7.2): bundled with the H.264 encoder above.
- `q_stats` query exposing render/encode/drop counters (§6).
- EGL-surfaceless backend behind the same `OffscreenContext` abstraction, for nodes with working GPU drivers (§2.2).
- **Raw-RGBA-over-zstd alternative** to JPEG for tiny interactive viewports. Measurement-gated; the per-frame JPEG encode cost on a small viewport may not actually beat zstd on raw bytes. Defer until LOD §5.2 lands and we have an interaction profile to measure against — without LOD there is no "tiny viewport" path to optimize.
- **Client-side frame caching for resize.** Stretch the last frame as a placeholder while the new-size frame is rendering, to hide the resize round-trip latency. UX polish; the [04-client](04-client.md) §6 fallback ("neutral background with a small overlay label") is the MVP behavior. Tracked in 04-client §13 if it lands.

**Permanently out of scope:**

- **Mesh subsampling for LOD.** The Griz fixed-function pipeline has no cheap mesh-decimation path; adding one is a `draw.c`-level project, not a renderer-pipeline change. If big-mesh interactivity ever demands it, that's its own initiative.
- **Parallel low-res thumbnail stream.** Motivated by reconnect / multi-viewport scenarios — neither is in the v1 plan (reconnect-to-live-server is gated on Invariant I12; multi-viewport is a phase-3 idea). Revisit only if those land.

## Open questions

*(All rendering/streaming open questions resolved as of 2026-04-19; resolutions are: JPEG is the v1 codec with raw-RGBA-over-zstd deferred to post-LOD measurement (§4.1, §9); client-side frame caching on resize is a 04-client UX call deferred to post-MVP polish (§9, [04-client](04-client.md) §13); parallel thumbnail stream permanently out of scope (§9); and golden-image determinism is tracked in [10-testing](10-testing.md), not here — OSMesa is deterministic across identical library versions, and cross-node drift is handled by the test framework's tolerance mode.)*
