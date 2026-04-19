"""Camera / view controls.

Thin wrappers over Griz's rx/ry/rz, tx/ty/tz, scale, and home commands.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from griz.session import Griz


class ViewAPI:
    def __init__(self, griz: "Griz") -> None:
        self._griz = griz

    def rotate(self, *, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> dict:
        worker = self._griz._require_worker()
        if x:
            worker.cmd(f"rx {float(x)}")
        if y:
            worker.cmd(f"ry {float(y)}")
        if z:
            worker.cmd(f"rz {float(z)}")
        return self._griz.state()

    def translate(self, *, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> dict:
        worker = self._griz._require_worker()
        if x:
            worker.cmd(f"tx {float(x)}")
        if y:
            worker.cmd(f"ty {float(y)}")
        if z:
            worker.cmd(f"tz {float(z)}")
        return self._griz.state()

    def scale(self, factor: float) -> dict:
        self._griz._require_worker().cmd(f"scale {float(factor)}")
        return self._griz.state()

    def zoom(self, factor: float) -> dict:
        """Alias for `scale` to match the design doc surface."""
        return self.scale(factor)

    def reset(self) -> dict:
        self._griz._require_worker().cmd("rview")
        return self._griz.state()
