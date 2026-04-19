"""Picking and selection helpers.

Thin wrappers over Griz's selection commands. Exposed as top-level
`Griz.select` / `Griz.highlight` / `Griz.clear_picks` — see
03-python-api.md §1.2. The underlying command names below match the
Griz command language in Src/interpret.c (`select`, `hilite`, `clrhil`).
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Iterable

if TYPE_CHECKING:
    from griz.session import Griz


def _fmt_ids(ids: Iterable[int]) -> str:
    parts = [str(int(i)) for i in ids]
    if not parts:
        raise ValueError("at least one id is required")
    return " ".join(parts)


class SelectionAPI:
    def __init__(self, griz: "Griz") -> None:
        self._griz = griz

    def select(self, klass: str, ids: Iterable[int]) -> dict:
        """Select entities of a given class (e.g. `node`, `brick`)."""
        self._griz._require_worker().cmd(f"select {klass} {_fmt_ids(ids)}")
        return self._griz.state()

    def highlight(self, klass: str, ids: Iterable[int]) -> dict:
        """Highlight entities of a given class."""
        self._griz._require_worker().cmd(f"hilite {klass} {_fmt_ids(ids)}")
        return self._griz.state()

    def clear_picks(self) -> dict:
        """Clear all selections and highlights."""
        self._griz._require_worker().cmd("clrhil")
        return self._griz.state()
