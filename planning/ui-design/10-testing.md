# 10 — Testing

## Scope

Test strategy across the whole stack: unit tests on each side, protocol conformance, full integration tests with a live server, and HPC-realistic end-to-end tests with representative datasets.

Out of scope: build/CI plumbing (see [09-build-packaging-ci](09-build-packaging-ci.md)).

## Related

- `UI.md` §9 (Risks)
- [02-protocol](02-protocol.md), [03-server](03-server.md), [04-client](04-client.md)

## Sections to fill

- **Existing test suite.** What `Src/test` covers today. Which tests can be reused against the server as-is.
- **Server unit tests.** Command parsing, state event emission, picking correctness, frame encoding. Run without OSMesa where possible.
- **Client unit tests.** State model, command-console behavior, host-profile parsing, input translation.
- **Protocol conformance.** A dedicated test that runs real client against real server across representative message flows. Useful for catching regressions when the protocol evolves.
- **Integration tests.** Launch a local server + headless client (Qt in offscreen mode), connect over loopback, run a script of commands, assert state and screenshots.
- **Golden-image rendering tests.** Small canonical datasets rendered through the server; compare frames against checked-in references with tolerance.
- **End-to-end on HPC.** A small suite that actually launches through SSH+SLURM against a dev cluster. Runs on a slower cadence — nightly or pre-release.
- **Performance regressions.** Tracked rendering time on large mesh fixtures. Latency tracked on a representative WAN link (or a simulated one via `tc`).
- **Test data.** Where canonical Mili fixtures live; size budget.
- **Flaky-test policy.** HPC-touching tests will be flaky; quarantine rules and on-call expectations.

## Open questions

- Do we have a small, fast Mili dataset suitable as a fixture? If not, create one.
- Is there a dev cluster available for automated SSH+SLURM tests, or do we need to mock SLURM?
- Golden-image tolerance policy — what's acceptable drift across GL / OSMesa versions?
