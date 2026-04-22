# Griz UI Modernization Plan

## 0. MVP Implementation Tracker

Scope: the shipped stdio server, an RPC transport on top of it, a Qt 6 native client, and remote launch. Target deployment is [01-architecture.md §5](ui-design/01-architecture.md) variant A (Linux all-in-one, no SSH) first, then variants B–D (login-node and SLURM compute-node). Drawn from [`ui-design/01`–`07`](ui-design/); design docs 08–11 (feature parity, build/CI, testing, migration) are **not** part of the MVP tracker.

Status legend: ⬜ not started · 🟡 in progress · ✅ done

### Phase 1 — Server refactor & query surface (shared with MCP)
Source: [03-server.md §9 steps 1–5](ui-design/03-server.md). Richer state + events on today's shipped stdio server; no RPC dependency. Benefits the MCP bridge too.

- ✅ Extract `server_core_startup.c` and `server_stdio.c` from `Src/viewer.c:3118–3302` (no behavior change).
- ✅ Extract `server_query.c` from `Src/viewer.c:2955–3116` (no behavior change).
- ✅ Extend `q_state`, `q_view`, `q_materials` payloads to the full schema in [`shared/query-commands.md`](shared/query-commands.md) — [03-server.md §4](ui-design/03-server.md).
- ✅ Implement `q_selection`, `q_render`, `q_database`.
- 🟡 Implement `notify_state` + `Src/server_events.c` + `state_changed` event emission with monotonic `state_seq` — [03-server.md §5](ui-design/03-server.md). *(MVP blanket `notify_state_all()` after each mutating command; per-handler `interpret.c` instrumentation and `state_overflow` sentinel are follow-on.)*

### Phase 2 — RPC transport
Source: [02-protocol.md §7](ui-design/02-protocol.md), [03-server.md §9 steps 6–8](ui-design/03-server.md).

- ✅ Extract transport-neutral `server_core_dispatch_line()` from the current stdio loop (`Src/server_core.c`, `Src/server_stdio.c`). Both transports now share the same request → response → state_changed state machine, routed through a pluggable `ServerLineEmitter` hook.
- ✅ Add `Src/server_rpc.c`: `bind(127.0.0.1:0)`, write rendezvous JSON (0600, atomic rename, `$HOME/.griz/rendezvous/<session>.json`), `accept` (multi-client — up to `RPC_MAX_CLIENTS=4` concurrent peers, per [planning/DEMO.md task D](DEMO.md)), validate first-frame 32-byte base64 token with constant-time compare per client. Responses/hello_ack route to the currently-dispatching client via `current_client_idx`; `state_changed` events and auto-push binary frames broadcast to every authenticated client; per-client peer-idle closes only the offending slot while SIGTERM broadcasts `session_ending`. Covered by `pygriz/tests/test_rpc_multi_client.py`.
- ✅ 5-byte framing (`uint32 N + kind`) with `kind ∈ {0x01 JSON, 0x02 binary (post-MVP), 0x03 heartbeat}` and a 16 MiB cap — [02-protocol.md §2.2](ui-design/02-protocol.md). Heartbeats (`0x03`) are echoed verbatim so the client can compute RTT.
- ✅ SIGTERM / SIGINT handler emits `session_ending(reason="signal_term")` through the framed emitter and `shutdown(SHUT_RD)`s the client socket so the main loop exits promptly — [03-server.md §7.3](ui-design/03-server.md).
- ✅ 20 s heartbeat cadence + 60 s peer-idle detection. The dispatch loop now drives a `poll()`-based tick: outbound-idle ≥ 20 s emits a `kind=0x03` heartbeat; inbound-idle ≥ 60 s (checked only on a pure poll timeout, so queued heartbeats drain first) emits `session_ending(reason="peer_idle")`. Handshake reuses the same idle window as its hello-wait timeout.
- ✅ Port `pygriz_mcp/tests/test_smoke.py` to run against `--transport=rpc` as the envelope-parity gate. `pygriz/src/griz/rpc_worker.py` mirrors the stdio `Worker` API (length-framed JSON, hello-with-token, rendezvous-file discovery), and `pygriz_mcp/tests/test_smoke_rpc.py` swaps the `session.Griz` factory to use it — the assertions are case-for-case identical to the stdio smoke tests.
- ⬜ Split dispatch into three threads (command / render / I/O) with bounded MPSC queues and a latest-wins frame mailbox — [03-server.md §2.2](ui-design/03-server.md). Current RPC loop is single-threaded (valid v0 per [02-protocol.md §7](ui-design/02-protocol.md)).

New server flags (see `Src/server_main.c`): `--bind=HOST`, `--port=N`, `--rendezvous=PATH` — all optional, defaults are `127.0.0.1`, kernel-assigned port, `$HOME/.griz/rendezvous/griz-<hex>.json`.

### Phase 3 — Rendering & frame push
Source: [05-rendering-and-streaming.md](ui-design/05-rendering-and-streaming.md) MVP scope (§§3, 4.1, 5.3, 6, 7.1). LOD, H.264/AV1, MP4 animation export, `q_stats`, and EGL-surfaceless are post-MVP.

- 🟡 Render-thread capture hook: `update_display` → `glFinish` → `glReadPixels` from OSMesa. *(Synchronous helper lives in `Src/server_render.c` — `server_render_capture_rgba()` — and backs both the inline screenshot path and the post-command auto-push (below). Command/render/I-O thread split is still ⬜.)*
- ✅ In-process JPEG encoder (`server_render_encode_jpeg()` in `Src/server_render.c`): hand-rolled `jpeg_destination_mgr` so it works against both the vendored libjpeg 6-b and system libjpeg-turbo; takes RGBA8, drops alpha, emits baseline JPEG at caller-supplied quality (default 85 per 05 §4.1). Post-command auto-push (next bullet) wraps it into a `kind=0x02` subtype=0x01 codec=0x01 frame with the full `{w, h, seq, rendered_at, encode_ms, quality, bytes, fmt}` sub-header.
- ✅ Post-command auto-push: after a successful state-mutating command, the dispatcher fires `server_render_push_jpeg_frame()` through the binary emitter. Query commands (`q_*`) and stdio transport are skipped. Covered by `pygriz/tests/test_rpc_frames.py::test_mutating_command_pushes_jpeg_frame`.
- ✅ 30 Hz render-trigger cap with server-side coalesce. `server_render_push_jpeg_frame()` gates on a monotonic-clock interval (`SERVER_FRAME_MIN_INTERVAL_MS=33`); excess pushes burn their `frame_seq` (so the client sees drop gaps per 02-protocol.md §4.2) and set a deferred-pending bit. The RPC dispatch loop clamps its `poll()` timeout by `server_render_ms_until_next_frame()` and drains any queued frame via `server_render_flush_deferred_if_due()` on pure-timeout ticks, so a burst ends on a fresh frame reflecting the final state. Covered by `pygriz/tests/test_rpc_frames.py::test_burst_commands_are_rate_limited`.
- ⬜ Render → I/O single-slot latest-wins mailbox; monotonic `frame_seq` so the client infers drops from gaps. *(Monotonic counter shipped via `server_render_next_frame_seq()`; rate-limiter already burns seqs for coalesced pushes so the "infer drops from gaps" half is live. A real single-slot mailbox still depends on the three-thread split.)*
- ✅ Viewport resize: `server_render_resize_viewport(w, h)` reallocates the OSMesa RGBA backing buffer, rebinds the context via `OSMesaMakeCurrent`, and pushes the new dimensions through `glViewport` + `set_mesh_view()`. Exposed as the JSON command `{"cmd":"resize","w":N,"h":M}`; rejects out-of-range (each axis must be 1..4096) with `resource_limit`. Emits a state_changed event and an auto-push frame at the new size. Covered by `pygriz/tests/test_rpc_frames.py::test_resize_*`. The initial buffer allocated by legacy `OffscreenContext()` is not tracked and leaks on the first resize (one-time, ≤64 MiB); subsequent resizes free the previous buffer.
- ✅ Inline PNG screenshot path (`kind=0x02` subtype `0x02` codec `0x02`). `screenshot` command in `Src/server_core.c` (dispatcher) + `Src/server_render.c` (libpng in-memory encoder) emits the binary frame and a paired JSON response; the Python `RpcWorker.screenshot()` method correlates the two by `request_id`. Covered by `pygriz/tests/test_rpc_screenshot.py`. Continuation-frame chunking for >16 MiB bodies is still ⬜.

New infrastructure landed alongside the screenshot path:

- `server_set_binary_emitter()` / `server_emit_binary_frame()` in `Src/server_core.{c,h}` are the transport-neutral binary-frame hook — RPC installs a `kind=0x02` framer, stdio keeps a no-op default so `screenshot` surfaces a typed `unsupported_command` error there.
- `Src/server_render.{c,h}` is the new TU for render + codec work (added to `SERVER_OBJS`). JPEG encoder and viewport-resize helpers will live here.
- `./build.sh` no longer defaults to `--enable-nojpeg --enable-nopng`; libpng/libjpeg auto-detect on TOSS, so `outpng` / `outjpeg` (and the binary-frame encoder) are live by default.

### Phase 4 — Picking & element queries
Source: [06-picking-and-queries.md §10 MVP path](ui-design/06-picking-and-queries.md). Box select, ray pick, hover probe, ID-buffer cache, and multi-select derivations are post-MVP.

- ✅ `draw_mode=DRAW_IDS` flag in `Src/draw.c` (`griz_set_draw_ids_mode` / `griz_draw_ids_mode_active`) + per-primitive `griz_draw_id_primitive()` override that bypasses `draw_poly` and emits `glColor4f(packed_rgb, class_tag/255)` directly — necessary because `draw_plain_poly()`'s `glColor3fv` would clobber alpha. Instrumented in `draw_hexs`, `draw_tets`, `draw_quads_3d`, `draw_tris_3d`, and `draw_nodes_2d_3d`. Transparent materials (`diffuse[matl][3] < 1`) skipped per 06 §3.3. Beams/trusses/pyramids/wedges/particles are post-MVP.
- ✅ `pick_at <x> <y> <mode>` command in `Src/server_core.c`: runs `server_render_pick_at()` in `Src/server_render.c` (push attribs → disable lighting/blend/dither/smoothing → `glClearColor(0,0,0,0)` → `glReadBuffer(GL_FRONT)` → ID-pass render → `glReadPixels` at (x, h-1-y) → pop attribs → normal re-render), decodes `(packed_id, class_tag)` from the RGBA pixel, sets hilite through `griz_set_hilite`, and emits `{kind, id, index, material, coords_world, result_value}` on hit / `data=null` on miss. Wired before `parse_command` and piggybacks on the dispatcher's `notify_state_all()` + auto-push frame so the client sees the new hilite.
- ✅ Selection state: the `Analysis` struct already owns `selected_objects` (multi) + `hilite_class/num/label` (singleton); `griz_set_hilite` / `griz_clear_hilite` (extracted from the `hilite` branch of `parse_command` in `Src/interpret.c`) are the shared mutators. `state_changed` fires via the post-command `notify_state_all()` in `server_core.c`.
- ✅ `q_selection` was already implemented in Phase 1 (`build_q_selection` in `Src/server_query.c`) and is dispatched from `server_try_query`. Phase 4 leaves it untouched; the hilite-set path above drives fresh selection payloads.
- ✅ `q_node <id>` and `q_element <id>` metadata builders in `Src/server_query.c` (`build_q_node_data`, `build_q_element_data`), wired into `server_try_query` via a `parse_numeric_arg` helper. Returns `{id, index, coords, result_value, displacement:null, attached_elements:null}` for nodes and `{id, index, kind, type, material, connectivity, result_value, result_value_per_int_pt:null}` for elements. `q_element` scans superclasses in draw-instrumentation order (hex, tet, quad, tri, beam, truss, pyramid, wedge, particle) to find the class containing the user-facing label. Covered by `pygriz/tests/test_rpc_picking.py`.

### Phase 5 — Qt client scaffolding (variant A target)
Source: [04-client.md](ui-design/04-client.md). First target: Linux all-in-one loopback (client + server on the same workstation, no SSH, no SLURM) — variant A from [01-architecture.md §5](ui-design/01-architecture.md). The Python worker at `pygriz/src/griz/worker.py` is the reference implementation for the network layer.

**Qt version:** MVP ships against Qt **5.15** (system-installed on TOSS login nodes — the design-doc target of Qt 6 is not available as a module or in system paths). Qt 5.15 is the final LTS and covers every widget the client uses (`QOpenGLWidget` moved into `Qt5::Widgets` in 5.4). A later Qt 5→6 port is deferred; the change set is localized to include-path renames (`<QtOpenGLWidgets/QOpenGLWidget>`), the `QRegExp` → `QRegularExpression` swap, and a handful of `Qt::endl` / container changes. Conan / vcpkg is likewise deferred — the system Qt install sidesteps the cert/rustls fetch problem that `uv` hits.

- 🟡 `client/` tree per [04-client.md §2.1](ui-design/04-client.md); CMake + Qt 5 Widgets; system-Qt dependency (Conan/vcpkg deferred). Top-level `client/CMakeLists.txt` + per-subtree static libs (`griz-client-net`, `griz-client-model`, `griz-client-ui`) with `CMakePresets.json` for `linux-release` / `linux-debug`. Build driver: `./build_client.sh [release|debug] [-- <cmake args>]` at the repo root; output at `client/build/linux-<type>/src/griz-client`. Requires `cmake >= 3.16` (default TOSS cmake works; `module load cmake/3.26.3` is fine). Qt 6 upgrade + Conan pinning is tracked with [04-client.md §2.2](ui-design/04-client.md) / [09-build-packaging-ci.md §3.2](ui-design/09-build-packaging-ci.md) for when we leave TOSS-only.
- ✅ `net/Worker` + `net/Framing`: length-framed read/write, hello/hello_ack, request-id correlation with per-id waiter + timeout. Wire bring-up is live: `client/src/net/Framing.cpp` implements big-endian 5-byte header framing with the 16 MiB cap; `client/src/net/Rendezvous.{h,cpp}` parses the `$HOME/.griz/rendezvous/*.json` schema; `client/src/net/Worker.{h,cpp}` drives `QProcess` spawn → rendezvous poll → `QTcpSocket` connect → hello (carrying the 32-byte base64 token) → `hello_ack` → `ready` → `sendCommand` / `runCommand` / `shutdown`. Heartbeats (`kind=0x03`) are echoed for symmetric RTT; binary frames (`0x02`) surface via `frameReceived`. Threading is single-thread-per-Worker on the creating thread's event loop; the three-thread split from [04-client.md §9](ui-design/04-client.md) is a later `moveToThread()` refinement that doesn't change this public API. Covered end-to-end by `client/tests/net/test_worker_smoke.cpp` — spawns a real `griz-server --transport=rpc`, runs the handshake, and round-trips `q_state` in ~95 ms.
- 🟡 `model/SessionState`: in-memory mirror of `q_state`; apply `state_changed` diffs; `stateOverflow()` signal triggers a full refetch — [04-client.md §4](ui-design/04-client.md). *(`applyFullState` parses the full schema in `planning/shared/query-commands.md` — database, time, view, render, materials, results, selection — and emits the per-section `*Changed` signals. `applyDiff` walks `event.fields` with replace-not-merge semantics, advances `m_lastSeq`, and fires `stateOverflow()` on a sequence gap. Wired in `App` so the post-`connected()` `q_state` and every subsequent `state_changed` event refresh the live mirror.)*
- 🟡 `ui/MainWindow`: menubar (File / Edit / View / Draw / Select / Animate / Window / Help); every menu action routes through `Console::execute`. *(Menubar structure shipped, plus accessors (`console()`, `materialsDock()`) and `setConnectionStatus` / `setHostLabel` / `setStateSeqLabel` / `setFpsLabel` slots used by `App`. Per-action wiring landed via `MainWindow::runCommand()` → `Console::execute`: File → Open Database… (Ctrl+O, `QFileDialog` → `load <path>`), Run Script… (`rdhis <path>`), Quit (Ctrl+Q, `QApplication::quit`); Edit → Clear Console (local `Console::clearOutput`); View → Reset View (F, `rview`), Projection → Persp/Ortho (exclusive `QActionGroup`, `switch persp` / `switch ortho`); Draw → exclusive Solid/Wireframe/Hidden-Line/Point Cloud (`switch solid|wf|hidden|cloud`); Select → Clear Hilite (`clrhil`), Clear All Selection (`clear_selection`); Animate → First/Previous/Next/Last (Home `f`, `,` `p`, `.` `n`, End `l`) + Play/Pause (Space, `anim`); Window menu hosts `QDockWidget::toggleViewAction()` for Materials / Selection / Results / TimeSlider / Console so dock visibility is checkable + persistable; Help → About (`info`). Layout save/restore: `closeEvent` saves `saveGeometry()` + `saveState()` under `QSettings("LLNL", "griz-client")`; ctor restores after the hardcoded default layout so a first-run client still sees the design-doc defaults. Save-image / Edit → Copy / Preferences / Mouse-mode radios are still ⬜ — next wiring pass picks those up.)*
- 🟡 `ui/Viewport`: `QOpenGLWidget` that blits received frames; resize-to-server throttled to 250 ms quiescence; neutral-background fallback when disconnected. *(Frame blit live: `Viewport::onBinaryFrame` parses the 02-protocol §3 sub-header (subtype/codec/flags + uint16-BE hdrlen + JSON `{w,h,seq,…}` + JPEG body), decodes via `QImage::loadFromData("JPEG")`, and draws in `paintEvent` with letterboxed `KeepAspectRatio` scaling. Wired in `App` to `Worker::frameReceived`, so the bar71 model paints once the server's post-`q_state` auto-push arrives. `resizeEvent` restarts a 250 ms single-shot `QTimer` that emits `resizeRequested(w, h)` → `App` sends `{cmd:"resize", w, h}` via the new `Worker::sendCommand(cmd, extras)` overload; `App::onWorkerConnected` calls `flushPendingResize()` so the server's initial `-w 1024 1024` framebuffer resyncs to the real widget once the handshake lands. FPS from a rolling 30-sample window of arrival timestamps streams out via `Viewport::fpsChanged` → `MainWindow::setFpsLabel`.)*
- 🟡 `ui/Console`: prompt + output pane; in-session Up/Down history; inline error rendering with `error.code` badge. *(Wired to `App::onConsoleCommand` → `Worker::sendCommand`; `Worker::responseReceived` is formatted by `App::formatResponse` — captured `stdout`/data on success, `[error.code] message` on error — and appended to the output pane. Live in `client/src/App.cpp`.)*
- 🟡 `commands/CommandBridge`: drag → `rx`/`ry`/`tx`/`ty`, scroll → `zoom`, click → `pick_at`, `R` → `rview`; one-command-in-flight rate limit. *(`client/src/commands/CommandBridge.{h,cpp}` new subtree; installs an event filter on `Viewport`. Left drag → `rx`/`ry` at 0.25 deg/px; `Shift`+left or middle drag → `tx`/`ty` at 0.002/px; wheel → `zf`/`zb` by `1.10^notches`; release within 3 px of press → `pick_at <x> <y> any`; R key → `rview`; P key toggles projection (`switch persp` / `switch ortho` alternated via a local `m_orthoActive` bool); digits `1`/`2`/`3` → preset views using compound `;` commands (front = `rview`, right = `rview; ry 90`, top = `rview; rx -90`). `F` and `Space` are window-scoped menu shortcuts, not viewport-filter keys, so they work even without viewport focus. One command in flight: mouse-move deltas accumulate into a pending coalesced vector that flushes on response; wheel/click/key drop-newest while unacked. Dominant-axis picker per flush sends either `rx` or `ry` (not both) so the non-dominant delta survives to the next idle tick. Build: `griz-client-commands` static lib linked into the executable.)*
- 🟡 Docks: `MaterialsDock`, `TimeSlider`, `ResultsDock`, `SelectionDock` wired to `SessionState` signals via `Qt::QueuedConnection`. *(All four docks now read from `SessionState` **and** push user intent back through a `commandRequested(QString)` signal that `App` forwards to `Worker::sendCommand`. `MaterialsDock` — `QTableWidget` with per-row Visible / Enabled checkboxes + color swatch + `id: label`; toggling a box emits `vis N` / `invis N` / `enable N` / `disable N` (Src/interpret.c line 3188), with a `m_suppressEmit` gate while we repopulate on `materialsChanged`. `TimeSlider` — horizontal `QSlider` bound to 1-based `state_min..state_max` with a `state N / M  t=X  (t_min..t_max)` label, plus transport QToolButtons ⏮ / ⏪ / ▶‖ / ⏩ / ⏭ wired to `f` / `p` / `anim` / `n` / `l`; the Play button tracks `time.animating` (icon flips to pause when active). New `time_min` / `time_max` keys in `q_time` / `q_state.time` feed the absolute-range display (server_query.c build_q_time). User slider moves emit `stateRequested(N)` → `state N`, and `m_suppressEmit` breaks the mirror loop (slider has `tracking=false`, scrubbing sends only on release). `ResultsDock` — QComboBox populated from `q_state.results.results[]` (flat catalog of `{name, title, origin}` from server_query.c build_q_results); the combo is editable with a case-insensitive contains-filter completer for fast lookup, and picking emits `show <name>`. Below the picker, `QFormLayout` with primary / component / griz_name / min / max, hides all rows when `hasActive=false`. `SelectionDock` — per-kind element + node `QListWidget`s with counts and a `hilite: one object`/`(none)` header, driven by `selectionChanged`; row click emits `hilite <kind> <id>` using the server's class short name kept on the client via the new `PickedItem { kind, id }` struct.)*
- 🟡 Status bar: connection state, FPS from `frame_seq` deltas, host nickname, current `state_seq`. *(Connection label transitions through `connecting…` → `connected` → `disconnected` driven by `Worker::connected`/`disconnected`; `seq=N` updates from `state_changed` via `SessionState::lastSeq()`; FPS is live — `Viewport` records each binary-frame arrival time and emits a rolling 30-sample estimate on `fpsChanged`, which `MainWindow::setFpsLabel` renders. On **unexpected** disconnects `MainWindow::flashDisconnect()` pulses the connection label between a saturated red background and a muted red-bold text state 4× over ~1.4 s, then leaves it in the muted red state until reconnect; clean quits go through `setConnectionStatus()` with no flash. Host nickname is still hardcoded `"local"` pending Phase 6 host profiles.)*
- 🟡 Error presentation: handshake `protocol_mismatch` modal, tunnel-death modal, `state_overflow` silent refetch — [04-client.md §10](ui-design/04-client.md). *(Wired in `App`: `Worker::protocolMismatch` shows a `QMessageBox::critical` naming both versions; unexpected disconnects (reason contains `peer_idle` or `exited`) show `QMessageBox::warning` with the reason, suppressed when `m_shuttingDown` is set by `onAboutToQuit` so clean-quit doesn't nag; `stateOverflow` silent refetch was already live (re-sends `q_state`). Tunnel-death modal proper pends Phase 6 `SshDriver` — current coverage is the subprocess-exit path.)*

### Phase 6 — Remote launch (SSH + SLURM)
Source: [07-launch-ssh-slurm.md](ui-design/07-launch-ssh-slurm.md). Prerequisites: shared `$HOME` between login and compute nodes ([Invariant I11](ui-design/01-architecture.md)) and working ambient SSH auth — `ssh user@host` succeeds from the user's terminal without interactive prompts ([07 §5.0](ui-design/07-launch-ssh-slurm.md)).

- ⬜ `hosts.toml` schema + loader (`model/HostProfiles`) per [07 §2.2](ui-design/07-launch-ssh-slurm.md); site starter profiles in `/etc/griz/hosts.d/`.
- ⬜ **Connect…** dialog and **Host → Manage hosts…** dialog — [04-client.md §3](ui-design/04-client.md), [07 §2.4](ui-design/07-launch-ssh-slurm.md).
- ⬜ System-SSH driver (`net/SshDriver` wrapping `QProcess`); honors user `~/.ssh/config`, agent, `known_hosts`.
- ⬜ Launch method `direct` — login-node server with `forbid_login_node_compute` site guard.
- ⬜ Launch method `slurm` — `sbatch --parsable` + `srun` on a compute node (primary).
- ⬜ Launch method `preallocated` — `srun --jobid=<id>` into an existing allocation.
- ⬜ Rendezvous read: poll `ssh cat <path>` at 500 ms cadence, 60 s timeout; surface `sacct`/`squeue` state on failure.
- ⬜ `ssh -N -L` tunnel lifecycle owned by the network thread; tunnel death → "Connection lost — relaunch?" modal (single connection per server lifetime, Invariant I12).
- ⬜ SLURM UI phases: `PENDING` (queue + ETA), `RUNNING` (rendezvous poll), `READY` (walltime countdown in status bar, modal warning <5 min remaining).
- ⬜ `session_ending(reason="slurm_walltime")` modal with countdown and Relaunch / Save & quit actions.

---

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

## 11. Relationship to the MCP effort

A parallel effort ([`MCP.md`](MCP.md)) exposes Griz to Python and MCP-compatible AI clients via a stdio JSON bridge. It shares its C-side underpinnings with this UI effort. To avoid duplicated or diverging implementations, the overlapping pieces are specified in [`shared/`](shared/) and referenced from both plans:

- [`shared/server-binary.md`](shared/server-binary.md) — one `griz-server` binary with `--transport={stdio,rpc}`. MCP uses `stdio`; this UI effort uses `rpc`.
- [`shared/command-protocol.md`](shared/command-protocol.md) — common JSON envelope (request / response / event), versioned handshake, typed errors. RPC-specific framing lives in [`ui-design/02-protocol.md`](ui-design/02-protocol.md) and wraps these same messages.
- [`shared/output-capture.md`](shared/output-capture.md) — `griz_out()` / `griz_err()` sink indirection so `popup_dialog` / `wrt_text` / `printf` cannot corrupt the transport.
- [`shared/query-commands.md`](shared/query-commands.md) — `q_state`, `q_view`, `q_materials`, … and the canonical state schema reused by this UI's `state_changed` events.
- [`shared/results-map.md`](shared/results-map.md) — single-source-of-truth mapping from human-readable result names to Griz command names, consumed by the Qt client and the Python package.

The Qt client owns its own UI state and the rendering-transport decisions (frame codec, picking, LOD); the MCP bridge owns Python ergonomics and MCP tool wrappers. The C-side dispatcher layer, output capture, query commands, and results map are shared and should be built once.

## 12. Next steps

Detailed design is underway in [`ui-design/`](ui-design/). The folder contains one markdown file per implementation area, each in a common skeleton (Scope / Related / body / Open questions) that we are expanding one at a time and reviewing before any code is written. See [`ui-design/README.md`](ui-design/README.md) for the full index and suggested reading order.

### Status of design docs

| # | Doc | Status |
|---|-----|--------|
| 01 | [Architecture](ui-design/01-architecture.md) | **Drafted.** Pins component names (`griz-client`, `griz-server`), no separate launcher, rendezvous-file bootstrap with 32-byte token, three-thread server and client models, invariants I1–I8. |
| 02 | [Protocol](ui-design/02-protocol.md) | **Drafted.** Length-framed transport on top of the shipped JSON envelope; binary-frame sub-protocol; token auth; keepalives; back-pressure. |
| 03 | [Server](ui-design/03-server.md) | **Drafted.** Starts from today's shipped stdio server; pins TU refactor (server_stdio/_rpc/_query/_events), three-thread model, full `q_*` catalog, `notify_state`, SIGTERM handling, and a 10-step TODO. |
| 04 | [Client](ui-design/04-client.md) | **Drafted.** Qt 6 layout, `SessionState` model, command console, viewport, selection UI, threading. Explicit Python → Qt translation table using `pygriz/worker.py` as the reference implementation. |
| 05 | [Rendering & streaming](ui-design/05-rendering-and-streaming.md) | **Drafted.** Reuses the shipped OSMesa path; JPEG v1 / H.264 v2; LOD policy during drag; screenshot in-memory PNG; minimal `draw.c` changes. |
| 06 | [Picking & queries](ui-design/06-picking-and-queries.md) | **Drafted.** `pick_at` command, ID-buffer pass, selection state, metadata queries, multi-select derivations, 10-step implementation order. |
| 07 | [Launch (SSH + SLURM)](ui-design/07-launch-ssh-slurm.md) | **Drafted.** Host profiles (TOML schema), three launch methods (direct/slurm/preallocated), rendezvous read-over-ssh, system-SSH by default, tunneling, SLURM UI, reconnect. |
| 08 | [Feature parity](ui-design/08-feature-parity.md) | **Drafted.** Motif audit framework + seed table (~30 rows). Material manager called out as standalone item. |
| 09 | [Build, packaging & CI](ui-design/09-build-packaging-ci.md) | **Drafted.** Server build is already shipped in `Src/Makefile.Library`; client build is new (CMake + Qt + Conan); CI matrix; release signing. |
| 10 | [Testing](ui-design/10-testing.md) | **Drafted.** Pyramid with the existing 14-smoke-test suite from `pygriz_mcp/tests/test_smoke.py` as the protocol-conformance seed. |
| 11 | [Migration](ui-design/11-migration.md) | **Drafted.** Phase 1/2/3 gates, rollback path, script-compatibility guarantee, deprecation schedule. |

### Immediate next tasks

1. Close open questions per doc (each has an explicit **Open questions** section).
2. Begin implementation work on the highest-value server extensions from [`ui-design/03-server.md`](ui-design/03-server.md) §9:
   - **Steps 1–5** (extract server TUs from `Src/viewer.c`, fill out `q_*` payloads, add `state_changed` events) benefit both the UI and MCP efforts and do not require RPC yet.
   - **Steps 6–10** (RPC transport, thread split, SIGTERM, picking, frame push) are the RPC-specific critical path for Phase 1.
3. Scaffold `client/` (CMake + Qt) per [`ui-design/04-client.md`](ui-design/04-client.md) §2 and [`ui-design/09-build-packaging-ci.md`](ui-design/09-build-packaging-ci.md) §3. Variant A (local all-in-one) is the right iteration loop before SSH+SLURM ships.

No implementation work starts until the relevant design doc is reviewed.
