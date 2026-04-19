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
    """Open a Griz database file. Returns the initial viewer state."""
    try:
        return session.open_database(path)
    except (GrizError, FileNotFoundError, OSError) as e:
        raise _err(e) from e


@mcp.tool
def close_database() -> str:
    """Close the current database and tear down the session."""
    try:
        return session.close_database()
    except GrizError as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# Field tools
# ------------------------------------------------------------------ #


@mcp.tool
def show_field(name: str, component: str | None = None) -> str:
    """Display a field on the mesh. Returns the updated viewer state."""
    try:
        griz = session.require_session()
        result = griz.field.show(name, component=component)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


@mcp.tool
def list_fields() -> str:
    """List all fields available in the current database."""
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
    """Rotate the camera by the given angles (degrees). Returns the updated state."""
    try:
        griz = session.require_session()
        result = griz.view.rotate(x=x, y=y, z=z)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError) as e:
        raise _err(e) from e


@mcp.tool
def reset_view() -> str:
    """Reset the camera to the default home view. Returns the updated state."""
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
    """Jump to the given time state index. Returns the updated viewer state."""
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
    """Animate through time states. Returns the list of per-frame states."""
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
    """Hide the given materials by ID. Returns the updated state."""
    try:
        griz = session.require_session()
        result = griz.materials.hide(material_ids)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


@mcp.tool
def show_materials(material_ids: list[int]) -> str:
    """Show the given materials by ID. Returns the updated state."""
    try:
        griz = session.require_session()
        result = griz.materials.show(material_ids)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError, ValueError) as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# Screenshot
# ------------------------------------------------------------------ #


@mcp.tool
def screenshot() -> Image:
    """Capture the current frame and return it as an image."""
    try:
        griz = session.require_session()
        data = griz.screenshot()
        if isinstance(data, bytes):
            return Image(data=data, format="rgb")
        # path was returned — read it
        with open(data, "rb") as f:
            return Image(data=f.read(), format="rgb")
    except (GrizError, RuntimeError, OSError) as e:
        raise _err(e) from e


# ------------------------------------------------------------------ #
# State / utility
# ------------------------------------------------------------------ #


@mcp.tool
def get_state() -> str:
    """Return the current viewer state as JSON."""
    try:
        return session.get_status()
    except GrizError as e:
        raise _err(e) from e


@mcp.tool
def restart_session() -> str:
    """Close and discard the current Griz session."""
    try:
        return session.restart()
    except GrizError as e:
        raise _err(e) from e


@mcp.tool
def raw_command(command: str) -> str:
    """Send a raw Griz command and return the full response."""
    try:
        griz = session.require_session()
        result = griz.raw(command)
        return json.dumps(result, default=str)
    except (GrizError, RuntimeError) as e:
        raise _err(e) from e


def main() -> None:
    """Entry point for ``griz-mcp`` console script."""
    mcp.run()
