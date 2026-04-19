"""Material visibility controls.

Griz exposes `vis <ids…>` and `invis <ids…>` for material visibility
(see Src/interpret.c ~line 3156). `list` and `show_only` use the
server's `q_materials` query to enumerate materials.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Iterable

if TYPE_CHECKING:
    from griz.session import Griz


def _fmt_ids(material_ids: Iterable[int]) -> str:
    parts = [str(int(m)) for m in material_ids]
    if not parts:
        raise ValueError("material_ids must contain at least one id")
    return " ".join(parts)


class MaterialsAPI:
    def __init__(self, griz: "Griz") -> None:
        self._griz = griz

    def hide(self, material_ids: Iterable[int]) -> dict:
        ids = _fmt_ids(material_ids)
        self._griz._require_worker().cmd(f"invis {ids}")
        return self._griz.state()

    def show(self, material_ids: Iterable[int]) -> dict:
        ids = _fmt_ids(material_ids)
        self._griz._require_worker().cmd(f"vis {ids}")
        return self._griz.state()

    def show_only(self, material_ids: Iterable[int]) -> dict:
        """Show only the given materials; hide all others.

        Requires `q_materials` support in the server to enumerate all
        material ids.
        """
        wanted = {int(m) for m in material_ids}
        if not wanted:
            raise ValueError("material_ids must contain at least one id")

        all_ids = {int(m["id"]) for m in self.list()}
        to_hide = sorted(all_ids - wanted)
        to_show = sorted(wanted)

        worker = self._griz._require_worker()
        if to_hide:
            worker.cmd(f"invis {' '.join(str(m) for m in to_hide)}")
        if to_show:
            worker.cmd(f"vis {' '.join(str(m) for m in to_show)}")
        return self._griz.state()

    def list(self) -> list[dict]:
        """List materials in the current database (requires q_materials)."""
        resp = self._griz._require_worker().cmd("q_materials")
        data = resp.get("data") or {}
        return list(data.get("materials") or [])
