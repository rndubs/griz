"""FastMCP server exposing Griz as MCP tools."""

from __future__ import annotations

import json

from fastmcp import FastMCP
from fastmcp.exceptions import ToolError
from fastmcp.utilities.types import Image
from griz.exceptions import GrizError, UnknownFieldError
from griz.results_map import default_map

from griz_mcp import session

mcp = FastMCP("griz-mcp")


def _field_families_hint() -> str:
    """Build the ``show_field`` guidance from the live results map.

    Stays in sync with ``Src/data/results_map.yaml`` automatically — a
    new family added to the YAML appears in the next error message
    without touching this file.
    """
    parts: list[str] = []
    for family, components in default_map().describe().items():
        if components is None:
            parts.append(f"{family} (scalar)")
        else:
            parts.append(f"{family} ({'/'.join(components)})")
    return (
        "Curated families: " + ", ".join(parts) + ". "
        "For raw Mili names like 'sx', 'seff', 'eps' (anything from "
        "list_fields()), call show_field(name=<raw>) WITHOUT a component."
    )


def _err(e: Exception) -> ToolError:
    """Translate a domain exception to a ToolError."""
    if isinstance(e, UnknownFieldError):
        return ToolError(f"UnknownFieldError: {e}. {_field_families_hint()}")
    return ToolError(f"{type(e).__name__}: {e}")


# ------------------------------------------------------------------ #
# Database lifecycle
# ------------------------------------------------------------------ #


@mcp.tool
def open_database(path: str | None = None) -> str:
    """Open a Mili simulation database for visualization.

    Two modes:

    * **Attach** (when a Qt UI is already running): the MCP server
      attaches to the UI's griz-server. ``path`` is *optional* and
      ignored — the UI already chose which database to open. Call
      ``session_status()`` first to check whether a UI is up.

    * **Spawn** (no UI running): ``path`` is required and points at a
      Mili plotfile (e.g. ``/path/to/run.pltA``). The MCP server
      spawns its own headless griz-server.

    Idempotent in attach mode: re-calling while already attached to the
    same live UI returns the current state without flickering the
    viewport.

    Returns the viewer state JSON: time range, materials, viewport size,
    current field. Must succeed before any other tool can run.
    """
    try:
        return session.open_database(path)
    except (GrizError, FileNotFoundError, OSError, RuntimeError) as e:
        raise _err(e) from e


@mcp.tool
def close_database() -> str:
    """Close the current database and shut down the visualization server."""
    try:
        return session.close_database()
    except GrizError as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# Field tools
# ------------------------------------------------------------------ #


@mcp.tool
def show_field(name: str, component: str | None = None) -> str:
    """Display a result field on the mesh with a color map.

    Two ways to call this:

    1. Curated families (recommended for human-readable code):
         stress: xx, yy, zz, xy, yz, zx, von_mises, pressure
         strain: xx, yy, zz, xy, yz, zx
         displacement: x, y, z, magnitude
         velocity / acceleration: x, y, z, magnitude
         temperature: (no component — scalar)
       e.g. ``show_field("stress", "von_mises")``.

    2. Raw Mili names from ``list_fields()``: pass any ``name`` value
       returned by ``list_fields()`` (e.g. ``sx``, ``seff``, ``eps``,
       ``dispmag``) as a single arg, no component. Useful when the
       desired field isn't in the curated families above.
       e.g. ``show_field("sx")`` for X-component stress.

    Mixing modes is rejected: passing a component alongside an unknown
    family is treated as a typo.

    Returns the updated viewer state.
    """
    try:
        griz = session.require_session()
        result = griz.field.show(name, component=component)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


@mcp.tool
def list_fields() -> str:
    """List all result fields available in the loaded database.

    Returns a JSON array of objects with ``name`` (the Griz command name)
    and ``title`` (human-readable description). Use the ``name`` value
    with ``show_field()`` to display a field. Both primal (raw database)
    and derived (computed) results are included.
    """
    try:
        griz = session.require_session()
        result = griz.field.list()
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError) as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# View tools
# ------------------------------------------------------------------ #


@mcp.tool
def rotate_view(x: float = 0.0, y: float = 0.0, z: float = 0.0) -> str:
    """Rotate the camera by the given angles in degrees around each axis.

    Positive X rotates the model "down" (nose toward you), positive Y
    rotates "right", positive Z rotates clockwise. Rotations accumulate.
    Returns the updated viewer state.
    """
    try:
        griz = session.require_session()
        result = griz.view.rotate(x=x, y=y, z=z)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError) as e:
        raise _err(e) from e


@mcp.tool
def reset_view() -> str:
    """Reset the camera to the default home orientation (isometric view).

    Undoes all prior rotations. Returns the updated viewer state.
    """
    try:
        griz = session.require_session()
        result = griz.view.reset()
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError) as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# Time tools
# ------------------------------------------------------------------ #


@mcp.tool
def set_time_state(state: int) -> str:
    """Jump to the given time-state index (0-based).

    Simulations store results at discrete time steps called "states".
    State 0 is the first output time; the maximum is reported in the
    ``state_count`` field of ``get_state()``. Returns the updated viewer
    state including the new ``time_value``.
    """
    try:
        griz = session.require_session()
        result = griz.time.set_state(state)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError) as e:
        raise _err(e) from e


@mcp.tool
def animate(
    start: int | None = None,
    end: int | None = None,
    step: int = 1,
    delay: float = 0.0,
) -> str:
    """Step through a range of time states, rendering each frame.

    `start` and `end` default to the first and last states. `step`
    controls the increment (use negative to go backward). `delay` is
    seconds to pause between frames (useful for visual pacing). Call
    ``screenshot()`` separately to capture the final frame as an image.
    Returns a JSON array of per-frame viewer states.
    """
    try:
        griz = session.require_session()
        frames = griz.time.animate(start=start, end=end, step=step, delay=delay)
        return json.dumps(frames, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# Material tools
# ------------------------------------------------------------------ #


@mcp.tool
def hide_materials(material_ids: list[int]) -> str:
    """Hide one or more materials by their 1-based ID.

    Hidden materials are removed from the rendered image but stay in the
    database. Use ``list_materials()`` to discover available IDs and
    current visibility. Returns the updated viewer state.
    """
    try:
        griz = session.require_session()
        result = griz.materials.hide(material_ids)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


@mcp.tool
def show_materials(material_ids: list[int]) -> str:
    """Make previously hidden materials visible again by their 1-based ID.

    Use ``list_materials()`` to see which materials are currently hidden.
    Returns the updated viewer state.
    """
    try:
        griz = session.require_session()
        result = griz.materials.show(material_ids)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


@mcp.tool
def list_materials() -> str:
    """List all materials in the loaded database with their visibility.

    Returns a JSON array of objects, each with ``id`` (1-based),
    ``visible`` (bool), and ``enabled`` (bool). Use the ``id`` values
    with ``hide_materials()`` and ``show_materials()``.
    """
    try:
        griz = session.require_session()
        result = griz.materials.list()
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError) as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# Render tools (plot decorations / on-off toggles)
# ------------------------------------------------------------------ #


@mcp.tool
def show_plot_labels(names: list[str]) -> str:
    """Turn on plot decorations on the rendered viewport.

    Valid names: ``title`` (problem title), ``time`` (time/state
    readout), ``cmap`` (colormap legend), ``minmax`` (cumulative
    min/max readout), ``coord`` (axis triad), ``bbox`` (bounding box),
    ``edges`` (mesh element edges), ``path`` (database path under the
    title), ``cscale`` (color scale numeric labels), ``scale``
    (displacement scale), ``date`` (current datetime), ``tinfo``
    (time-step info). Multiple toggles can be enabled in a single
    call. Returns the updated viewer state — the ``render.toggles``
    block reflects the new values.
    """
    try:
        griz = session.require_session()
        result = griz.render.show(*names)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


@mcp.tool
def hide_plot_labels(names: list[str]) -> str:
    """Turn off plot decorations on the rendered viewport.

    Same name vocabulary as ``show_plot_labels``: ``title``, ``time``,
    ``cmap``, ``minmax``, ``coord``, ``bbox``, ``edges``, ``path``,
    ``cscale``, ``scale``, ``date``, ``tinfo``. Multiple toggles can
    be disabled in a single call. Returns the updated viewer state.
    """
    try:
        griz = session.require_session()
        result = griz.render.hide(*names)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


@mcp.tool
def set_plot_labels(
    title: bool | None = None,
    time: bool | None = None,
    cmap: bool | None = None,
    minmax: bool | None = None,
    coord: bool | None = None,
    bbox: bool | None = None,
    edges: bool | None = None,
    path: bool | None = None,
    cscale: bool | None = None,
    scale: bool | None = None,
    date: bool | None = None,
    tinfo: bool | None = None,
) -> str:
    """Set plot decorations explicitly via named booleans.

    Each argument controls one toggle: ``True`` enables it, ``False``
    disables it, ``None`` (the default) leaves it untouched. Typed
    counterpart to ``show_plot_labels`` / ``hide_plot_labels`` — same
    vocabulary, named parameters. Returns the updated viewer state.

    Current toggle state lives in ``get_state().render.toggles``.
    """
    try:
        griz = session.require_session()
        flags: dict[str, bool] = {}
        for k, v in (
            ("title", title),
            ("time", time),
            ("cmap", cmap),
            ("minmax", minmax),
            ("coord", coord),
            ("bbox", bbox),
            ("edges", edges),
            ("path", path),
            ("cscale", cscale),
            ("scale", scale),
            ("date", date),
            ("tinfo", tinfo),
        ):
            if v is not None:
                flags[k] = bool(v)
        result = griz.render.set_toggles(**flags)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# Screenshot
# ------------------------------------------------------------------ #


@mcp.tool
def screenshot() -> Image:
    """Capture the current rendered frame as a PNG image.

    Returns the image directly. The visualization must have an open
    database with a field displayed to produce a meaningful image.
    The default framebuffer is 1024x1024 pixels.
    """
    try:
        griz = session.require_session()
        data = griz.screenshot(format="png")
        if isinstance(data, bytes):
            return Image(data=data, format="png")
        # path was returned — read it
        with open(data, "rb") as f:
            return Image(data=f.read(), format="png")
    except (GrizError, RuntimeError, OSError) as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# State / utility
# ------------------------------------------------------------------ #


@mcp.tool
def get_state() -> str:
    """Return the current viewer state as JSON.

    The state includes: ``time_state``, ``time_value``, ``state_count``,
    ``max_time_value``, ``current_field``, and ``viewport`` dimensions.
    Useful for checking what's displayed before taking a screenshot.
    """
    try:
        return session.get_status()
    except GrizError as e:
        raise _err(e) from e


@mcp.tool
def session_status() -> str:
    """Report the MCP session's lifecycle status without sending commands.

    Returns a JSON object describing whether a session is open, whether
    it's in attach or spawn mode, the resolved rendezvous path (if any
    live UI exists), the UI's server PID/host/port, and the open
    database path. Pure local inspection — safe to call before
    ``open_database``.

    Use this to answer "is the Qt UI up?" or "am I attached to the
    same UI as last time?" without shelling out to ``pgrep`` or ``ls``.
    """
    return session.status()


@mcp.tool
def restart_session() -> str:
    """Close the current Griz session and release all resources.

    After restarting, call ``open_database()`` to begin a new session.
    Use this to recover from errors or switch to a different database.
    """
    try:
        return session.restart()
    except GrizError as e:
        raise _err(e) from e


@mcp.tool
def raw_command(command: str) -> str:
    """Send a raw Griz command string and return the full JSON response.

    Escape hatch for commands not exposed as dedicated tools. The
    ``command`` is sent directly to the Griz engine (e.g. ``"rview"``
    to reset the view, ``"help"`` for the built-in help text). Returns
    the raw response including any ``stdout`` captured from the command.
    """
    try:
        griz = session.require_session()
        result = griz.raw(command)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError) as e:
        raise _err(e) from e


def main() -> None:
    """Entry point for ``griz-mcp`` console script."""
    mcp.run()
