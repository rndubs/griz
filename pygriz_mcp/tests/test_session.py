"""Tests for the session singleton module."""

from __future__ import annotations

import pytest

from griz_mcp import session


class TestOpenDatabase:
    def test_opens_and_returns_state(self, mock_griz):
        result = session.open_database("/fake/db.plt")
        assert '"time_state"' in result
        assert len(mock_griz) == 1
        assert mock_griz[0].is_open

    def test_closes_previous_before_opening(self, mock_griz):
        session.open_database("/fake/db1.plt")
        session.open_database("/fake/db2.plt")
        assert len(mock_griz) == 2
        assert not mock_griz[0].is_open
        assert mock_griz[1].is_open


class TestCloseDatabase:
    def test_close_when_open(self, mock_griz):
        session.open_database("/fake/db.plt")
        result = session.close_database()
        assert "closed" in result.lower()
        assert not mock_griz[0].is_open

    def test_close_when_already_closed(self, mock_griz):
        result = session.close_database()
        assert "closed" in result.lower()


class TestRequireSession:
    def test_raises_when_no_session(self):
        with pytest.raises(RuntimeError, match="No database is open"):
            session.require_session()

    def test_returns_session_when_open(self, mock_griz):
        session.open_database("/fake/db.plt")
        griz = session.require_session()
        assert griz is mock_griz[0]


class TestRestart:
    def test_restart_clears_session(self, mock_griz):
        session.open_database("/fake/db.plt")
        result = session.restart()
        assert "restarted" in result.lower()
        with pytest.raises(RuntimeError):
            session.require_session()

    def test_restart_when_no_session(self):
        result = session.restart()
        assert "restarted" in result.lower()
