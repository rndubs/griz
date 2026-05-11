"""Tests for MCP tool functions in server.py."""

from __future__ import annotations

import json

import pytest
from fastmcp.exceptions import ToolError
from griz.exceptions import GrizConnectionError, UnknownFieldError

from griz_mcp import session
from griz_mcp.server import (
    animate,
    close_database,
    get_state,
    hide_materials,
    list_fields,
    list_materials,
    open_database,
    raw_command,
    reset_view,
    restart_session,
    rotate_view,
    screenshot,
    set_time_state,
    show_field,
    show_materials,
)


# ------------------------------------------------------------------ #
# Database lifecycle
# ------------------------------------------------------------------ #


class TestOpenDatabase:
    def test_success(self, mock_griz):
        result = open_database("/fake/db.plt")
        state = json.loads(result)
        assert "time_state" in state

    def test_file_not_found_raises_tool_error(self, mock_griz):
        def bad_factory(**kw):
            raise FileNotFoundError("no such file")
        session._set_factory(bad_factory)
        with pytest.raises(ToolError, match="FileNotFoundError"):
            open_database("/missing/db.plt")

    def test_griz_error_raises_tool_error(self, mock_griz):
        def bad_factory(**kw):
            raise GrizConnectionError("cannot connect")
        session._set_factory(bad_factory)
        with pytest.raises(ToolError, match="GrizConnectionError"):
            open_database("/fake/db.plt")


class TestCloseDatabase:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = close_database()
        assert "closed" in result.lower()


# ------------------------------------------------------------------ #
# Field tools
# ------------------------------------------------------------------ #


class TestShowField:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = show_field("stress")
        state = json.loads(result)
        assert "time_state" in state
        mock_griz[0].field.show.assert_called_once_with("stress", component=None)

    def test_with_component(self, mock_griz):
        open_database("/fake/db.plt")
        show_field("stress", component="xx")
        mock_griz[0].field.show.assert_called_once_with("stress", component="xx")

    def test_no_session_raises_tool_error(self):
        with pytest.raises(ToolError, match="No database is open"):
            show_field("stress")

    def test_unknown_field_raises_tool_error(self, mock_griz):
        open_database("/fake/db.plt")
        mock_griz[0].field.show.side_effect = UnknownFieldError("nope")
        with pytest.raises(ToolError, match="UnknownFieldError"):
            show_field("bogus")


class TestListFields:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = list_fields()
        fields = json.loads(result)
        assert fields == [{"name": "stress"}]

    def test_no_session_raises(self):
        with pytest.raises(ToolError, match="No database is open"):
            list_fields()


# ------------------------------------------------------------------ #
# View tools
# ------------------------------------------------------------------ #


class TestRotateView:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = rotate_view(x=45.0)
        state = json.loads(result)
        assert "time_state" in state
        mock_griz[0].view.rotate.assert_called_once_with(x=45.0, y=0.0, z=0.0)

    def test_no_session_raises(self):
        with pytest.raises(ToolError, match="No database is open"):
            rotate_view(x=10.0)


class TestResetView:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = reset_view()
        state = json.loads(result)
        assert "time_state" in state

    def test_no_session_raises(self):
        with pytest.raises(ToolError, match="No database is open"):
            reset_view()


# ------------------------------------------------------------------ #
# Time tools
# ------------------------------------------------------------------ #


class TestSetTimeState:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = set_time_state(3)
        state = json.loads(result)
        assert "time_state" in state
        mock_griz[0].time.set_state.assert_called_once_with(3)

    def test_no_session_raises(self):
        with pytest.raises(ToolError, match="No database is open"):
            set_time_state(0)


class TestAnimate:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = animate(start=0, end=5)
        payload = json.loads(result)
        assert isinstance(payload, dict)
        assert "frame_count" in payload
        assert "final_state" in payload

    def test_no_session_raises(self):
        with pytest.raises(ToolError, match="No database is open"):
            animate()


# ------------------------------------------------------------------ #
# Material tools
# ------------------------------------------------------------------ #


class TestHideMaterials:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = hide_materials([1, 2])
        state = json.loads(result)
        assert "time_state" in state
        mock_griz[0].materials.hide.assert_called_once_with([1, 2])

    def test_no_session_raises(self):
        with pytest.raises(ToolError, match="No database is open"):
            hide_materials([1])


class TestShowMaterials:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = show_materials([3, 4])
        state = json.loads(result)
        assert "time_state" in state
        mock_griz[0].materials.show.assert_called_once_with([3, 4])


class TestListMaterials:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = list_materials()
        mats = json.loads(result)
        assert isinstance(mats, list)
        assert mats[0]["id"] == 1

    def test_no_session_raises(self):
        with pytest.raises(ToolError, match="No database is open"):
            list_materials()


# ------------------------------------------------------------------ #
# Screenshot
# ------------------------------------------------------------------ #


class TestScreenshot:
    def test_returns_image(self, mock_griz):
        open_database("/fake/db.plt")
        img = screenshot()
        # fastmcp Image wraps bytes
        from fastmcp.utilities.types import Image
        assert isinstance(img, Image)

    def test_no_session_raises(self):
        with pytest.raises(ToolError, match="No database is open"):
            screenshot()


# ------------------------------------------------------------------ #
# State / utility
# ------------------------------------------------------------------ #


class TestGetState:
    def test_when_open(self, mock_griz):
        open_database("/fake/db.plt")
        result = get_state()
        state = json.loads(result)
        assert "time_state" in state

    def test_when_closed(self):
        result = get_state()
        assert "No database" in result


class TestRestartSession:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = restart_session()
        assert "restarted" in result.lower()


class TestRawCommand:
    def test_success(self, mock_griz):
        open_database("/fake/db.plt")
        result = raw_command("help")
        resp = json.loads(result)
        assert resp["command"] == "help"

    def test_no_session_raises(self):
        with pytest.raises(ToolError, match="No database is open"):
            raw_command("help")
