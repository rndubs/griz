# 04 — Client

## Scope

Architecture and structure of the Qt 6 native client: application skeleton, main-window regions, client-side state model, command console, viewport widget, selection UI, persistence, threading, error presentation. Concrete enough to start CMake + Qt scaffolding against.

Out of scope: the wire protocol (see [02-protocol](02-protocol.md)), launch flow (see [07-launch-ssh-slurm](07-launch-ssh-slurm.md)), per-dialog feature parity (see [08-feature-parity](08-feature-parity.md)).

## Related

- `../UI.md` §3.2 (Client), §7 (Roadmap)
- [`../shared/command-protocol.md`](../shared/command-protocol.md), [`../shared/query-commands.md`](../shared/query-commands.md), [`../shared/results-map.md`](../shared/results-map.md)
- [01-architecture](01-architecture.md) §6 (threading), [02-protocol](02-protocol.md), [05-rendering-and-streaming](05-rendering-and-streaming.md), [06-picking-and-queries](06-picking-and-queries.md)

## 1. Current state (2026-04)

**Nothing in C++ / Qt exists yet.** The Python worker at `pygriz/src/griz/worker.py` (427 lines) is the reference implementation of the client side of the protocol and carries most of the design decisions the Qt network layer should mirror:

- Spawn server, wait for `ready`, perform hello/hello_ack handshake.
- Auto-incrementing request id (`f"req_{next(counter)}"`).
- Background reader thread; main thread blocks on a per-id condvar.
- Per-command timeout with `GrizCommandError` on `status="error"`.
- Terminator command (`quit`) + graceful subprocess drain + hard kill on timeout.

The Python session layer at `pygriz/src/griz/session.py` is the reference for namespaced client APIs (`.field`, `.view`, `.time`, `.materials`, `.selection`), which map nicely to Qt dock panels.

Treat this doc as the C++ equivalent specification; see §11 for an explicit mapping.

## 2. Application skeleton

### 2.1 Repo layout

```
client/                             # NEW, sibling to Src/
  CMakeLists.txt
  conan/ or vcpkg/                  # third-party manifest; pick one (§12)
  resources/
    icons/ ... styles.qss ... results_map.yaml (copy of Src/data/)
  src/
    main.cpp
    App.{h,cpp}                     # QApplication shell, CLI parse
    net/
      Worker.{h,cpp}                # mirrors pygriz/worker.py
      Framing.{h,cpp}                # length-prefix frame reader/writer
      Rendezvous.{h,cpp}             # read $HOME/.griz/rendezvous via ssh
      SshDriver.{h,cpp}              # child process wrapper
    model/
      SessionState.{h,cpp}           # client-side mirror of server state
      ResultsMap.{h,cpp}             # loads Src/data/results_map.yaml
      HostProfiles.{h,cpp}           # ~/.config/griz/hosts.toml
    ui/
      MainWindow.{h,cpp,ui}
      Viewport.{h,cpp}               # streamed-frame GL widget
      Console.{h,cpp}                # command console
      MaterialsDock.{h,cpp}
      TimeSlider.{h,cpp}
      ResultsDock.{h,cpp}
      SelectionDock.{h,cpp}
      HostDialog.{h,cpp}
    commands/
      CommandBridge.{h,cpp}          # "click translates to `rx 30`" layer
  tests/
    net/      model/      ui/
```

One CMake target per subtree (`griz-client-net`, `griz-client-model`, `griz-client-ui`, and the top-level `griz-client`). Keeps test scope tight.

### 2.2 Qt modules

- `Qt6::Core`, `Qt6::Widgets`, `Qt6::Gui`, `Qt6::Network` (for the TCP socket), `Qt6::OpenGLWidgets` (for the viewport), `Qt6::Concurrent` (small use, for decode).
- **No** `Qt6::Quick` — widgets-only keeps the visual closer to the current Motif UI (a deliberate choice to minimize retraining per UI.md §9).
- yaml-cpp for `results_map.yaml`.
- libssh2 OR external system `ssh`: deferred to [07-launch-ssh-slurm](07-launch-ssh-slurm.md).

### 2.3 Menu bar

Start by mirroring the current Motif menu top-level structure (see [08-feature-parity](08-feature-parity.md) for the per-item audit):

```
File   Edit   View   Draw   Select   Animate   Window   Help
```

Every menu action routes through the same code path as a command console entry: builds a Griz command string and calls `worker.cmd(...)` (invariant **I1**). This keeps the command history meaningful and aligns with MCP.

## 3. Main window regions

```
+--------------------------------------------------------------+
| [menubar]                                                    |
+--------------------------------------------------------------+
|      |                                                |      |
| Mat  |                                                | Res  |
| eria |                                                | ults |
| ls   |                Viewport                        | dock |
| dock |           (streamed frames)                    |      |
|      |                                                |      |
+------+                                                +------+
| Sele |                                                | Time |
| ctio |                                                | cont |
| n    |                                                | rol  |
| dock |                                                |      |
+------+------------------------------------------------+------+
| Command console (prompt + output + history)                  |
+--------------------------------------------------------------+
| [statusbar: connection state | FPS | remote host | state N]  |
+--------------------------------------------------------------+
```

- Central widget: `Viewport`.
- Dock widgets (Qt `QDockWidget`, user-movable): `Materials`, `Selection`, `Results`, `TimeSlider`. Allow layout save/restore via `QMainWindow::saveState()`.
- Console is a persistent bottom dock (not an overlay), sized ~20% of window height by default. Command history persists across sessions.

## 4. Client state model

`SessionState` holds a full in-memory mirror of the server's state dict as shipped by `q_state` and kept fresh by `state_changed` events. Mirrors the schema in [`../shared/query-commands.md`](../shared/query-commands.md) exactly.

```cpp
class SessionState : public QObject {
  Q_OBJECT
public:
  const DatabaseInfo& database() const;
  const TimeState&    time()     const;
  const ViewState&    view()     const;
  const RenderState&  render()   const;
  const std::vector<Material>& materials() const;
  const ResultsState& results()  const;
  const Selection&    selection() const;

  uint64_t lastSeq() const;   // last seen state_seq

signals:
  void timeChanged(const TimeState&);
  void viewChanged(const ViewState&);
  void materialsChanged();
  void resultsChanged(const ResultsState&);
  void selectionChanged(const Selection&);
  void stateOverflow();       // triggers full q_state refetch
};
```

### 4.1 Seeding

On connect / reconnect: `worker.cmd("q_state")` → parse into `SessionState` → emit all `*Changed` signals (for the initial paint). Widgets connect to these signals only; they never poll the worker directly.

### 4.2 Live updates

`Worker` posts `state_changed` envelopes onto the main thread via `Qt::QueuedConnection`. The handler applies the diff key-by-key, updates `lastSeq()`, and emits the relevant signal(s). On `state_overflow`, trigger a full `q_state` refetch.

**Diff semantics:** per the protocol spec, nested objects in `fields` are replacements, not merges. Simplifies the handler.

### 4.3 Binding to widgets

Each dock observes one or two signals and repopulates its model:

| Widget | Signal bound to | Action |
|--------|-----------------|--------|
| `TimeSlider` | `timeChanged` | update slider position + labels |
| `MaterialsDock` | `materialsChanged` | repopulate list model |
| `ResultsDock` | `resultsChanged` | update field/component dropdowns + min/max labels |
| `SelectionDock` | `selectionChanged` | update the selected-elements table |
| `Viewport` | — | consumes frame stream directly, not state |
| Menu check-states | `renderChanged` | sync toggles (`ontime`, `oncmap`, ...) |

## 5. Command console

Must behave like the current Motif console (so scripts and muscle memory survive) and cooperate with menus (so every menu click is reviewable in the history).

Features:

- **Prompt + output pane.** Separate panes so command-echo doesn't scroll output off. Output pane renders `response.stdout` + `response.stderr` appended per command. Commands, their ids, and responses scroll with monotonic timestamps.
- **History.** Up/Down cycle through prior commands; `Ctrl-R` reverse-search. History persists in `$XDG_CACHE_HOME/griz/history.log` (one per host profile).
- **Autocomplete.** Client-side static list for v1 (harvest tokens from `Src/interpret.c`'s command-table walker; ~200 commands). Server-provided via a new `q_commands` query is a v2 nice-to-have. Tab completion on command name; argument completion deferred.
- **Scripts.** `rdhis FILENAME` already exists in the Griz command set and works over the wire verbatim. The client also exposes `File → Run Script…` that `cmd()`s `rdhis <path>`.
- **Menu interop.** Every menu action calls `Console::execute(QString)` which renders the command in the console and then dispatches to worker. Keeps the history honest.
- **Error presentation.** `status="error"` responses render in red with the `error.code` badge; `error.message` goes to the output pane, `response.stderr` (usage lines) follows. No modal popups for command errors.

## 6. Viewport widget

- `QOpenGLWidget` subclass; uses the client's own GL context only to blit the received frame to the window. No Griz GL code runs on the client (invariant **I4**).
- Texture upload on frame receipt (network thread → decode thread → queued signal to UI thread → upload).
- Resize: on `resizeEvent`, throttle to 250 ms quiescence, then send `switch <w> <h>` (or equivalent viewport resize command) via worker. Server will eventually reply with a new frame at the requested size.
- Fallback rendering when disconnected or pre-first-frame: a neutral background with a small overlay label.

### 6.1 Input translation (CommandBridge)

Mouse and keyboard input are translated into Griz commands at the client:

| Input | Griz command | Notes |
|-------|--------------|-------|
| Left-drag | `rx <dy>` / `ry <dx>` | rate-limit to one command per ~50 ms during drag |
| Middle-drag | `tx <dx>` / `ty <dy>` | same |
| Scroll | `zoom <factor>` | one per scroll event |
| `R` key | `rview` | reset view |
| Click (no drag) | `pick <x> <y>` | [06-picking-and-queries](06-picking-and-queries.md) |
| Box-drag (with modifier) | `box_select ...` | [06-picking-and-queries](06-picking-and-queries.md) |

Rate-limiting during drags: the client issues one command at a time and coalesces queued deltas when the outstanding response hasn't returned. Matches the command FIFO discipline.

## 7. Selection UI

- `SelectionDock` displays the server-side selection: table of `{kind, id, metadata}` rows. Rows come from the `selection` part of `SessionState`.
- Highlight re-render happens server-side (the client just receives a new frame with the highlight applied); the client does no geometry math.
- Right-click row actions: "Hide material N", "Clear selection", "Export CSV". Each routes through the command bridge.

## 8. Persisted settings

- `$XDG_CONFIG_HOME/griz/hosts.toml`: [01-architecture](01-architecture.md) §3 defines schema.
- `$XDG_CONFIG_HOME/griz/sessions.toml`: reconnect hints.
- `$XDG_CONFIG_HOME/griz/ui.toml`: window geometry, dock layout, recent DBs, colormap preference.
- `$XDG_CACHE_HOME/griz/history.log`: command console history per host.
- `$XDG_CACHE_HOME/griz/known_hosts`: known-hosts shim used by the SSH driver.

File format: TOML for all config (human-friendly, hand-edit-safe). YAML reserved for data (results map).

## 9. Threading

[01-architecture](01-architecture.md) §6 pins three client threads. Mapping:

- **UI thread.** Qt event loop; all widget state. Never does network or decode work. Connects to `Worker`'s signals via `Qt::QueuedConnection`.
- **Network thread.** `Worker` lives here. Owns the `QTcpSocket`, the framing reader, the SSH child process handle. Emits queued signals for `response`, `event`, `frame`.
- **Decode thread.** Lives inside `Viewport` (or a dedicated `QThreadPool` runner). Consumes `kind=0x02` binary frames from a single-slot mailbox, decodes to `QImage`/`QOpenGLTexture`, posts to UI thread.

Queues between threads: `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` for envelope events (small, low-rate). A `QMutex`-protected single-slot mailbox with swap-on-write for frames.

## 10. Error presentation

| Class | Surface | Detail |
|-------|---------|--------|
| `status="error"` on command response | Inline in console + toast in status bar | No modal; the error log in the console is the history |
| `protocol_mismatch` during handshake | Modal | "Server is X, client needs ≥Y"; block until dismissed |
| SSH tunnel death / network thread read error | Modal + reconnect banner | Offer "Reconnect" and "Cancel session" |
| `session_ending(reason="slurm_walltime")` | Modal with countdown | Offer "Relaunch job" + "Save session and quit" |
| Server crash (EOF without `session_ending`) | Modal + tail of server log (via `ssh cat`) | Offer "Relaunch" |
| `state_overflow` sentinel | Silent (client refetches) | Log only |

## 11. Python → Qt translation table

For contributors building Qt on top of a known-good Python reference:

| Concept | Python (`pygriz`) | Qt client |
|---------|------|-----|
| Subprocess + handshake | `Worker.__init__` at `pygriz/src/griz/worker.py:57–138` | `net/Worker.cpp` |
| Background read loop | `Worker._reader_loop` `:149–172` | dedicated thread in `Worker` |
| Per-id correlation | `_responses_by_id` dict + `threading.Condition` | `QHash<QString, QPointer<ResponseWaiter>>` + `QWaitCondition` |
| Request timeout | `cmd(..., timeout=30.0)` `:274–309` | `QTimer` on outstanding waiter |
| Error mapping | `GrizCommandError(code, message)` `:304–308` | `GrizException` with `code`, `message` |
| Terminator + drain | `Worker.cleanup()` `:359–388` | `Worker::close()` with QProcess::waitForFinished |
| Session API surface | `Griz.field`/`.view`/`.time`/`.materials` | `SessionController` dispatching to same commands |
| Screenshot → PNG | `Griz.screenshot` `:139–180` + `_sgi.py` | same: `outrgb`, then in-memory SGI→PNG (or bypass once server emits PNG directly) |

The Qt client's network thread is essentially a C++ translation of `worker.py` with **length-framed** reads instead of `readline()`. Every other correlation/timeout/shutdown decision is already in Python — don't reinvent it.

## 12. Build and third-party

Full detail in [09-build-packaging-ci](09-build-packaging-ci.md). Short version:

- CMake top-level `client/CMakeLists.txt` independent of the autoconf `Src/` build.
- Third-party via Conan (preferred; easier cross-platform Qt fetch) or vcpkg (easier Windows). Pick one in [09-build-packaging-ci](09-build-packaging-ci.md).
- Platforms: Linux x86_64 and macOS (arm64 + x86_64) for v1; Windows in phase 3 per `../UI.md` §7.

## Open questions

- **Dockable vs. fixed layout.** Lean dockable (`QDockWidget`) — power users want control. Default layout bundled; reset-to-default menu item.
- **Theme:** follow OS theme via Qt's platform integration, with an optional bundled dark theme for dark-workflow folks. No light/dark forced on the user.
- **Persistent command bridge vs. transient.** The current design sends each menu click as its own Griz command (no batched "commit"). Any interactive workflow that needs atomic multi-command updates (e.g. "set three toggles then redraw") can use `;`-separated compound commands already supported by `parse_command()`.
- **Offline mode:** should the client open a DB *without* a server? Would require linking Mili; violates invariant **I4**. Recommend no — if users want offline, they can run `griz-server --transport=rpc` on the workstation (variant A in [01-architecture](01-architecture.md) §5).
- **Accessibility baseline.** Target WCAG AA contrast; keyboard navigation across all docks; font scale via Qt's high-DPI knobs. Deferred tuning to phase 2.
