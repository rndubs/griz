"""Tests for the session singleton module."""

from __future__ import annotations

import json
import os
import time

import pytest

from griz_mcp import session


def _write_rendezvous(path, server_pid: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(
            {
                "version": 1,
                "session_id": "griz-deadbeef",
                "host": "127.0.0.1",
                "port": 12345,
                "token": "x" * 44,
                "server_pid": server_pid,
                "started_at": int(time.time()),
            }
        )
    )


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


class TestRendezvousResolution:
    """Resolution of GRIZ_MCP_ATTACH_RENDEZVOUS into a live attach target."""

    def test_pid_alive_for_own_process(self):
        assert session._is_pid_alive(os.getpid())

    def test_pid_alive_false_for_zero(self):
        assert not session._is_pid_alive(0)

    def test_rendezvous_live_when_pid_alive(self, tmp_path):
        rv = tmp_path / "ui-1.json"
        _write_rendezvous(rv, os.getpid())
        assert session._is_rendezvous_live(str(rv))

    def test_rendezvous_dead_when_file_missing(self, tmp_path):
        assert not session._is_rendezvous_live(str(tmp_path / "missing.json"))

    def test_rendezvous_dead_when_pid_dead(self, tmp_path):
        rv = tmp_path / "ui-stale.json"
        # PID 2^31-1 is effectively guaranteed not to exist on Linux.
        _write_rendezvous(rv, 2**31 - 1)
        assert not session._is_rendezvous_live(str(rv))

    def test_discover_picks_most_recent_live(self, tmp_path, monkeypatch):
        monkeypatch.setenv("HOME", str(tmp_path))
        rv_dir = tmp_path / ".griz" / "rendezvous"
        old = rv_dir / "ui-1.json"
        new = rv_dir / "ui-2.json"
        _write_rendezvous(old, os.getpid())
        time.sleep(0.01)
        _write_rendezvous(new, os.getpid())
        assert session._discover_live_rendezvous() == str(new)

    def test_discover_skips_stale_takes_older_live(self, tmp_path, monkeypatch):
        monkeypatch.setenv("HOME", str(tmp_path))
        rv_dir = tmp_path / ".griz" / "rendezvous"
        live_old = rv_dir / "ui-1.json"
        stale_new = rv_dir / "ui-2.json"
        _write_rendezvous(live_old, os.getpid())
        time.sleep(0.01)
        _write_rendezvous(stale_new, 2**31 - 1)
        assert session._discover_live_rendezvous() == str(live_old)

    def test_discover_returns_none_when_dir_empty(self, tmp_path, monkeypatch):
        monkeypatch.setenv("HOME", str(tmp_path))
        assert session._discover_live_rendezvous() is None


class TestOpenDatabaseAttachMode:
    """open_database under various GRIZ_MCP_ATTACH_RENDEZVOUS values."""

    def test_env_unset_falls_back_to_spawn(self, mock_griz, monkeypatch):
        monkeypatch.delenv(session._ATTACH_ENV, raising=False)
        session.open_database("/fake/db.plt")
        assert mock_griz[0]._database_path == "/fake/db.plt"
        assert not hasattr(mock_griz[0], "_attached_to") \
            or mock_griz[0]._attached_to is None  # never set

    def test_env_points_at_live_rendezvous_attaches_to_it(
        self, mock_griz, tmp_path, monkeypatch
    ):
        rv = tmp_path / "ui-live.json"
        _write_rendezvous(rv, os.getpid())
        monkeypatch.setenv(session._ATTACH_ENV, str(rv))
        session.open_database("/ignored/path.plt")
        assert mock_griz[0]._attached_to == str(rv)
        assert mock_griz[0]._database_path is None

    def test_stale_env_falls_back_to_discovery(
        self, mock_griz, tmp_path, monkeypatch
    ):
        monkeypatch.setenv("HOME", str(tmp_path))
        rv_dir = tmp_path / ".griz" / "rendezvous"
        live = rv_dir / "ui-live.json"
        _write_rendezvous(live, os.getpid())
        # env points at a path that doesn't exist; discovery should win.
        monkeypatch.setenv(
            session._ATTACH_ENV, str(tmp_path / "ui-stale.json")
        )
        session.open_database("/ignored/path.plt")
        assert mock_griz[0]._attached_to == str(live)

    def test_sentinel_value_triggers_discovery(
        self, mock_griz, tmp_path, monkeypatch
    ):
        monkeypatch.setenv("HOME", str(tmp_path))
        rv_dir = tmp_path / ".griz" / "rendezvous"
        live = rv_dir / "ui-live.json"
        _write_rendezvous(live, os.getpid())
        monkeypatch.setenv(session._ATTACH_ENV, "auto")
        session.open_database("/ignored/path.plt")
        assert mock_griz[0]._attached_to == str(live)

    def test_attach_requested_but_nothing_live_raises(
        self, mock_griz, tmp_path, monkeypatch
    ):
        monkeypatch.setenv("HOME", str(tmp_path))
        monkeypatch.setenv(session._ATTACH_ENV, "auto")
        with pytest.raises(RuntimeError, match="no live griz-server"):
            session.open_database("/some/db.plt")
