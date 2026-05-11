"""Plot-decoration toggles (title / time / cmap / minmax / coord / bbox / edges).

Wraps Griz's `on <names…>` / `off <names…>` commands. The whitelist
mirrors the toggles `q_state` round-trips under `render.toggles`
(Src/server_query.c:338) so every settable name is also readable —
unknown names raise `ValueError` *before* any command reaches the
server, which keeps typos from leaking into Griz syntax.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Iterable

if TYPE_CHECKING:
    from griz.session import Griz


class RenderAPI:
    # Mirrors Src/server_query.c:338 — every toggle here is settable
    # via `on`/`off` and readable via q_state's render.toggles block.
    KNOWN: frozenset[str] = frozenset(
        {
            "coord", "time", "cmap", "minmax", "title", "bbox", "edges",
            # R7 expansion — additional plot decorations.
            "path", "cscale", "scale", "date", "tinfo",
        }
    )

    def __init__(self, griz: "Griz") -> None:
        self._griz = griz

    def show(self, *names: str) -> dict:
        """Turn on one or more toggles. Returns the updated state."""
        self._dispatch("on", names)
        return self._griz.state()

    def hide(self, *names: str) -> dict:
        """Turn off one or more toggles. Returns the updated state."""
        self._dispatch("off", names)
        return self._griz.state()

    def set_toggles(self, **flags: bool) -> dict:
        """Set many toggles at once: ``set_toggles(title=True, edges=False)``.

        Groups by truthiness so each direction becomes a single
        ``on`` / ``off`` command instead of N commands. Empty call is a
        no-op that still returns the current state.
        """
        on = [k for k, v in flags.items() if v]
        off = [k for k, v in flags.items() if not v]
        self._dispatch("on", on)
        self._dispatch("off", off)
        return self._griz.state()

    def state(self) -> dict[str, bool]:
        """Return the current ``render.toggles`` block as ``{name: bool}``."""
        toggles = self._griz.state().get("render", {}).get("toggles", {})
        return {str(k): bool(v) for k, v in toggles.items()}

    def _dispatch(self, verb: str, names: Iterable[str]) -> None:
        names = list(names)
        if not names:
            return
        unknown = [n for n in names if n not in self.KNOWN]
        if unknown:
            raise ValueError(
                f"unknown render toggle(s): {unknown}. "
                f"Known: {sorted(self.KNOWN)}"
            )
        self._griz._require_worker().cmd(f"{verb} {' '.join(names)}")
