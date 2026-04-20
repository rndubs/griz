"""RPC-transport parity gate for the MCP smoke tests.

Mirrors `test_smoke.py` case-for-case, but wires the session factory to
a `Griz` configured with the length-framed `RpcWorker` instead of the
newline-delimited `Worker`. Runs against:

    MCP Client → FastMCP server → griz_mcp tools → griz.Griz(RpcWorker)
               → griz-server --transport=rpc over TCP loopback

Purpose: ensure the JSON envelope is bit-identical on both transports
(the "one envelope, two transports" invariant from planning/ui-design/
02-protocol.md §2.5). Any divergence between this file and
test_smoke.py means we've accidentally added a transport-specific
envelope quirk and should fix it at the core.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DB = REPO_ROOT / "Src" / "test" / "image" / "bar71" / "bar71.pltA"

_server_binaries = sorted(
    REPO_ROOT.glob("Src/GRIZ4-*/bin_server_opt/griz-server"),
    key=lambda p: p.stat().st_mtime,
    reverse=True,
)
DEFAULT_BIN = _server_binaries[0] if _server_binaries else None


@pytest.fixture(autouse=True)
def _require_real_server():
    if not DEFAULT_BIN or not Path(DEFAULT_BIN).is_file():
        pytest.skip("griz-server binary not built; run ./build.sh server")
    if not DEFAULT_DB.exists():
        pytest.skip(f"sample database not found at {DEFAULT_DB}")


@pytest.fixture(autouse=True)
def _reset_session_with_rpc():
    """Install an RPC-backed Griz factory for every test in this module."""
    from griz import Griz, RpcWorker
    from griz_mcp import session

    session.reset()

    def _factory(**kwargs):
        return Griz(worker_factory=RpcWorker, **kwargs)

    session._set_factory(_factory)
    yield
    session.reset()


# ---------------------------------------------------------------------- #
# Helpers — identical to test_smoke.py.
# ---------------------------------------------------------------------- #


def _call_tool(mcp_server, name: str, args: dict | None = None):
    from fastmcp import Client

    async def _run():
        async with Client(mcp_server) as client:
            return await client.call_tool(name, args or {})

    import asyncio
    return asyncio.run(_run())


def _text_from(result) -> str:
    for item in result.content:
        if hasattr(item, "text"):
            return item.text
    raise AssertionError(f"no text content in result: {result}")


def _json_from(result) -> dict | list:
    return json.loads(_text_from(result))


def _has_image(result) -> bool:
    for item in result.content:
        if hasattr(item, "data") and hasattr(item, "mimeType"):
            return True
    return False


# ---------------------------------------------------------------------- #
# Tests — same assertions as test_smoke.py, different transport.
# ---------------------------------------------------------------------- #


class TestRpcSmokeWithRealDatabase:
    """Envelope-parity gate: RPC transport must match stdio behavior."""

    def _mcp(self):
        from griz_mcp.server import mcp
        return mcp

    def test_open_database(self):
        result = _call_tool(self._mcp(), "open_database", {"path": str(DEFAULT_DB)})
        state = _json_from(result)
        assert "time_state" in state
        assert "state_count" in state
        assert state["state_count"] > 0

    def test_open_and_get_state(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "get_state")
        state = _json_from(result)
        assert state["time_state"] == 0
        assert state["state_count"] == 81
        assert state["viewport"] == {"width": 1024, "height": 1024}

    def test_list_fields(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "list_fields")
        fields = _json_from(result)
        assert isinstance(fields, list)
        assert len(fields) > 0
        names = [f["name"] for f in fields]
        assert any("s" in n for n in names)

    def test_show_field(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "show_field", {"name": "stress", "component": "xx"})
        state = _json_from(result)
        assert state["current_field"] == "sx"

    def test_set_time_state(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "set_time_state", {"state": 10})
        state = _json_from(result)
        assert state["time_state"] == 9

    def test_rotate_and_reset_view(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "rotate_view", {"x": 45.0, "y": 30.0})
        state = _json_from(result)
        assert "time_state" in state
        result = _call_tool(mcp, "reset_view")
        state = _json_from(result)
        assert "time_state" in state

    def test_list_materials(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "list_materials")
        mats = _json_from(result)
        assert isinstance(mats, list)
        assert len(mats) >= 1
        assert mats[0]["id"] == 1
        assert mats[0]["visible"] is True

    def test_hide_and_show_materials(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "hide_materials", {"material_ids": [1]})
        state = _json_from(result)
        assert "time_state" in state
        result = _call_tool(mcp, "show_materials", {"material_ids": [1]})
        state = _json_from(result)
        assert "time_state" in state

    def test_screenshot_returns_png(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        _call_tool(mcp, "show_field", {"name": "stress", "component": "xx"})
        result = _call_tool(mcp, "screenshot")
        assert _has_image(result), "screenshot should return an image content block"

    def test_animate(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "animate", {"start": 0, "end": 3, "step": 1})
        frames = _json_from(result)
        assert isinstance(frames, list)
        assert len(frames) == 4

    def test_raw_command(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "raw_command", {"command": "help"})
        resp = _json_from(result)
        assert resp["status"] == "ok"

    def test_close_database(self):
        mcp = self._mcp()
        _call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)})
        result = _call_tool(mcp, "close_database")
        text = _text_from(result)
        assert "closed" in text.lower()

    def test_full_workflow(self):
        mcp = self._mcp()
        state = _json_from(_call_tool(mcp, "open_database", {"path": str(DEFAULT_DB)}))
        assert state["state_count"] == 81

        fields = _json_from(_call_tool(mcp, "list_fields"))
        assert len(fields) > 0

        state = _json_from(
            _call_tool(mcp, "show_field", {"name": "stress", "component": "xx"})
        )
        assert state["current_field"] == "sx"

        state = _json_from(_call_tool(mcp, "set_time_state", {"state": 40}))
        assert state["time_state"] == 39

        _call_tool(mcp, "rotate_view", {"x": 30.0, "y": 45.0})

        result = _call_tool(mcp, "screenshot")
        assert _has_image(result)

        mats = _json_from(_call_tool(mcp, "list_materials"))
        assert len(mats) >= 1

        _call_tool(mcp, "close_database")

        result = _call_tool(mcp, "get_state")
        text = _text_from(result)
        assert "No database" in text
