# Render Toggles API (pygriz + MCP)

Adds a typed `render` namespace covering Griz's on/off plot-decoration
toggles (title, time, cmap, minmax, …). Today these are reachable only
through `g.raw("on title …")` / `raw_command("on title …")` — the
escape hatch — even though they round-trip cleanly through `q_state`'s
`render.toggles` block. This bridges the last gap where MCP / pygriz
callers must drop to raw Griz syntax for a routine action.

Scope is intentionally narrow: cover the toggles that `q_state` already
emits (MVP), then expand vocabulary in a second pass once the server
query is widened.

## 0. Implementation Tracker

- [ ] **R1** — `RenderAPI` in `pygriz/src/griz/render.py` (`show`, `hide`, `set_toggles`, `state`)
- [ ] **R2** — Wire `Griz.render` property + `__init__` re-export
- [ ] **R3** — Unit + smoke tests for `RenderAPI` (stdio + RPC parity)
- [ ] **R4** — MCP tools `set_plot_labels` / `show_plot_labels` / `hide_plot_labels`
- [ ] **R5** — MCP smoke-test parity (`pygriz_mcp/tests/test_smoke.py`, `test_smoke_rpc.py`)
- [ ] **R6** — Update `tmp/sx_time_history.py` to use `g.render.show(...)` instead of `g.raw(...)` (proof point)
- [ ] **R7** — Audit `Src/interpret.c` `on`/`off` vocabulary; widen `q_state` toggle block + `RenderAPI` whitelist
- [ ] **R8** — Update CLAUDE.md (CLI gotcha tip stays accurate; mention typed path)

---

## 1. Background

### What's already there
- `Src/interpret.c:1980` parses `on <name…>` / `off <name…>` against ~20
  toggle names (`bbox`/`box`, `coord`, `time`, `title`, `path`, `cmap`,
  `minmax`, `scale`/`dscal`, `date`, `tinfo`, `cscale`, `edges`, `all`,
  plus `echocmd`, `autogray`, `ipt_labels`, `snap`, `mat_labels`).
- `Src/server_query.c:338` emits **7 of these** in `q_state`'s
  `render.toggles` block: `coord`, `time`, `cmap`, `minmax`, `title`,
  `bbox`, `edges`. Everything else is settable but not observable.
- `pygriz` has `field`, `view`, `time`, `materials`, `selection` typed
  namespaces. No render/display namespace.
- MCP exposes one `@mcp.tool` per pygriz method, plus the `raw_command`
  escape hatch. No tool covers plot toggles.

### What this design adds
A `RenderAPI` whose vocabulary is exactly the set of toggles `q_state`
round-trips. The user can `show`/`hide` toggles by name, set many at
once via kwargs, and read back the current state without parsing raw
Griz output. MCP gets matching tools so an LLM caller never needs
`raw_command` for plot decoration.

The choice to limit MVP vocabulary to the 7 already in `q_state` is
deliberate: anything we let users *set* should also be *readable* via
the typed surface, so `set → state → assert` workflows work. R7
broadens the vocabulary alongside a server change.

---

## 2. Python API design

**File:** `pygriz/src/griz/render.py` (new)

```python
class RenderAPI:
    """Plot-decoration toggles (title, colormap, minmax, …).

    The toggle vocabulary is whatever `q_state` reports under
    ``render.toggles``. Callers passing an unknown name get
    ``ValueError`` *before* anything is sent to the server.
    """

    # Whitelist mirrors what q_state emits (Src/server_query.c:338).
    KNOWN = frozenset({"coord", "time", "cmap", "minmax", "title", "bbox", "edges"})

    def __init__(self, griz: "Griz") -> None:
        self._griz = griz

    def show(self, *names: str) -> dict:
        """Turn on one or more toggles. Returns the updated state."""
        self._dispatch("on", names)
        return self._griz.state()

    def hide(self, *names: str) -> dict:
        """Turn off one or more toggles."""
        self._dispatch("off", names)
        return self._griz.state()

    def set_toggles(self, **flags: bool) -> dict:
        """Set many toggles at once: ``g.render.set_toggles(title=True, edges=False)``.

        Groups by truthiness so each direction is a single ``on`` /
        ``off`` command rather than N commands. Empty call is a no-op
        and returns the current state.
        """
        on = [k for k, v in flags.items() if v]
        off = [k for k, v in flags.items() if not v]
        self._dispatch("on", on)
        self._dispatch("off", off)
        return self._griz.state()

    def state(self) -> dict:
        """Return the current ``render.toggles`` block as ``{name: bool}``."""
        return dict(self._griz.state().get("render", {}).get("toggles", {}))

    def _dispatch(self, verb: str, names: Iterable[str]) -> None:
        names = list(names)
        if not names:
            return
        unknown = [n for n in names if n not in self.KNOWN]
        if unknown:
            raise ValueError(
                f"unknown render toggle(s): {unknown}. "
                f"Known: {sorted(self.KNOWN)}"
            )
        self._griz._require_worker().cmd(f"{verb} {' '.join(names)}")
```

### Wiring (R2)

`pygriz/src/griz/session.py`:

```python
from griz.render import RenderAPI
# in __init__:
self._render = RenderAPI(self)
# new property:
@property
def render(self) -> RenderAPI:
    return self._render
```

`pygriz/src/griz/__init__.py` — add `from griz.render import RenderAPI`
to the exports (mirrors how `FieldAPI` etc. are surfaced).

### Why no "smart group" methods (e.g. `plot_labels()`)

The MCP layer is the right home for high-level "turn on the standard
labels" affordances — it caters to LLM ergonomics. Pygriz stays close
to the underlying primitive so a power user can compose it.

---

## 3. MCP surface design

**File:** `pygriz_mcp/src/griz_mcp/server.py`

Three tools, parallel to `hide_materials`/`show_materials`/`list_materials`:

```python
@mcp.tool
def show_plot_labels(names: list[str]) -> str:
    """Turn on plot decorations.

    Valid names: ``title``, ``time``, ``cmap`` (colormap legend),
    ``minmax`` (cumulative min/max readout), ``coord`` (axis triad),
    ``bbox`` (bounding box), ``edges`` (mesh edges). Multiple names may
    be passed in one call. Returns the updated viewer state.
    """

@mcp.tool
def hide_plot_labels(names: list[str]) -> str:
    """Turn off plot decorations. See show_plot_labels for the
    valid name set."""

@mcp.tool
def set_plot_labels(
    title: bool | None = None,
    time: bool | None = None,
    cmap: bool | None = None,
    minmax: bool | None = None,
    coord: bool | None = None,
    bbox: bool | None = None,
    edges: bool | None = None,
) -> str:
    """Set plot decorations explicitly. ``None`` (the default) leaves
    a toggle untouched; ``True``/``False`` flip it. Returns the updated
    state."""
```

Implementation drops to `griz.render.show(...)` / `.hide(...)` /
`.set_toggles(...)`. Errors map via the existing `_err()` translator —
`ValueError` from the whitelist already shows a useful message; we
propagate it as `ToolError` so LLMs see the legal names.

`set_plot_labels` is the LLM-friendly form (typed booleans, named
parameters, IDE-completable). `show`/`hide` are the bulk-ergonomic form
that mirrors `show_materials` / `hide_materials`. Both pay for
themselves: the named-bool form is great for "make this look right",
the list form is great for "toggle these N things off".

`get_state()` already returns `render.toggles`, so there's no
`list_plot_labels` tool — callers read state instead. Document that in
the tool docstrings.

---

## 4. Tests (R3, R5)

### Pygriz unit-style (`pygriz/tests/test_render.py`, new)

Use the same in-process worker fixture pattern `test_field.py` /
`test_materials.py` follow. Cases:

1. `show("title")` → `state()["render"]["toggles"]["title"] is True`.
2. `hide("title", "cmap")` clears both, single command (assert via
   command-log spy that exactly `off title cmap` was emitted).
3. `set_toggles(title=True, edges=False)` produces one `on` and one
   `off` command, in that order (or no command if both lists are empty).
4. `show("does-not-exist")` raises `ValueError` *without* calling
   the worker (prevents accidental Griz syntax leakage).
5. `state()` returns a plain `dict[str, bool]` of the 7 names.

### Pygriz smoke (`pygriz/tests/test_render_smoke.py`, new)

End-to-end against a real `griz-server` (skip-if-binary-missing). One
golden-path test: open `bar71.pltA`, `g.render.show("title", "time")`,
assert via `g.state()` that both went on, `g.render.hide("title")`
flips title back off, time stays on.

### MCP smoke parity

Extend `pygriz_mcp/tests/test_smoke.py` with:
- `test_show_plot_labels_roundtrip` — call `show_plot_labels(["title","time"])`,
  parse JSON return, assert `render.toggles.title` and `.time` are
  `True`.
- `test_hide_plot_labels_unknown_name` — call with `["nonexistent"]`,
  expect a `ToolError` whose message lists the legal names.
- `test_set_plot_labels_typed` — call
  `set_plot_labels(title=True, edges=False)`, assert state matches.

The RPC parity gate (`test_smoke_rpc.py`) inherits these by virtue of
being a transport swap — same suite, different worker factory. Goal:
zero per-transport divergence.

---

## 5. Proof point (R6)

After R1–R4 land, replace the lone `g.raw(...)` line in
`tmp/sx_time_history.py` with:

```python
g.render.show("title", "time", "cmap", "minmax")
```

Validates the typed surface against the workflow that motivated the
ticket. The `tmp/` directory is throwaway; we keep this script around
just long enough to sign off the change, then delete it.

---

## 6. Vocabulary expansion (R7, follow-up)

The MVP whitelist is the intersection of "settable in `interpret.c`"
and "readable in `q_state`". Expanding requires both ends:

1. Audit `Src/interpret.c:1980` `on`/`off` parser for the full
   vocabulary. Record the canonical list.
2. For each toggle, decide:
   - **Plot decoration** (e.g. `path`, `date`, `tinfo`, `cscale`,
     `scale`/`dscal`) — extend `q_state`'s toggle block to expose
     them, then add to `RenderAPI.KNOWN`.
   - **Diagnostic / dev** (e.g. `echocmd`, `autogray`, `snap`) —
     decide case-by-case; some belong in a future debug API, not
     `render`.
   - **Aliases** (e.g. `box` ↔ `bbox`) — keep one canonical name in the
     whitelist; raise on the alias to avoid round-trip ambiguity.
3. Update tests + tool docstrings.

This is a separate ticket because it touches the C server. R1–R6 stand
alone and don't block on it.

---

## 7. Out of scope

- **Render mode** (`hidden` / `wireframe` / `solid` / `gray`). Lives
  alongside toggles in `q_state` but uses different Griz commands
  (`switch hidden` etc.); deserves its own typed API surface
  (`g.render.set_mode("wireframe")`) once the toggle layer ships.
- **Colormap selection** (`render.colormap` is also in `q_state` but
  set by `cmap <preset>` / `setcm`, not on/off). Same story —
  separate API, separate ticket.
- **Persistence across sessions** — pygriz/MCP do not remember
  toggles between connections. Not a regression; not in scope.
- **Backward-compat shim** — `g.raw("on title")` keeps working
  unchanged. We don't need to deprecate it; it's the documented escape
  hatch.
