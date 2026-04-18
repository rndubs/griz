# 04 — Client

## Scope

The Qt 6 native client: application structure, window layout, state management, command console, camera and selection input, how it consumes the protocol.

Out of scope: the protocol itself (see [02-protocol](02-protocol.md)), launch flow (see [07-launch-ssh-slurm](07-launch-ssh-slurm.md)), feature-level dialogs (see [08-feature-parity](08-feature-parity.md)).

## Related

- `UI.md` §3.2 (Client), §7 (Roadmap)
- [02-protocol](02-protocol.md), [05-rendering-and-streaming](05-rendering-and-streaming.md), [06-picking-and-queries](06-picking-and-queries.md)

## Sections to fill

- **Application skeleton.** Main window, document/session model, menu bar, status bar. Qt project layout (CMake targets, module boundaries).
- **Main window regions.** Viewport (displays streamed frames), command console, navigator/tree (materials, surfaces, results), time step control, status bar. Rough wireframe.
- **State model.** Client-side mirror of server state; how it's populated (initial snapshot + `StateEvent` stream); how UI widgets bind to it.
- **Command console.** Input history, autocomplete strategy (static keyword list vs. server-provided), output pane, ability to compose and send. Should mirror current Griz command behavior.
- **Viewport widget.** How streamed frames are decoded and blitted. Handling resize (client requests a new server viewport size). Mouse/keyboard capture and translation into camera and pick events.
- **Selection UI.** Visual affordance, selection list, metadata display, export.
- **Persisted settings.** Window layout, host profiles, recent sessions, color theme, keybindings.
- **Threading.** Network thread, decode thread, UI thread. Where frame → widget hand-off happens.
- **Error presentation.** Connection loss, server error, protocol version mismatch, SLURM job killed.
- **Accessibility basics.** Keyboard navigation, font scaling, high-DPI.

## Open questions

- Dock-able panels (Qt `QDockWidget`) vs. fixed layout?
- Should the command console and the GUI controls compete for state, or does the GUI always emit commands through the same channel the console does?
- Theming: follow OS theme, or ship a dedicated light/dark theme for consistency?
