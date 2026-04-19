# 11 — Migration

## Scope

How the project moves from "Motif GUI only" to "Qt client + `griz-server` primary", with a known coexistence window, clear per-phase user impact, script compatibility throughout, and an explicit deprecation schedule. Covers communication, training, rollback, and the path to eventually removing `gui.c` from the tree.

Out of scope: technical design of either side (covered elsewhere); specific dialog-by-dialog mappings (see [08-feature-parity](08-feature-parity.md)).

## Related

- `../UI.md` §7 (Phased roadmap), §8 (Scope), §9 (User training risk)
- [03-server](03-server.md) §6 (decoupling `gui.c`), [08-feature-parity](08-feature-parity.md), [09-build-packaging-ci](09-build-packaging-ci.md)

## 1. Current state (2026-04)

The coexistence is already in place on the server side and partly demonstrated:

- **Legacy GUI** (Motif+X11): still builds from `Src/gui.c` and friends through the `debug`/`opt` targets. Untouched by the MCP work.
- **Legacy batch** (`griz4s.linux_opt_batch`): still the entry point for scripted batch jobs. Untouched.
- **Server** (`griz-server`): built under `-DGRIZ_SERVER_BUILD`; gates out `gui.c` entirely; powers the Python / MCP layer today. Active.
- **Qt client**: does not exist yet.

So the migration starts from a known-good three-way state: GUI build, batch build, server build — all coexisting via compile-time gates. No code has been removed.

## 2. Per-phase migration

Anchored on `../UI.md` §7:

### 2.1 Phase 0 (where we are)

- Server infrastructure shipped. Python / MCP layer consuming it.
- Motif GUI default for human users on workstations/login nodes.
- **User impact: none.**

### 2.2 Phase 1 — "new client opt-in, old GUI default"

- First release of `griz-client` for Linux + macOS.
- New command in the run-support path: `griz-client` binary opens the host dialog and connects.
- Legacy `griz` binary unchanged. Users keep typing `griz file.plt` in their SSH session and get the Motif UI.
- **User impact:** optional. Nothing breaks. Users can try the new client for feedback.
- **Gate to exit phase 1:** stable client + server, documented launch, feature-parity on the v1 subset in [08-feature-parity](08-feature-parity.md).

### 2.3 Phase 2 — "new client default, old GUI behind a flag"

- Install scripts at LLNL sites default `PATH` to `griz-client`.
- Motif GUI still available via `griz --legacy-ui` or an explicit `griz-gui` alias.
- SLURM integration in client live; compute-node sessions the preferred path.
- **User impact:** workflow change. Users who double-click or alias `griz` now launch the new client. Documentation and training ramp up.
- **Gate to exit phase 2:** six months of stable phase 2 usage OR explicit signoff (see §7).

### 2.4 Phase 3 — "Motif GUI removed"

- `Src/gui.c` and Motif dependencies deleted from the build.
- Legacy `griz` binary reduced to `griz_batch` (already exists) for script users.
- Windows client ships.
- **User impact:** final. Power users who avoided phase 2 migration get a last push.
- **Gate to enter phase 3:** see §7 and §10.

## 3. Script compatibility — the non-negotiable

Invariant **I8** and `../UI.md` §10 success criterion: **existing Griz command scripts must continue to work unchanged through every phase**, including phase 3.

Mechanism (already in place):

- `griz-server` reuses `parse_command()` verbatim. Every command that worked in the legacy batch binary works in the server today (see `Src/viewer.c:3256` — `parse_command(cmd_buf, analy)`).
- New commands added for the Qt client are additive; no rename of existing commands. Commands the Qt client wants but can't express yet (e.g. `pick_at`) don't break scripts because scripts never used them.
- The legacy `griz_batch` / batch_opt binary stays functional for headless script runs, if a user wants `-b script.hist` rather than piping commands to `griz-server --transport=stdio`.

**Regression test:** a corpus of real user command scripts, collected ahead of phase 1, run as part of the end-to-end suite ([10-testing](10-testing.md) §8). This pair explicitly checks "does every command in every collected script still succeed against the server?"

## 4. Coexistence rule

While both front-ends live in the tree:

- **All new visible features** target the Qt client + server. The Motif GUI is maintenance-only.
- **Bug fixes** that land in engine code (`Src/interpret.c`, `Src/draw.c`, `Src/results.c`) automatically benefit both front-ends — that's the whole point of sharing the engine.
- **Bug fixes that are GUI-specific** (Motif widget bugs): fix in the Motif GUI only if the bug is actively blocking users; otherwise document as a known issue and let phase 3 retire it.
- **Rule of thumb:** "Can the new client already do it?" → do nothing on Motif. "Has Motif broken in a way that blocks real work today?" → narrow fix; prefer blog-post-sized changes over refactors.

## 5. Communication

- **Release notes.** Every release during phase 1 and 2 documents what moved, what's new in the client, and any compatibility-relevant details.
- **Internal announcements.** LLNL user-group email list: one announcement per phase boundary, plus a preview of phase 2 a few weeks before the default switches.
- **Feedback collection.** Dedicated channel (Slack or internal tracker) during phases 1 and 2. Explicit ask: "try the new client, report what's missing."
- **Docs.** Refresh the repo-root `README.md` and top-of-tree docs as phases advance. The existing `CLAUDE.md` and `planning/` stay accurate; they're living.

## 6. Training

Target: zero-prep for a working-client user, <10 minutes for a long-time Motif user.

- **Short video** (phase 2): "From Motif to the new Griz client in 5 minutes." Covers launch dialog, host profile, menu changes, material manager location, command console. Host on an LLNL-internal site.
- **Written quickstart** in `Doc/`: the new client's host-profile format, common tasks ("how do I open a DB", "how do I pick an element"), and a Motif→Qt menu map.
- **Office hours** during phase 2 rollout: 30 minutes weekly for the first month.

## 7. Rollback plan

If phase 2 default switch breaks a critical workflow:

- **Short-term (same release):** announce the known issue; instruct users to run `griz --legacy-ui` or `griz-legacy` until the fix ships.
- **Medium-term (next patch):** fix forward. If the fix is too slow, **revert the default-switch** in the installer; Motif GUI stays default for another release.
- **Last resort:** one-release holdback is acceptable. Two-release holdback requires an explicit decision by the owning engineer + code owner.

No "rollback to the previous major version" — script users don't need it (scripts still work), and GUI users have the fallback flag.

## 8. Deprecation schedule

Concrete calendar once phase 2 ships:

| Event | Trigger | Notice |
|-------|---------|--------|
| Deprecation warning on startup | Phase 2 release | "This GUI is deprecated; Griz will default to the new client starting v X.Y. Run `griz --legacy-ui` to keep the old GUI." |
| Old GUI requires explicit flag | Phase 2 + 1 release | `griz` alone opens the new client; `griz --legacy-ui` opens Motif. |
| Old GUI removed from default install | Phase 2 + 2 releases (min 6 months) | Motif binary still buildable from source; not packaged. |
| Motif code deleted from source tree | Phase 3 | `Src/gui.c` and Motif deps removed; only `griz_batch` + `griz-server` + `griz-client` remain. |

Minimum support period between phase-2 default switch and phase-3 removal: **two releases**. This gives straggler users two release cycles to migrate.

## 9. Telemetry (optional, HPC-friendly)

Passive local counters to guide phase-3 timing:

- **What:** per-command hit counter written to `~/.griz/stats/<yyyy>-<mm>.toml`. Counts dialog opens and command executions. Anonymous; no machine-sent data.
- **How sampled:** on session close, dump counters to disk. Can be volunteered to a central drop folder manually.
- **Why:** answers "are any dialogs still used only via Motif?" before phase 3 removes them.
- **Off by default** to respect HPC sensibilities; opt-in via a settings checkbox in the Motif build. Never implemented in the Qt client (Qt client usage is sufficient signal that the user migrated).

Entirely optional; phase-3 can proceed without it if a direct user survey (§10) is considered enough.

## 10. Phase 3 gates — explicit

Before phase 3 (removal):

- [ ] Client feature parity for all `v1`-and-`v2` dialogs in [08-feature-parity](08-feature-parity.md) §3.
- [ ] At least one release cycle with `griz --legacy-ui` as the only path to Motif.
- [ ] User survey or telemetry confirming <5% of active users still run `--legacy-ui` in the last month.
- [ ] Explicit signoff by the Griz code owner and at least one external power user.
- [ ] Documentation fully updated to drop any Motif references.

Before phase 2 default switch:

- [ ] Phase-1 client shipped on Linux + macOS.
- [ ] End-to-end tests ([10-testing](10-testing.md) §8) green for a sustained period (e.g. one month of nightly runs).
- [ ] Training assets ready (§6).
- [ ] Rollback path verified (§7).
- [ ] At least one large site (LLNL or similar) opts in as the phase-2 pilot.

## 11. Phase 3 and MCP

Note how the MCP effort fits into the transition:

- MCP does not depend on the Motif GUI. It's stdio against `griz-server`. Phase 3 changes nothing for MCP.
- If anything, phase 3 *helps* MCP users by removing a linkage to unused Motif/X11 libraries on server installs.

## Open questions

- **Windows phase.** Does Windows land in phase 3 or phase 4? `../UI.md` §7 says phase 3; if phase 2 takes longer, a separate phase 3 = Windows-only may be cleaner. Decide closer to phase-2 exit.
- **User acceptance of Qt look-and-feel.** Mostly a question of theming (see [04-client](04-client.md) §12). Survey at phase-1 release.
- **Sign-off.** Which two roles specifically? Suggest: Griz engineering lead + one LLNL power-user representative. Pin at phase-2 kickoff.
- **Keeping the Motif build after phase 3.** Option: keep `gui.c` in a named branch for forensic reference, delete from `master`. Lightweight and gives determined users a way back. Low cost.
