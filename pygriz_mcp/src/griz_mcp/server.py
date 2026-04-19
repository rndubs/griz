"""FastMCP server exposing Griz as MCP tools."""

from __future__ import annotations

import json

from fastmcp import FastMCP
from fastmcp.exceptions import ToolError
from fastmcp.utilities.types import Image
from griz.exceptions import GrizError

from griz_mcp import session

mcp = FastMCP("griz-mcp")


def _err(e: Exception) -> ToolError:
    """Translate a domain exception to a ToolError."""
    return ToolError(f"{type(e).__name__}: {e}")


# ------------------------------------------------------------------ #
# Database lifecycle
# ------------------------------------------------------------------ #


@mcp.tool
def open_database(path: str) -> str:
    """Open a Mili simulation database for visualization.

    `path` is the path to a Mili plotfile, typically ending in `.plt` or
    `.pltA` (e.g. ``/path/to/simulation/run.pltA``). Returns the initial
    viewer state including time range, material count, and viewport size.
    Must be called before any other tool.
    """
    try:
        return session.open_database(path)
    except (GrizError, FileNotFoundError, OSError) as e:
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

    Common fields and components:
      stress: xx, yy, zz, xy, yz, zx, von_mises, pressure
      strain: xx, yy, zz, xy, yz, zx
      displacement: x, y, z, magnitude
      temperature: (no component needed — scalar field)

    Example: ``show_field("stress", "von_mises")`` shows von Mises stress.
    Use ``list_fields()`` to see all available fields in the current database.
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
