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

The sentinel value ``GRIZ_MCP_ATTACH_RENDEZVOUS=auto`` (case-insensitive)
opts in to attach mode without pinning a specific PID and silently
falls through to spawn mode when no live UI is found — this is what
``.mcp.json`` ships as the default. Any other literal value names a
specific path; if that path is stale and no other live rendezvous
exists, ``open_database`` raises rather than silently spawning, since
the user clearly meant to attach.
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
_attached_rendezvous: str | None = None

_ATTACH_ENV = "GRIZ_MCP_ATTACH_RENDEZVOUS"
_ATTACH_AUTO_SENTINEL = "auto"


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


def _is_auto_sentinel(env_value: str | None) -> bool:
    return bool(env_value) and env_value.strip().lower() == _ATTACH_AUTO_SENTINEL


def _resolve_attach_path() -> tuple[str | None, str | None]:
    """Return ``(resolved_path, env_value)``.

    ``env_value`` is the raw env var (None if unset). ``resolved_path``
    is the path to attach to: the env value if it's live, else a
    discovered live rendezvous, else None.

    The ``auto`` sentinel skips the literal-path check and goes straight
    to discovery, so a non-existent ``auto`` file isn't reported as a
    stale literal path.
    """
    env_value = os.environ.get(_ATTACH_ENV)
    if not env_value:
        return None, None
    if _is_auto_sentinel(env_value):
        return _discover_live_rendezvous(), env_value
    if _is_rendezvous_live(env_value):
        return env_value, env_value
    return _discover_live_rendezvous(), env_value


def open_database(path: str | None = None) -> str:
    """Open a database, creating a new Griz session.

    In attach mode (GRIZ_MCP_ATTACH_RENDEZVOUS set), `path` is ignored
    and we attach to the peer's server instead. The returned state
    reflects whatever the peer has open.

    Idempotency: if the current session is already attached to the
    resolved live rendezvous, returns the current state without
    tearing down — avoids a UI flicker on repeated calls.

    The ``auto`` sentinel for the env var falls through to spawn mode
    when no live UI is found; any other literal value that's stale and
    has no live alternative raises so the user can fix the path.
    """
    global _session, _attached_rendezvous
    resolved, env_value = _resolve_attach_path()

    if (
        _session is not None
        and _session.is_open
        and resolved is not None
        and _attached_rendezvous == resolved
    ):
        return json.dumps(_session.state(), default=str)

    if _session is not None and _session.is_open:
        _session.close()
    _session = _griz_factory()
    _attached_rendezvous = None

    if resolved is not None:
        _session.attach(resolved)
        _attached_rendezvous = resolved
    elif env_value is not None and not _is_auto_sentinel(env_value):
        raise RuntimeError(
            f"{_ATTACH_ENV}={env_value!r} requested attach mode, but no "
            "live griz-server rendezvous was found (the named file is "
            "stale or missing, and no other live ui-*.json exists under "
            "$HOME/.griz/rendezvous/). Start a Qt client first, or set "
            f"{_ATTACH_ENV}=auto to fall through to spawn mode when no "
            "UI is running."
        )
    else:
        if path is None:
            raise RuntimeError(
                "open_database requires a `path` argument when no Qt UI "
                "is running to attach to. Either pass a Mili plotfile "
                "path, or start a Qt client first (scripts/griz-ui.sh up)."
            )
        _session.open(path)
    state = _session.state()
    return json.dumps(state, default=str)


def close_database() -> str:
    """Close the current database and tear down the session."""
    global _session, _attached_rendezvous
    if _session is not None:
        _session.close()
        _session = None
    _attached_rendezvous = None
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


def status() -> str:
    """Return the MCP session lifecycle status as JSON.

    Pure local inspection — no commands sent to a worker. Resolves the
    current attach env, scans rendezvous files, and reports whatever's
    visible. Lets the LLM see "is a UI up, am I attached to it, what
    DB is open?" without any shelling out.
    """
    env_value = os.environ.get(_ATTACH_ENV)
    resolved, _ = _resolve_attach_path()

    payload: dict = {
        "session_open": False,
        "mode": "idle",
        "attach_env": env_value,
        "live_rendezvous": resolved,
    }

    if resolved is not None:
        server_pid = None
        try:
            with open(resolved) as fh:
                rv = json.load(fh)
            server_pid = int(rv.get("server_pid", 0)) or None
        except (OSError, ValueError):
            rv = {}
        payload["rendezvous"] = resolved
        payload["server_pid"] = server_pid
        payload["server_host"] = rv.get("host")
        payload["server_port"] = rv.get("port")

    if _session is not None and _session.is_open:
        payload["session_open"] = True
        payload["mode"] = "attach" if _attached_rendezvous else "spawn"
        attached_to = _attached_rendezvous
        payload["database"] = (
            str(_session.database_path) if _session.database_path else None
        )
        if attached_to:
            payload["rendezvous"] = attached_to

    return json.dumps(payload, default=str)


def restart() -> str:
    """Close and discard the current session."""
    global _session, _attached_rendezvous
    if _session is not None:
        try:
            _session.close()
        except Exception:
            pass
        _session = None
    _attached_rendezvous = None
    return "Session restarted."


def reset() -> None:
    """Reset module state (used by test fixtures)."""
    global _session, _griz_factory, _attached_rendezvous
    if _session is not None:
        try:
            _session.close()
        except Exception:
            pass
        _session = None
    _griz_factory = Griz
    _attached_rendezvous = None
