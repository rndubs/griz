# 08 — Feature parity

## Scope

Inventory of what the legacy Motif GUI in `Src/gui.c` (9928 lines) exposes, mapped to its eventual place in the new Qt client. Goal: guarantee nothing important is silently dropped and prioritize v1 vs. later. This is an audit that expands in place as items are checked off; the table below is a starting frame, not a finished list.

Out of scope: implementation of any specific dialog (goes in its own follow-up doc once scheduled).

## Related

- `../UI.md` §7 (Phased roadmap), §8 (Scope)
- [04-client](04-client.md) — new UI structure (menu top-level, docks, console)
- [11-migration](11-migration.md) — coexistence of old and new UI while the audit closes out

## 1. Why this audit is shorter than it looks

A crucial observation: **most of `gui.c` is widget plumbing around commands, not command logic.** The commands themselves live in `Src/interpret.c` and are what the server already dispatches. This means:

- Anything a user achieves by typing a command today already works from the Qt client via [04-client](04-client.md) §5 (command console) and from MCP via `Griz.raw(...)`. No parity gap for the command surface — that's invariant **I8** in [01-architecture](01-architecture.md).
- The parity gap is specifically about **the Motif dialogs** that build command strings behind the scenes: material manager, result selector, surface manager, colormap editor, utility panel, viewing options, animation controls.

So: inventory the dialogs, not the commands.

## 2. Current state (2026-04)

- **Legacy GUI builds.** The `debug` / `opt` targets link `gui.c` and all the Motif widget code. Not affected by the server work.
- **Server builds (batch_opt, server_opt) skip `gui.c`.** The `-DGRIZ_SERVER_BUILD` gate and the `serial_batch_mode = TRUE` runtime flag (`Src/viewer.c:3156`) together guarantee no Motif code runs in the server. Confirmed by the clean link: `SERVER_OBJS` in `Src/Makefile.Library:241–285` does not list `gui.o`.
- **Motif calls inside engine code.** Per [03-server](03-server.md) §6, `popup_dialog` is handled; a pending audit (`grep -n 'XtV\|XmText\|XmCreate\|XtAppAddWorkProc' Src/interpret.c Src/results.c Src/draw.c Src/offscreen.c`) should confirm no reachable-in-server call slips through.

## 3. Audit approach

### 3.1 Extraction method

Every widget-construction block in `Src/gui.c` is produced by a `XmCreate*` or `XtVaCreateManagedWidget` call. A mechanical first pass:

```
grep -n 'XmCreate\|XtVaCreateManagedWidget\|XmCreateFileSelectionDialog\|xmPushButton' Src/gui.c
```

produces the skeletal inventory. Each hit gets classified with three columns:

1. **What command(s) does this widget emit?** Trace the widget's callback in `gui.c`; find the string it formats and hands to `parse_command()`.
2. **What engine state does it read?** Many widgets pre-populate from `analy->…` globals (material list, state range, current result name).
3. **Disposition in new UI.** Five buckets:
   - `v1` — ships in Phase 1.
   - `v2` — ships in Phase 2 (HPC integration / polish phase per `../UI.md` §7).
   - `console-only` — the command is dispatchable from the console; no dedicated Qt widget.
   - `drop` — obsolete / unused / explicit scope cut.
   - `replace` — new Qt widget with different UX, not a 1:1 port.

### 3.2 Table (frame)

Large tabular audit. Seed entries below; the rest fills in during implementation. When the table exceeds ~100 rows, split into `08a-motif-audit.md` per the original skeleton's note.

| Widget / dialog | Location (gui.c) | Command(s) emitted | Engine state read | Disposition | Notes |
|----------------|-----------------|--------------------|-------------------|-------------|-------|
| File → Open DB | menu + FileSelectionDialog | `load <path>` | — | **v1** | Dock-safe in Qt's `QFileDialog`. |
| File → Save image | menu + FileSelectionDialog | `outrgb <path>` (or `outpng` when enabled) | — | **v1** | Route through PNG in-memory per [05-rendering-and-streaming](05-rendering-and-streaming.md) §7.1. |
| File → Run script | menu | `rdhis <path>` | — | **v1** | Trivial. |
| File → Exit | menu | `quit` | — | **v1** | |
| Edit → Command history | text window | (none, reads `~/.grizhistN`) | history file | **v1** | Use the console's history; the hist file's role becomes a secondary export. |
| View → Reset view | menu | `rview` | — | **v1** | |
| View → Pan/Rotate/Zoom toggles | radio | `<mode>` commands | — | **v1** | Map to mouse-mode toggle in viewport. |
| Draw → Wireframe / Solid / Material | radio | `wire` / `solid` / `mat` | `render.mode` | **v1** | Dock toggle bound to `q_render` state. |
| Draw → Toggles (coord axes, time, cmap, minmax) | checkboxes | `ontime` / `oncmap` / `onminmax` / etc. | `render.toggles.*` | **v1** | Bound to state. |
| Select → Hilite element | dialog | `hilite ...` | mesh data | **v1** | Replaced by click-pick in viewport per [06-picking-and-queries](06-picking-and-queries.md). Numeric-entry form also preserved. |
| Select → Clear | menu | `clrhil`, `clear_selection` | selection | **v1** | |
| Animate → Play | controls | `anim ...` | `time.*` | **v1** | Bind to `TimeSlider` play/pause. |
| Material manager | big dialog (~800 LOC) | `hide`, `vis`, `disable`, `enable`, `mat`, `setmcol` | `materials[]` | **v1** (lean) + **v2** (color editor) | See §4 — standalone design item. |
| Surface manager | big dialog | `surf on/off`, `surf <opts>` | surface table | **v2** | Surfaces are second-priority users; many workflows don't touch them. |
| Colormap editor | dialog | `setcol`, `colmap` | `render.colormap` | **v2** | Default palettes in v1; editor in v2. |
| Utility panel | compile-time-optional dialog | misc `util_*` | — | **v2** | Off by default in v1; revisit demand. |
| Result field selector | dialog | `show <field>` | `results.*` | **v1** | Dock-resident per [04-client](04-client.md) §3. |
| Isosurface controls | dialog | `iso <opts>` | iso state | **v2** | |
| Traction controls | dialog | `traction <opts>` | traction state | **v2** | |
| Vector controls | dialog | `vec <opts>` | vec state | **v2** | |
| Threshold controls | dialog | `thresh <opts>` | threshold state | **v2** | |
| Clip planes | dialog | `clip <opts>` | clip state | **v1** (lean) | Common enough to warrant v1. |
| Reflection planes | dialog | `refl <opts>` | refl state | **v2** | |
| Free nodes | dialog | `free_nodes` | fn state | **v2** | |
| Explode view | dialog | `explode <factor>` | view state | **v2** | |
| Lighting | dialog | `light <opts>` | lighting state | **console-only** → **v2** | Rarely-touched; start in console. |
| Help → About | dialog | — | buildinfo | **v1** | Pull from `Src/buildinfo.c`. |
| Help → Command ref | static text | — | — | **v1** | Link to generated command list (see §5). |

Rows to add during the audit pass, not pre-filled here: all per-render-option widgets, extra file operations (`savtxt`, `savhis`), window-size controls, background-color picker, font / label settings, etc. Target: >90% of `gui.c`'s non-plumbing dialogs categorized before Phase 1 freeze.

## 4. Material manager (the hard one)

Calling it out early because it's the largest single dialog in `gui.c` and the most-used in the current workflow.

### 4.1 Current capabilities

- Per-material visibility toggle.
- Per-material enable toggle (participates in results calculation).
- Per-material color (RGB editor).
- Material label, element count (derived).
- Apply / reset / dismiss buttons.
- "All off / all on" bulk actions.

### 4.2 v1 Qt scope

A dock (`MaterialsDock`, [04-client](04-client.md) §3) rather than a modal. Table with one row per material, columns: `id`, `label`, visible toggle, enabled toggle, color swatch. Right-click: "Hide only this", "Show only this", "Isolate". Bulk-action buttons.

### 4.3 Data flow

- Read from `q_materials` + state events.
- Writes route through existing commands (`hide mat <id>`, `vis mat <id>`, `disable mat <id>`, `enable mat <id>`, `setmcol <id> <r> <g> <b>`). Invariant **I1**.

### 4.4 v2 extension

Color editor (HSV wheel, named palettes), save/restore named material-color schemes.

## 5. Command history and scripting

- **Console.** Replaces the Motif "command history" dialog. Same rolling history, persists across sessions, search/filter UI ([04-client](04-client.md) §5).
- **Script files.** `rdhis <path>` already works server-side; the File menu exposes "Run script…". Open question: should the client also offer a script recorder that dumps the session's commands to a file? Nice-to-have; v2.
- **`hist_fname`.** Existing per-session history file still writes on the server (`Src/viewer.c:3193–3201`). Primary UI gains nothing from it, but batch users lose nothing — files remain on disk.

## 6. Screenshots and annotations

- **Screenshots.** `outrgb` is the existing path; v1 extends with an in-memory PNG path (see [05-rendering-and-streaming](05-rendering-and-streaming.md) §7.1). Legacy `outrgb` / `outjpeg` / `outpng` commands keep working for script compatibility (invariant **I8**) — note that `outjpeg`/`outpng` are compiled out of the default server build (see `../../CLAUDE.md` Build section).
- **Animations.** `anim` still sweeps server-side; frame-dump path works. MP4 is v2 (see [05-rendering-and-streaming](05-rendering-and-streaming.md) §7.2).
- **Annotations** (text labels, legends): already rendered into the frame by `draw.c`. No client-side work needed unless we want editable labels in-client — then it's v2.

## 7. Keybindings

Current Motif accelerators in `gui.c`:

- `Ctrl-Q` exit, `Ctrl-O` open, `Ctrl-S` save image, etc. (Standard set.)
- F-keys bound to some view operations.

Qt client keeps all current accelerators (muscle memory) and documents them in a Help → Keybindings pane. New additions bound on unused keys only. Per-user rebinding is v2.

## 8. Standard menus — Qt layout

Proposed top-level menu structure for v1:

```
File     Open DB, Run Script, Save Image, Save Session, Quit
Edit     Copy console, Clear console, Preferences
View     Reset View, Mouse Mode {Rotate,Pan,Zoom}, Viewport Size, Toggle Docks
Draw     Mode {Wire,Solid,Material}, Options {Coord,Time,Cmap,Minmax},
         Clip Planes, Colormap
Select   Hilite by ID, Clear, Select by Material, Invert, Connected
Animate  Play, Step Forward, Step Back, Range, Speed
Host     Connect, Reconnect, Disconnect, Manage Hosts, Server Log
Help     Keybindings, Command Reference, About
```

Noting that this reorganizes slightly vs. the Motif menu bar: "Host" is new (the legacy GUI has no remote concept), the "Host" menu will absorb anything launch-related from [07-launch-ssh-slurm](07-launch-ssh-slurm.md).

## 9. Audit process

Practical steps for the audit in order of when they happen:

1. **Grep `gui.c` for widget-create calls.** Dump the set. ~200 entries expected.
2. **For each dialog**, locate its callback, trace the `parse_command()` call, read the command string and its argument handling.
3. **Cross-check against `interpret.c`.** The command grammar there is authoritative; any widget that can't be reproduced from a command is a widget that violates invariant **I1** and needs explicit handling.
4. **Populate the table.** Group by disposition (v1/v2/console/drop/replace).
5. **Estimate LOC per v1 dialog.** Used in the phase-1 plan in `../UI.md` §7.
6. **Identify "stealth" features** — things that happen only via the GUI and have no corresponding command. These are invariant **I8** risks; must gain a command in interpret.c before the Qt client's dialog can be built.

## 10. Usage data (want)

The main open question below: which Motif features are actually used? No logs today. Options:

- **Self-report.** Email a dozen power users with the current dialog list and ask "which do you use weekly / monthly / never?" Cheap, biased toward long-tail awareness.
- **Light telemetry.** If the Motif build gains a 2-line `wrt_text` that tags each dialog open, a week of real use produces a frequency histogram. Acceptable for internal use; not shipped.

Either improves the v1/v2 split and retires "replace" vs. "drop" judgment calls.

## Open questions

- **Which features are actually used?** (See §10.)
- **Any long-tail `interpret.c` commands with no GUI surface today** that deserve one? The client-side audit should note these as `new-dialog-candidate`.
- **Are there dialogs that warrant a full UX redesign** vs. a direct port? Material manager and colormap editor are strong candidates for redesign. Others: port first, iterate after user feedback.
- **Dialog modality.** The Motif UI leans hard on modals; the Qt client should prefer docks. Anything that truly must be modal (e.g. "confirm delete selection", file-open) stays modal.
