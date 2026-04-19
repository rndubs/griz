# Griz

## MCP Implementation

Whenever working on the MCP layer, ensure that you update the ./planning/MCP.md file with our upated status after doing any work.
Keep the MCP.md file tidy.

The top-level checklist lives in `## 0. Implementation Status` at lines 1–84 of `./planning/MCP.md` — read just that range (`Read` with `offset=1, limit=84`) to check/update progress without pulling the full design doc.

## Build

Run `./build.sh` from the repo root to configure and build the `batchopt`
target. Pass alternate configure flags as arguments (e.g.
`./build.sh --with-mili=/some/path`); default is `--enable-nojpeg --enable-nopng`.
Output binary: `Src/GRIZ4-*/bin_batch_opt/griz4s.linux_opt_batch`.

Prereqs on LLNL TOSS: Mili at `/usr/apps/mdg`, system OSMesa/X11/Motif in
`/usr/lib64` + `/usr/include/GL`, an Intel or GCC compiler module loaded
(e.g. `intel-classic/2021.6.0-magic`), and `autoconf` on `PATH`.

If `configure.ac` changes, run `autoconf -f` in `Src/` before `build.sh`.

## Python

Any python work should use `uv` from Astral.
Call `uv --help` to access the full list of `uv` sub-commands when needed.
