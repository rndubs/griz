"""Module-level Griz session singleton.

MCP is single-client-per-server, so a module-level session is the correct
model.  A ``_griz_factory`` callable allows test injection without patching.

Attach mode (planning/DEMO.md task C): when GRIZ_MCP_ATTACH_RENDEZVOUS
is set, ``open_database`` attaches to a running Qt UI's griz-server
rather than spawning its own. The ``path`` argument is ignored — the
peer already chose which database to open — and we return the live
q_state so the LLM sees what's currently loaded.

Resolution order for the attach target:
  1. The env var's literal value, if it points at a live rendezvous
     (file exists, embedded ``server_pid`` is alive).
  2. Otherwise scan ``$HOME/.griz/rendezvous/ui-*.json`` and pick the
     most recently modified file whose ``server_pid`` is alive.

This means a sentinel value like ``GRIZ_MCP_ATTACH_RENDEZVOUS=auto``
opts in to attach mode without pinning a specific PID, and a stale
path (e.g. after restarting the Qt UI) silently recovers as long as
*some* live rendezvous exists.
"""

from __future__ import annotations

import glob
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


def _is_pid_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        # Process exists but is owned by another user.
        return True
    return True


def _is_rendezvous_live(path: str) -> bool:
    try:
        with open(path) as f:
            data = json.load(f)
    except (OSError, ValueError):
        return False
    return _is_pid_alive(int(data.get("server_pid", 0)))


def _discover_live_rendezvous() -> str | None:
    """Return the most recently modified live rendezvous, or None."""
    home = os.path.expanduser("~")
    pattern = os.path.join(home, ".griz", "rendezvous", "ui-*.json")
    candidates = sorted(
        glob.glob(pattern),
        key=lambda p: os.path.getmtime(p),
        reverse=True,
    )
    for p in candidates:
        if _is_rendezvous_live(p):
            return p
    return None


def _resolve_attach_path() -> tuple[str | None, str | None]:
    """Return ``(resolved_path, env_value)``.

    ``env_value`` is the raw env var (None if unset). ``resolved_path``
    is the path to attach to: the env value if it's live, else a
    discovered live rendezvous, else None.
    """
    env_value = os.environ.get(_ATTACH_ENV)
    if not env_value:
        return None, None
    if _is_rendezvous_live(env_value):
        return env_value, env_value
    return _discover_live_rendezvous(), env_value


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
    resolved, env_value = _resolve_attach_path()
    if resolved is not None:
        _session.attach(resolved)
    elif env_value is not None:
        raise RuntimeError(
            f"{_ATTACH_ENV}={env_value!r} requested attach mode, but no "
            "live griz-server rendezvous was found (the named file is "
            "stale or missing, and no other live ui-*.json exists under "
            "$HOME/.griz/rendezvous/). Start a Qt client first, or unset "
            f"{_ATTACH_ENV} for spawn mode."
        )
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
