# 11 — Migration

## Scope

How the project moves from "Motif GUI only" to "new client + server only" without breaking users in between. Covers coexistence, communication, training, deprecation, and eventual removal.

Out of scope: technical design of either side (covered elsewhere).

## Related

- `UI.md` §7 (Roadmap), §8 (Scope)
- [03-server](03-server.md), [08-feature-parity](08-feature-parity.md)

## Sections to fill

- **Coexistence window.** How long both frontends live in the tree. Build targets for each. Rule for adding features (must work in new client; old GUI is maintenance-only).
- **Per-phase user impact.**
  - Phase 1 release: new client is opt-in, old GUI is default.
  - Phase 2: new client is default, old GUI is available behind a flag.
  - Phase 3: old GUI removed.
- **Communication.** Release notes, internal announcements, LLNL user group email lists. Where to collect feedback during the transition.
- **Training.** Short how-to video or doc for the new launch flow, host profiles, and any UI reorganization. Aim for ~10-minute watch.
- **Script compatibility.** Existing Griz command scripts must continue to work unchanged. Test with a sample of real user scripts collected before Phase 1 cut.
- **Rollback plan.** What we do if Phase 2 default-switch breaks a critical workflow. Downgrade path to the Motif GUI for a release.
- **Deprecation schedule.** Specific versions / dates at which old GUI is (a) warned on startup, (b) removed from default build, (c) deleted from source.
- **Telemetry (optional, HPC-friendly).** Passive local counters for which features are used, to guide Phase 3 removal decisions. Anonymous, off-by-default if at all.

## Open questions

- Are users comfortable with Qt look-and-feel, or is there any institutional preference that would affect theming?
- What's the minimum support period between Phase 2 default-switch and Phase 3 removal? One release? Two?
- Who signs off on Phase 2 and Phase 3 transitions?
