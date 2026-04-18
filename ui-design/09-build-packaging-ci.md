# 09 — Build, packaging, and CI

## Scope

How the new code is built, tested in CI, and shipped. Covers the server's integration with the existing autoconf build, the Qt client's CMake build, and packaging for each client platform.

Out of scope: test content itself (see [10-testing](10-testing.md)).

## Related

- `UI.md` §7 (Roadmap) — packaging gates in phases 1 and 3.
- [03-server](03-server.md), [04-client](04-client.md)

## Sections to fill

- **Server build.**
  - Reuse existing autoconf. Add a new build target alongside `batchopt` (candidate name: `serveropt` / `serverdebug`).
  - What changes in `configure.ac` and the Makefiles. New optional dependencies (protobuf, libjpeg-turbo, etc.).
  - Option to omit GUI entirely when building the server.
- **Client build.**
  - CMake + Qt 6. Conan / vcpkg / system packages for third-party dependencies — pick one.
  - Repo layout. Likely `client/` at repo root, separate from `Src/`.
- **Shared code.**
  - Protocol definitions (protobuf `.proto` files or equivalent) in a shared directory consumed by both builds.
  - Versioning: one `VERSION` that both sides read.
- **CI.**
  - Matrix: Linux x86_64, macOS (arm64 + x86_64), Windows x86_64 (phase 3).
  - Runners: GitHub Actions by default; note any LLNL-internal CI constraints.
  - Artifacts per build.
- **Packaging.**
  - Linux: tarball with `bin/` and dynamic deps; optional RPM/DEB if needed by sites.
  - macOS: `.app` bundle, signed with Developer ID, notarized. Apple account prerequisites.
  - Windows: MSI built with WiX or equivalent, Authenticode signed. Signing account prerequisites.
- **Release cadence.** Tagged releases mirror Griz version. Compatibility matrix between client and server across versions.

## Open questions

- Is there an LLNL-internal artifact repository the Linux package should feed into?
- macOS notarization — who owns the Apple Developer account?
- Can the client and server be pinned to the same version, or do we need N-1 compatibility for a transition window?
