"""Shared fixtures for griz_mcp tests."""

from __future__ import annotations

from unittest.mock import MagicMock

import pytest

from griz_mcp import session


class MockGriz:
    """Lightweight stand-in for griz.Griz that never spawns a subprocess."""

    def __init__(self, **kwargs):
        self._open = False
        self._database_path = None
        self._field = MagicMock()
        self._view = MagicMock()
        self._time = MagicMock()
        self._materials = MagicMock()

        # Defaults for namespace methods
        self._state = {"time_state": 0, "max_time_state": 5}
        self._field.show.return_value = self._state
        self._field.list.return_value = [{"name": "stress"}]
        self._view.rotate.return_value = self._state
        self._view.reset.return_value = self._state
        self._time.set_state.return_value = self._state
        self._time.animate.return_value = [self._state]
        self._materials.hide.return_value = self._state
        self._materials.show.return_value = self._state
        self._materials.list.return_value = [
            {"id": 1, "visible": True, "enabled": True},
        ]

    def open(self, path):
        self._open = True
        self._database_path = path

    def attach(self, rendezvous_path):
        self._open = True
        self._database_path = None
        self._attached_to = str(rendezvous_path)

    def close(self):
        self._open = False
        self._database_path = None

    @property
    def is_open(self):
        return self._open

    @property
    def database_path(self):
        return self._database_path

    def state(self):
        return dict(self._state)

    def raw(self, command, **kwargs):
        return {"status": "ok", "command": command}

    def screenshot(self, **kwargs):
        # Return minimal valid PNG bytes for testing
        return b"\x89PNG\r\n\x1a\n" + b"\x00" * 100

    @property
    def field(self):
        return self._field

    @property
    def view(self):
        return self._view

    @property
    def time(self):
        return self._time

    @property
    def materials(self):
        return self._materials


@pytest.fixture(autouse=True)
def _scrub_attach_env(monkeypatch):
    """Scrub ``GRIZ_MCP_ATTACH_RENDEZVOUS`` so tests never silently attach to
    a live UI when the caller's shell has the user-facing .mcp.json default
    set. Lives in its own fixture (separate from ``_reset_session``) because
    test modules redefine ``_reset_session`` to install custom factories,
    which would shadow this scrub if it lived there.
    """
    monkeypatch.delenv("GRIZ_MCP_ATTACH_RENDEZVOUS", raising=False)


@pytest.fixture(autouse=True)
def _reset_session():
    """Reset the module-level session before each test."""
    session.reset()
    yield
    session.reset()


@pytest.fixture
def mock_griz():
    """Provide a MockGriz factory and wire it into the session module."""
    instances: list[MockGriz] = []

    def factory(**kwargs):
        g = MockGriz(**kwargs)
        instances.append(g)
        return g

    session._set_factory(factory)
    return instances
