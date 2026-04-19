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

    def open(self, path):
        self._open = True
        self._database_path = path

    def close(self):
        self._open = False
        self._database_path = None

    @property
    def is_open(self):
        return self._open

    def state(self):
        return dict(self._state)

    def raw(self, command, **kwargs):
        return {"status": "ok", "command": command}

    def screenshot(self):
        return b"\x01\x01" + b"\x00" * 100

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
