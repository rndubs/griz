# 10 — Testing

## Scope

Test strategy across the stack: server-side unit + integration tests (leveraging what the MCP effort already shipped), Qt client unit tests, protocol-conformance tests, end-to-end tests against real HPC, golden-image rendering, and performance regression tracking.

Out of scope: the build / CI machinery that runs all this (see [09-build-packaging-ci](09-build-packaging-ci.md)).

## Related

- `../UI.md` §9 (Risks)
- [02-protocol](02-protocol.md), [03-server](03-server.md), [04-client](04-client.md)

## 1. Current state (2026-04)

Useful inheritance from the MCP effort:

| Suite | Location | Count | What it proves |
|-------|----------|-------|----------------|
| Python worker unit tests | `pygriz/tests/test_worker.py` | 6 | Subprocess spawn, `cmd()` roundtrip, handshake, timeouts, error mapping. Runs without `griz-server` via fakes for most cases. |
| Results-map unit tests | `pygriz/tests/test_results_map.py` | 51 | YAML loading, `(field, component) → griz_name`, aliases, scalar fields, env-var override. |
| MCP smoke tests (end-to-end) | `pygriz_mcp/tests/test_smoke.py` | 14 | MCP client → `griz-mcp` → `griz.Griz` → Worker → `griz-server`, against `bar71.pltA`. Skips automatically if the binary isn't built. |
| MCP tool unit tests | `pygriz_mcp/tests/test_tools.py` | 15+ | Per-tool validation against injected test sessions. |
| MCP session-lifecycle tests | `pygriz_mcp/tests/test_session.py` | ~5 | Module-level singleton, factory injection. |

**Legacy C test layout** in `Src/test/` — covers parts of Griz's I/O and compute pipeline. Most of these remain valid for both the batch binary and the server binary (same engine). Not rebuilt for the UI effort; reuse as-is.

**No Qt / client tests yet.** Nothing exists to test.

**Test data:** `bar71.pltA` Mili DB referenced by the smoke tests. Real file, small (good for CI).

## 2. Target test pyramid

```
                +-------------------------+
                |  End-to-end on HPC     |  slow (minutes), nightly
                |  (SSH+SLURM against    |
                |   real cluster)        |
                +-------------------------+
              +-----------------------------+
              |  Integration (CI)           |  minutes, per-commit on main
              |  server+client on loopback  |
              +-----------------------------+
            +---------------------------------+
            |  Golden-image rendering          |  seconds
            +---------------------------------+
          +-----------------------------------+
          |  Protocol conformance               |  seconds
          +-------------------------------------+
       +---------------------------------------+
       |  Server & client unit tests              |  ms
       +------------------------------------------+
```

## 3. Server unit tests

### 3.1 What's easy to unit-test in C

- `server_parse_request()` — JSON vs. raw detection, malformed JSON handling. Pure function; no subprocess. Set up with `cJSON_Parse` on test input, assert `ServerRequest` fields.
- `server_emit_response()` / `server_emit_error()` / `server_emit_data_response()` — redirect stdout to a buffer, invoke, parse the result, assert JSON shape.
- `server_try_hello()` — table of hello strings → expected ack.
- `server_record_error()` / `server_peek_error()` — sequence of popups → expected code per taxonomy.
- `build_q_state_data()` / `build_q_view_data()` / etc. — against a minimal `Analysis *` fake or a real loaded DB.
- (Future) `notify_state()` coalescer — sequence of key writes → expected emitted event.

Framework: a tiny xunit.c-style runner added under `Src/test/server/`. Invoked from `Src/Makefile.Library` as a `make test` target. No heavy test-framework dep.

### 3.2 What's not unit-tested

- `parse_command()` itself — too large and too stateful; exercised through integration.
- OSMesa / `draw.c` paths — exercised through golden-image tests (§6).

## 4. Protocol conformance

Goal: pin the envelope shape so future protocol evolution is intentional.

Implementation approach: **reuse the smoke suite**. `pygriz_mcp/tests/test_smoke.py` already round-trips the full stack; rename/extend as `tests/conformance/` and add:

- A hello/ack matrix (version range pairs, expected acks / errors).
- Error-taxonomy coverage — one case per `error.code`.
- Query-command payload schemas — assert every `q_*` returns keys matching the schema in [`../shared/query-commands.md`](../shared/query-commands.md).
- State-event sequence assertions (once `state_changed` emission lands).

Run it against **both transports** once RPC lands: same test harness points at either `--transport=stdio` or `--transport=rpc`. Parity suite is the primary guard against transport drift.

## 5. Client unit tests

Once `client/` exists (see [04-client](04-client.md) §2.1):

| Target | What's tested |
|--------|---------------|
| `client/tests/net/` | Framing reader/writer, mock-socket round-trips, handshake state machine, timeout, error propagation. Framework: Qt Test or GoogleTest — pick one in the CMake setup. |
| `client/tests/model/` | `SessionState` merge semantics (incoming `state_changed` → correct signals emitted), `ResultsMap` loader, `HostProfiles` TOML parse/save. |
| `client/tests/ui/` | Widget behavior via Qt Test (offscreen): console autocomplete, materials-dock drag-to-reorder, time-slider binding. High-ROI set; skip exhaustive UI automation. |
| `client/tests/commands/` | Input event → Griz command string translation (e.g. "dx=5 → `rx 5`"). |

Target: unit tests run under 30 s on any CI runner.

## 6. Golden-image rendering

The trickiest category.

- **Fixture.** `bar71.pltA` + a small deterministic command file (`test_render.hist`). Command file exercises a known camera, time state, render mode, and material set.
- **Capture.** Server runs the command file, emits an `outrgb` at the end. Frame is compared pixel-wise against a checked-in reference under `Src/test/golden/`.
- **Tolerance.** Per-pixel RGB diff with a `≤2` tolerance per channel (OSMesa has small cross-version jitter). Structural similarity (SSIM) floor as a secondary gate for larger meshes.
- **Maintenance.** When draw.c changes legitimately alter output, update the reference in a discrete PR with the commit message explaining the visual change. No mass-regenerate.

Golden-image tests run on the TOSS runner (OSMesa is available there and consistent). Skipped on GitHub Ubuntu unless OSMesa is specifically installed.

## 7. Integration tests

Loopback / all-in-one:

- Launch `griz-server --transport=rpc` locally on `127.0.0.1:0`, read port from the rendezvous file.
- Launch Qt client in offscreen mode (`QT_QPA_PLATFORM=offscreen`) pointing at `127.0.0.1:port`.
- Run a script of commands via the client; assert client-side `SessionState` ends in the expected shape and specific `state_changed` events were observed.

Framework: pytest harness driving both processes (Python is easier here than making Qt its own test driver). Reuse the `pygriz` worker as the test-oracle of what the server said.

## 8. End-to-end on HPC

Once SSH+SLURM launch lands (see [07-launch-ssh-slurm](07-launch-ssh-slurm.md)):

- A minimal suite that actually provisions a SLURM job, reads the rendezvous, establishes the tunnel, sends a handful of commands, asserts the responses, closes cleanly, verifies the job ended.
- **Runs nightly**, not per-commit. HPC access costs; we don't need per-commit signal from here.
- Lives in `tests/hpc/` with a pytest marker `@pytest.mark.hpc` that CI honors.

Site dependency: one dev cluster with a service account that can submit/cancel jobs. Until that's available, skip.

## 9. Performance regression tracking

- **Render-time** (server): time `analy->update_display(analy)` on a large fixture at a reference camera. Benchmark; fail if >10% regression from prior tag.
- **Encode-time** (server): JPEG-encode a reference RGBA buffer. Benchmark with libjpeg-turbo.
- **Pick latency** (server): point-pick against cached ID buffer. Single-digit ms target.
- **WAN RTT** (client): measurement-only telemetry under `tc netem`-simulated 80 ms link; assert interactive drag feels responsive. Manual phase-0 measurement first, automated post-phase-2.

No strict budget yet — establish baselines before gating CI on them.

## 10. Test data

- `Src/test/data/bar71.pltA` — existing fixture, small. Good for smoke + conformance.
- `Src/test/data/large_mesh.pltA` — not yet shipped. Candidate for golden-image and perf tests; size budget ~500 MB (still repo-friendly via git LFS if needed).
- `Src/test/cmdfiles/*.hist` — deterministic command sequences. Grow as needed.

## 11. Flaky-test policy

HPC-touching tests (end-to-end, optionally network-bound integration) are inherently flakier than bench tests:

- **Quarantine.** A flaky test tagged with `@pytest.mark.flaky` counts as a warning, not a red build. Quarantined tests get a tracking issue and are either fixed or removed within two weeks.
- **On-call.** No 24/7 coverage; failures in nightly HPC runs get triaged the next business day by the on-duty Griz dev.
- **Retry.** Hard no for pre-merge tests. Allowed once on nightlies.

## Open questions

- **Small fast Mili fixture.** `bar71.pltA` covers most cases. Do we need a vector-dominated / tensor-dominated / multi-mesh fixture too? Low-priority; create on demand.
- **Dev cluster availability.** Depends on LLNL HPC provisioning. If unavailable, consider a containerized slurm-single-node setup for end-to-end tests.
- **Golden-image drift.** How often OSMesa versions bump in practice. Audit after first six months.
- **Qt Test vs. GoogleTest.** Qt Test is better at widget-level tests; GoogleTest is nicer for non-Qt C++ (`net/`, `model/`). Mixing is fine — use Qt Test where it reads cleaner.
