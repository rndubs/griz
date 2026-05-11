# 09 — Build, packaging, CI

## Scope

How the new code is built, tested in CI, and shipped. Covers the server's existing autoconf integration (shipped — the `server_opt` target already lives in `Src/Makefile.Library`), the Qt client's CMake build (new, not yet scaffolded), how the two share artifacts, and packaging per platform.

Out of scope: test content (see [10-testing](10-testing.md)); release messaging (see [11-migration](11-migration.md)).

## Related

- `../UI.md` §7 (Roadmap gates at Phase 1 and Phase 3)
- [03-server](03-server.md), [04-client](04-client.md)
- Project build doc: repo-root `CLAUDE.md` § Build

## 1. Current state (2026-04)

### Server build

Already shipped and in daily use:

| Piece | Where |
|-------|-------|
| Entry point | `./build.sh [batch\|server\|all] [-- <configure args>]` at the repo root. Drives autoconf + gmake. |
| Configure | `Src/configure` (generated; regen with `autoconf -f` after `configure.ac` edits) |
| Library rules | `Src/Makefile.Library`. The `server_opt` target is at `:241–285`. |
| Compile gate | `-DGRIZ_SERVER_BUILD` added to server-only `CPPFLAGS` (`Src/Makefile.Library:265`). |
| Object set | `SERVER_OBJS = $(OBJS) server_main.o server_core.o cJSON.o` (`Src/Makefile.Library:269`). |
| cJSON vendored | `Src/ext/cJSON/cJSON.{c,h}`. Linked into server target only. |
| Output | `Src/GRIZ4-*/bin_server_opt/griz-server` — confirmed present in the current tree. |
| Default flags | `--enable-nojpeg --enable-nopng` (per repo-root `CLAUDE.md`); disables `outjpeg`/`outpng` at runtime. Pass alternate flags after `--` to `build.sh`. |
| OSMesa / Mili / Motif | System paths under `/usr/apps/mdg` and `/usr/lib64`; expected on LLNL TOSS hosts. Compiler module loaded externally (e.g. `intel-classic/2021.6.0-magic`). |

**Unshipped (but in scope for UI):** client build, CI matrix, packaging, release signing, shared-version propagation.

### Python package builds

The `pygriz/` and `pygriz_mcp/` trees build via `uv` (Astral):

- `cd pygriz_mcp && uv sync --extra test` installs `llnl-griz-mcp` + `llnl-griz` (editable).
- Tests: `uv run pytest tests/test_smoke.py` (see [10-testing](10-testing.md)).
- Wheels: `uv build` under each project. Data files (`Src/data/results_map.yaml`) are included via `importlib.resources` on `griz.data`.

No Python CI / wheel publishing yet.

## 2. Server build — future-facing changes

As [03-server](03-server.md) §9 lands:

1. New source files `Src/server_stdio.c`, `Src/server_rpc.c`, `Src/server_core_startup.c`, `Src/server_query.c`, `Src/server_events.c`. Each appended to `SERVER_OBJS` in `Src/Makefile.Library`.
2. New optional dependencies:
   - `libjpeg-turbo` (for in-memory JPEG encoding, [05-rendering-and-streaming](05-rendering-and-streaming.md) §4.1) — look up via `pkg-config` in `configure.ac`, guard with `HAVE_LIBJPEG`.
   - `libx264` or equivalent (for H.264, phase 2) — same pattern.
3. No new runtime deps in v1 beyond libjpeg-turbo if we enable in-memory JPEG.
4. `--transport=rpc` gets compiled only if a platform TCP option is available; guard with `HAVE_RPC_TRANSPORT`. This lets MCP-only builds on locked-down hosts ship without RPC bits (open question in [`../shared/server-binary.md`](../shared/server-binary.md)).

## 3. Client build — new scaffolding

### 3.1 Layout

Sibling to `Src/`, per [04-client](04-client.md) §2.1:

```
client/
  CMakeLists.txt                  # top-level
  cmake/                          # FindQt.cmake etc.
  src/{main.cpp, App.*, net/, model/, ui/, commands/}
  resources/
  tests/
  CMakePresets.json               # per-platform presets
  conan/conanfile.py              # Conan recipe (or vcpkg.json)
```

No autoconf, no integration with `Src/Makefile.Library`. The two builds are orthogonal.

### 3.2 Toolchain

- **Compiler.** Linux: GCC 11+ or Clang 14+. macOS: Xcode 14+. Windows: MSVC 2022.
- **Qt 6.5+.** LTS-line; 6.5 is the current LTS as of the cutoff. Fetched via Conan's `qt/6.5.x` recipe on Linux and Windows; via the Qt installer on macOS for the notarizable distribution bundle (Conan's macOS Qt builds are not notarization-safe).
- **Build system.** CMake 3.22+ (matches Qt 6's minimum). Presets keep the platform dance legible:
  ```
  cmake --preset linux-release
  cmake --build --preset linux-release
  ```

### 3.3 Third-party choice

**Recommend Conan for Linux + Windows, `brew` / `vcpkg` for macOS.** Rationale:

- Conan recipes for Qt and libssh2 are well-maintained.
- macOS codesigning pipelines are easier on Homebrew-installed Qt, or Qt's official bundle.
- Switch to vcpkg only if Conan integration creates friction.

Third-party list (v1):

- Qt 6 (Core, Widgets, Gui, Network, OpenGLWidgets, Concurrent).
- yaml-cpp (for `results_map.yaml`).
- libssh2 (phase-3 embedded fallback; not needed v1 if system SSH is used).
- (Phase 2) `ffmpeg` / libavcodec for H.264 decode.

### 3.4 In-repo vs separate

Keep `client/` **in the same repo** as `Src/`. Shared `VERSION` file (repo root), shared `results_map.yaml` under `Src/data/`, joint tagging. A separate repo invites "client v X.1 against server vX.0" drift and makes cross-cutting refactors painful. The Qt client is also small (~a few kLOC initially) compared to the C core.

## 4. Shared code and versioning

### 4.1 Shared data

- `Src/data/results_map.yaml` — consumed by both the Python package (via `importlib.resources`) and the Qt client (copied into `client/resources/results_map.yaml` at build time; future option to generate a `results_map.h` for server-side human-readable name emission per [`../shared/results-map.md`](../shared/results-map.md) § Server).
- `Src/ext/cJSON/` — C only, server-side; the Qt client uses Qt's JSON classes.

### 4.2 Shared schema

The JSON envelope lives in `Src/server_core.h` comments (normative) and [`../shared/command-protocol.md`](../shared/command-protocol.md) (human docs). No `.proto` files or schema repo. If schema definition becomes painful enough, a code-generated `nlohmann::json` ↔ Qt class may help; don't solve this preemptively.

### 4.3 Version

One `VERSION` file at the repo root. Contents: `{major}.{minor}.{patch}[-suffix]`. Both builds read it:

- `Src/Makefile.Library`: `GRIZ_VERSION := $(shell cat ../VERSION)` — feeds `-DGRIZ_VERSION=\"$(GRIZ_VERSION)\"` into both batch and server builds. `server_emit_ready()` already emits this as `version`.
- `client/CMakeLists.txt`: `file(READ ${PROJECT_SOURCE_DIR}/../VERSION GRIZ_VERSION)`. Sets `PROJECT_VERSION`, surfaces in About box and the hello envelope.

Compat policy: **client and server pinned to the same version through phase 1**. No N-1 support window; the `hello_ack` handshake will refuse a mismatched major (`Src/server_core.c:465–519` already implements this). Once the product stabilizes in phase 2, relax to major-match, minor-compat.

Protocol version (`"1.0"` at `Src/server_core.c:21`) is a separate string, increments independently of the product version — see [02-protocol](02-protocol.md) §5.

## 5. CI matrix

### 5.1 Runners

| Platform | Runner | What runs |
|----------|--------|-----------|
| Linux x86_64 (Ubuntu 22.04) | GitHub Actions `ubuntu-22.04` | Server opt build (with OSMesa installed), Python tests (pygriz + pygriz_mcp), client opt build + unit tests. |
| macOS arm64 | `macos-14` | Client opt build + unit tests. No server build on macOS unless/until variant A in [01-architecture](01-architecture.md) §5 is supported. |
| macOS x86_64 | `macos-13` | Same as arm64 for coverage. Drops when arm64 usage dominates. |
| Windows x86_64 | `windows-2022` (phase 3) | Client opt build + unit tests. |
| LLNL internal | TOSS runner (GitLab?) | Server builds against real Mili in `/usr/apps/mdg`; integration smoke against `bar71.pltA`. |

The LLNL runner is where server tests run against a representative Mili install; GitHub's Ubuntu runners don't have Mili, so server tests there either mock it or skip.

### 5.2 Jobs

- `build-server` — `./build.sh server` on the TOSS runner; fails on any compile error; archives `griz-server` artifact.
- `test-python` — `uv run pytest` on `pygriz/` and `pygriz_mcp/`. Skips smoke tests if `griz-server` is absent (matches current test design).
- `build-client-linux` / `-macos` / `-windows` — Qt + CMake. Each archives its build tree.
- `test-client` — unit tests in `client/tests/` (per module).
- `integration-smoke` — the 14-test smoke suite from `pygriz_mcp/tests/test_smoke.py` against the newly-built server artifact. Existing; reuses as-is.
- `lint-yaml` — lint `Src/data/results_map.yaml` for uniqueness of primary keys / aliases (new, see [`../shared/results-map.md`](../shared/results-map.md)).
- `protocol-conformance` — when it exists ([10-testing](10-testing.md) §4).

### 5.3 Caching

Conan / CMake build-dirs cached per commit-base (`actions/cache`). Qt installs cached aggressively — fetching Qt is slow. Python deps cached by `uv.lock`.

## 6. Packaging

### 6.1 Linux (server)

- **Tarball** of `griz-server` + required shared libs. Distributed via the LLNL software installation path (under `/usr/apps/mdg` or similar; site-specific).
- Rebuild against each TOSS compiler module the user base runs on.

### 6.2 Linux (client)

- Tarball with `bin/griz-client`, `lib/` (bundled Qt), `share/griz/` (icons, `results_map.yaml`).
- Optional RPM/DEB if a site requests. Low priority.

### 6.3 macOS (client)

- `.app` bundle. Bundled Qt frameworks, relocated with `macdeployqt`.
- Code-signed with Developer ID (LLNL account required).
- Notarized via `notarytool`.
- Distributed as `.dmg` with a staple.

### 6.4 Windows (client, phase 3)

- MSI via WiX.
- Authenticode signed (LLNL account required).
- Bundled MSVC runtimes + Qt DLLs.

### 6.5 Python packages

- Wheels for `llnl-griz` and `llnl-griz-mcp` published to the LLNL internal PyPI (or `pip install git+…` directly until then).
- No binary extension (pure Python); single wheel per release.

## 7. Release cadence

- **Tagged releases** of the monorepo. One tag produces: server binaries (per TOSS variant), client installers (per platform), Python wheels.
- **Compatibility matrix** through phase 2 onward: server `X.Y.z` accepts clients `X.Y-2.*` to `X.Y.*` (two-minor backward window). Pre-phase-2: strict same-version.
- **Protocol versions** increment separately as per [02-protocol](02-protocol.md) §5.

## 8. Signing and secrets

- Apple Developer ID cert for macOS: held by LLNL release team. Signing job runs on a dedicated macOS runner with the cert in Keychain (encrypted, not in repo).
- Code-signing cert for Windows: same pattern.
- GPG signature on the Linux tarball: optional, low priority.
- No secrets in the repo. GitHub Actions secrets or LLNL-equivalent for signing keys.

## 9. Developer workflows

Local dev loops the project wants to keep fast:

- **Server-only iteration.** `./build.sh server` after source edits; the autoconf+gmake graph incremental-builds on `.c` changes. Edit → build → `uv run pytest tests/test_smoke.py -k <pattern>`.
- **Python iteration.** `uv run pytest` no server rebuild needed unless envelope changes.
- **Client iteration.** `cmake --build --preset linux-debug` + `ctest`.
- **Variant A (local all-in-one).** Developer runs `griz-client --local --db <path>`; the client spawns `griz-server --transport=rpc` as a child process on `127.0.0.1`. Same code path that SSH+SLURM users hit, only without the SSH part. Critical for iterating on UI without a cluster.

## Open questions

- **Conan vs. vcpkg.** Defaulting to Conan; reconsider on Windows if Conan-Qt friction bites.
- **LLNL internal artifact repo.** Does one exist for Linux packages? If yes, RPM builds become priority.
- **macOS notarization owner.** Someone on the team needs an Apple Developer Enterprise account or individual account under LLNL's umbrella. Block on identifying that person before first macOS release.
- **Compatibility window.** Strict same-version is simplest to support; relaxing to N-1 adds a compatibility matrix. Revisit once v1 ships.
- **Client-server version drift detection.** Today the handshake refuses major mismatch; minor drift is accepted with a warning. Good enough — no additional drift telemetry planned.
