"""Field / result operations.

`show()` resolves a (field, component) pair through the curated results
map and then drives the server with `show <griz-name>`. When `name`
isn't in the curated map and no component is given, it's treated as a
raw Griz/Mili command-language name (e.g. `sx`, `seff`, `eps`) and
passed through unchanged — this matches what `list_fields()` returns
and avoids forcing callers to translate every Mili name through the
curated abstraction.

`list()` uses the server's `q_results` query to enumerate available
fields. `info()` uses `q_result_info` (not yet implemented server-side).
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from griz.exceptions import UnknownFieldError
from griz.results_map import default_map

if TYPE_CHECKING:
    from griz.session import Griz


def _resolve_with_passthrough(name: str, component: str | None) -> str:
    """Resolve via the curated map; pass `name` through if it's an
    unrecognized family and no component was provided.

    Two cases that still raise (rather than passing through):
      * known family without a component — the curated map's error
        ("requires a component; available: …") is more helpful than
        a downstream server rejection.
      * unknown family with a component — unambiguously a typo.
    """
    rmap = default_map()
    try:
        return rmap.resolve(name, component)
    except UnknownFieldError:
        if component is None and name not in rmap.fields():
            return name
        raise


class FieldAPI:
    def __init__(self, griz: "Griz") -> None:
        self._griz = griz

    def show(self, name: str, *, component: str | None = None) -> dict:
        """Resolve and display a field.

        Returns the state dict after the show completes.
        """
        griz_name = _resolve_with_passthrough(name, component)
        self._griz._require_worker().cmd(f"show {griz_name}")
        return self._griz.state()

    def list(self) -> list[dict]:
        """List fields the current database exposes (requires q_results)."""
        resp = self._griz._require_worker().cmd("q_results")
        data = resp.get("data") or {}
        return list(data.get("results") or [])

    def info(self, name: str, *, component: str | None = None) -> dict:
        """Return metadata for a field (requires q_result_info)."""
        griz_name = _resolve_with_passthrough(name, component)
        resp = self._griz._require_worker().cmd(f"q_result_info {griz_name}")
        return dict(resp.get("data") or {})
