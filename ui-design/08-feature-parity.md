# 08 — Feature parity

## Scope

Inventory of everything the current Motif GUI exposes, mapped to its place in the new client. The goal is to make sure nothing important is silently dropped and to prioritize what lands in v1 vs. later.

Out of scope: implementation details of any one feature (go in the relevant doc).

## Related

- `UI.md` §7 (Phased roadmap), §8 (Scope)
- [04-client](04-client.md)

## Sections to fill

- **Audit of `Src/gui.c`.** Enumerate every widget-construction block: menus, dialogs, panels, popups. For each:
  - Name and location in the old GUI.
  - Command(s) it generates.
  - Engine state it reads or writes.
  - Status in new UI: in v1 / in v2 / dropped / replaced by command console only.
- **Standard menus.** File, View, Draw, Select, Animate, etc. Re-lay out for Qt idioms without surprising long-time users.
- **Material manager.** Large, complex dialog. Call out as a standalone design item if needed.
- **Surface manager.** Same.
- **Utility panel.** Compile-time optional today. Decide default state in new UI.
- **Command history and scripting.** How prior command entry and loading of command files is preserved.
- **Screenshots, movies, and annotations.** Current paths and how they map forward.
- **Keybindings.** Audit existing accelerators; preserve or consciously change.

## How to structure the audit

Recommend a large table in this doc or a sibling `08a-motif-audit.md` with columns:

| Widget / dialog | Current command(s) | Engine touch points | v1 / v2 / drop | Notes |
|---|---|---|---|---|

## Open questions

- Which features are actually used vs. nominally present? Any usage data?
- Any long-tail commands in `interpret.c` with no GUI surface today that should get one?
- Which dialogs warrant redesign vs. a direct port?
