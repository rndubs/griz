"""Field / result operations.

`show()` resolves a human-readable (field, component) pair through the
results map and then drives the server with `show <griz-name>`. The
server commands used here come from Griz's interpret.c and the planning
notes in 03-python-api.md §4.1.

`list()` and `info()` depend on `q_results` / `q_result_info` which are
not yet implemented server-side (tracked in planning/shared/query-
commands.md). The stubs here let callers exercise the code path; they
will surface `GrizCommandError` with `code=="unknown_command"` until the
server-side query lands.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from griz.results_map import default_map

if TYPE_CHECKING:
    from griz.session import Griz


class FieldAPI:
    def __init__(self, griz: "Griz") -> None:
        self._griz = griz

    def show(self, name: str, *, component: str | None = None) -> dict:
        """Resolve and display a field.

        Returns the state dict after the show completes.
        """
        griz_name = default_map().resolve(name, component)
        self._griz._require_worker().cmd(f"show {griz_name}")
        return self._griz.state()

    def list(self) -> list[dict]:
        """List fields the current database exposes (requires q_results)."""
        resp = self._griz._require_worker().cmd("q_results")
        data = resp.get("data") or {}
        return list(data.get("results") or [])

    def info(self, name: str, *, component: str | None = None) -> dict:
        """Return metadata for a field (requires q_result_info)."""
        griz_name = default_map().resolve(name, component)
        resp = self._griz._require_worker().cmd(f"q_result_info {griz_name}")
        return dict(resp.get("data") or {})
