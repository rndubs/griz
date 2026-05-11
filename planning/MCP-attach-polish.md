# MCP Attach-Mode Polish

Removes the manual ceremony around bringing up the Qt UI + MCP server pair
and attaching them. Today a fresh session looks like:

1. `setsid nohup griz-client ... & disown`
2. `ls $HOME/.griz/rendezvous/`, pick the live one, `cat` it
3. `export GRIZ_MCP_ATTACH_RENDEZVOUS=<that path>`
4. (relaunch the MCP server so it picks up the env var)
5. `open_database("anything-the-path-is-ignored")`

That's five steps where it should be one.

## 0. Implementation Tracker

- [x] **F1** — `.mcp.json` defaults `GRIZ_MCP_ATTACH_RENDEZVOUS=auto` so attach is the default when a UI is running
- [x] **F2** — `auto` sentinel falls through to spawn mode when no live UI is found, instead of raising
- [x] **F3** — `scripts/griz-ui.sh up|down|status` idempotent UI lifecycle wrapper
- [x] **F4** — `griz_session_status` MCP tool exposes mode/rendezvous/PIDs/database without shelling out
- [x] **F5** — `open_database(path: Optional[str] = None)`; idempotent when already attached and live
- [x] **F6** — `show_field` error message lists curated families and the raw-name escape hatch

---

## F1 — `.mcp.json` default env

**File:** `/usr/WS1/whitmore/pydev/griz/.mcp.json`

Add an `env` block setting `GRIZ_MCP_ATTACH_RENDEZVOUS=auto`. With F2 this
is safe: the MCP server attaches if any live UI rendezvous exists, else
silently falls back to spawning its own server (current default behavior
when the env var is unset).

```json
{
  "mcpServers": {
    "griz": {
      "command": "uv",
      "args": ["--directory", "./pygriz_mcp", "run", "griz-mcp"],
      "env": { "GRIZ_MCP_ATTACH_RENDEZVOUS": "auto" }
    }
  }
}
```

---

## F2 — `auto` falls through to spawn

**File:** `pygriz_mcp/src/griz_mcp/session.py`

Current `open_database` raises `RuntimeError` when the env var is set but
no live rendezvous is found. With F1 the env var is *always* set, so this
would break spawn mode entirely.

New rule: `GRIZ_MCP_ATTACH_RENDEZVOUS=auto` means "attach if you can,
spawn otherwise." Any other literal value means "attach to this specific
path, scan-fallback, raise if nothing live" (preserves the diagnostic for
users who named a specific path).

Implementation: in `_resolve_attach_path`, special-case `"auto"` (and the
empty string, since that already falls through). The `RuntimeError`
branch in `open_database` only fires when the user named a literal path
that's now stale.

Tests: extend `pygriz_mcp/tests/` to cover (a) auto + no UI → spawn,
(b) auto + live UI → attach, (c) literal stale path + no live UI → raise.

---

## F3 — `scripts/griz-ui.sh`

**New file:** `scripts/griz-ui.sh` (mode 0755)

Subcommands:

- `up` — preflight (`griz-client`, `griz-server`, sample DB, `DISPLAY`,
  `xdpyinfo`); if a live UI rendezvous already exists print its path and
  exit 0; otherwise `setsid nohup` the client, poll the rendezvous dir,
  print the path, exit 0.
- `status` — print one-line summary `mode=<attach-able|none> ui_pid=<…>
  server_pid=<…> rendezvous=<path>`; exit 0 if a UI is up, 1 otherwise.
- `down` — find live UIs (via `pgrep` against the binary path), `kill
  -TERM`, wait, escalate to `-KILL` if needed; clean stale rendezvous
  files for dead PIDs.

Output is line-oriented for easy capture by the LLM:
`echo "rendezvous=$path"`. Exit codes communicate state.

Replaces the multi-line `setsid nohup ... & disown` recipe in
`PROMPT.md`.

---

## F4 — `griz_session_status` MCP tool

**File:** `pygriz_mcp/src/griz_mcp/server.py`, `session.py`

New `@mcp.tool` returning a JSON object:

```json
{
  "session_open": true,
  "mode": "attach" | "spawn" | "idle",
  "database": "Src/test/image/bar71/bar71.pltA",
  "rendezvous": "/home/.../ui-12345.json",
  "ui_pid": 12345,
  "server_pid": 12347,
  "attach_env": "auto"
}
```

Computed without sending any commands to a worker — pure local
inspection. Lets the LLM answer "is something running and what is it?"
without `pgrep`/`cat` shell calls.

Implementation lives in `session.py` as `status()`; `server.py` adds a
thin `@mcp.tool` wrapper. Reuses `_resolve_attach_path`, `_is_pid_alive`,
and the active `_session` for source of truth.

---

## F5 — `open_database` idempotency + optional path

**File:** `pygriz_mcp/src/griz_mcp/server.py`, `session.py`

Two changes:

1. `path: str` → `path: Optional[str] = None` on the MCP tool. When
   `path is None` and we're not in attach mode, raise a clean
   `ToolError("path is required when attaching is disabled")`. When
   `path is None` in attach mode, attach to the live UI. When `path` is
   provided in attach mode, log a one-line "attach mode — ignoring
   path" event and proceed with attach.

2. If `_session` is already open *and* the resolved attach target hasn't
   changed *and* the worker is still alive, return the current `state()`
   instead of tearing down + reattaching. Saves a UI flicker and the
   ~1s reconnect cost on every `open_database` call.

Updated docstring spells out attach vs spawn explicitly so the LLM stops
needing the "the path is ignored but a value is still required" gotcha.

---

## F6 — `show_field` error UX

**File:** `pygriz/src/griz/results_map.py` (the `UnknownFieldError`
producer) or `pygriz_mcp/src/griz_mcp/server.py` (translate at the MCP
layer).

Today: passing `show_field("sx", "xx")` raises
`UnknownFieldError: unknown field 'sx'`.

After: error message lists the curated families (stress / strain /
displacement / velocity / acceleration / temperature) and notes that raw
Mili names like `sx`, `seff`, `eps` work *without* a component
(via `list_fields()` for the full set).

Lowest-impact location is the MCP layer — translate the
`UnknownFieldError` into a `ToolError` with the richer message, leaving
`pygriz` untouched.

---

## Out of scope (deliberately)

- Auto-cleaning very old stale rendezvous files. Discovery already
  filters by live PID; stale files are harmless.
- `griz-mcp` watching for new rendezvous files mid-session and
  re-attaching when the UI restarts. Bigger change, separate ticket.
- Spawning the Qt UI from the MCP server itself. The UI is interactive
  and tying its lifetime to MCP makes the failure modes worse, not
  better.
- Fixing the upstream "Other I/0" Mili typo.
- Making screenshots parallelizable. Server-side global state — not
  fixable here.
