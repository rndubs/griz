"""Module-level Griz session singleton.

MCP is single-client-per-server, so a module-level session is the correct
model.  A ``_griz_factory`` callable allows test injection without patching.

Attach mode (planning/DEMO.md task C): when GRIZ_MCP_ATTACH_RENDEZVOUS
points at an existing rendezvous JSON file, ``open_database`` skips the
subprocess spawn and attaches to the server that wrote that file
(typically the Qt UI). The ``path`` argument is ignored — the peer
already chose which database to open — and we return the live q_state
so the LLM sees what's currently loaded.
"""

from __future__ import annotations

import json
import os
from typing import Callable

from griz import Griz

GrizFactory = Callable[..., Griz]

_session: Griz | None = None
_griz_factory: GrizFactory = Griz

_ATTACH_ENV = "GRIZ_MCP_ATTACH_RENDEZVOUS"


def _set_factory(factory: GrizFactory) -> None:
    """Override the Griz constructor (for testing)."""
    global _griz_factory
    _griz_factory = factory


def open_database(path: str) -> str:
    """Open a database, creating a new Griz session.

    In attach mode (GRIZ_MCP_ATTACH_RENDEZVOUS set), `path` is ignored
    and we attach to the peer's server instead. The returned state
    reflects whatever the peer has open.
    """
    global _session
    if _session is not None and _session.is_open:
        _session.close()
    _session = _griz_factory()
    attach_rv = os.environ.get(_ATTACH_ENV)
    if attach_rv:
        _session.attach(attach_rv)
    else:
        _session.open(path)
    state = _session.state()
    return json.dumps(state, default=str)


def close_database() -> str:
    """Close the current database and tear down the session."""
    global _session
    if _session is not None:
        _session.close()
        _session = None
    return "Database closed."


def require_session() -> Griz:
    """Return the active session or raise."""
    if _session is None or not _session.is_open:
        raise RuntimeError("No database is open. Call open_database first.")
    return _session


def get_status() -> str:
    """Return a summary of the current session state."""
    if _session is None or not _session.is_open:
        return "No database is open."
    state = _session.state()
    return json.dumps(state, default=str)


def restart() -> str:
    """Close and discard the current session."""
    global _session
    if _session is not None:
        try:
            _session.close()
        except Exception:
            pass
        _session = None
    return "Session restarted."


def reset() -> None:
    """Reset module state (used by test fixtures)."""
    global _session, _griz_factory
    if _session is not None:
        try:
            _session.close()
        except Exception:
            pass
        _session = None
    _griz_factory = Griz
