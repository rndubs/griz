"""Time / state navigation.

Wraps Griz's `state N`, `time T`, `next` / `prev` commands and provides
a local `animate` helper that walks the state range sequentially.
"""

from __future__ import annotations

import time as _time
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from griz.session import Griz


class TimeAPI:
    def __init__(self, griz: "Griz") -> None:
        self._griz = griz

    def set_state(self, state: int) -> dict:
        self._griz._require_worker().cmd(f"state {int(state)}")
        return self._griz.state()

    def set_time(self, value: float) -> dict:
        self._griz._require_worker().cmd(f"time {float(value)}")
        return self._griz.state()

    def next(self) -> dict:
        self._griz._require_worker().cmd("next")
        return self._griz.state()

    def prev(self) -> dict:
        self._griz._require_worker().cmd("prev")
        return self._griz.state()

    def animate(
        self,
        *,
        start: int | None = None,
        end: int | None = None,
        step: int = 1,
        delay: float = 0.0,
    ) -> list[dict]:
        """Iterate through states locally, returning the state dict per tick."""
        if step == 0:
            raise ValueError("animate step must be non-zero")

        current = self._griz.state()
        state_start = int(start if start is not None else current.get("time_state", 0))
        state_end = int(
            end if end is not None
            else current.get("max_time_state", state_start)
        )

        if step > 0:
            states = range(state_start, state_end + 1, step)
        else:
            states = range(state_start, state_end - 1, step)

        frames: list[dict] = []
        for s in states:
            frames.append(self.set_state(s))
            if delay > 0:
                _time.sleep(delay)
        return frames
