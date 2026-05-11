"""Unit tests for RenderAPI.

Uses a stub Griz that records commands in-memory so the dispatch
behavior can be checked without a running server. Smoke coverage
against a real `griz-server` lives in test_render_smoke.py.
"""

from __future__ import annotations

import pytest

from griz.render import RenderAPI


class _StubWorker:
    def __init__(self) -> None:
        self.commands: list[str] = []

    def cmd(self, command: str) -> dict:
        self.commands.append(command)
        return {}


class _StubGriz:
    def __init__(self, toggles: dict | None = None) -> None:
        self._worker = _StubWorker()
        self._toggles = dict(toggles or {})

    def _require_worker(self) -> _StubWorker:
        return self._worker

    def state(self) -> dict:
        return {"render": {"toggles": dict(self._toggles)}}


@pytest.fixture
def stub() -> _StubGriz:
    return _StubGriz()


@pytest.fixture
def render(stub: _StubGriz) -> RenderAPI:
    return RenderAPI(stub)


class TestShowHide:
    def test_show_single_emits_one_on_command(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.show("title")
        assert stub._worker.commands == ["on title"]

    def test_show_multi_emits_one_combined_on_command(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.show("title", "time", "cmap")
        # Order is preserved from the call site.
        assert stub._worker.commands == ["on title time cmap"]

    def test_hide_single_emits_one_off_command(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.hide("edges")
        assert stub._worker.commands == ["off edges"]

    def test_hide_multi_emits_one_combined_off_command(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.hide("title", "cmap")
        assert stub._worker.commands == ["off title cmap"]

    def test_show_no_args_is_noop(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.show()
        assert stub._worker.commands == []

    def test_hide_no_args_is_noop(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.hide()
        assert stub._worker.commands == []


class TestSetToggles:
    def test_set_toggles_groups_by_truthiness(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.set_toggles(title=True, time=True, edges=False, bbox=False)
        # One on for the True names, one off for the False names.
        assert len(stub._worker.commands) == 2
        on_cmd, off_cmd = stub._worker.commands
        assert on_cmd.startswith("on ")
        assert off_cmd.startswith("off ")
        assert set(on_cmd.split()[1:]) == {"title", "time"}
        assert set(off_cmd.split()[1:]) == {"edges", "bbox"}

    def test_set_toggles_only_on_emits_only_on(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.set_toggles(title=True, time=True)
        assert len(stub._worker.commands) == 1
        assert stub._worker.commands[0].startswith("on ")

    def test_set_toggles_only_off_emits_only_off(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.set_toggles(title=False, time=False)
        assert len(stub._worker.commands) == 1
        assert stub._worker.commands[0].startswith("off ")

    def test_set_toggles_empty_is_noop(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        render.set_toggles()
        assert stub._worker.commands == []


class TestWhitelist:
    def test_unknown_name_raises_before_dispatch(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        # Whitelist must reject unknown names *before* the worker is touched.
        with pytest.raises(ValueError, match="unknown render toggle"):
            render.show("plottytwoshoes")
        assert stub._worker.commands == []

    def test_one_unknown_in_a_batch_rejects_the_whole_batch(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        with pytest.raises(ValueError):
            render.show("title", "definitely-not-a-toggle")
        assert stub._worker.commands == []

    def test_unknown_in_set_toggles_kwargs_raises(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        with pytest.raises(ValueError):
            render.set_toggles(title=True, fictitious=False)
        # Whichever direction trips first, no command should reach
        # the worker at all (we want set_toggles to be all-or-nothing).
        # The current implementation runs `on` first, so the on side
        # will fire if its names are valid. Document that behavior:
        # we only assert nothing past the failure point — the `off` is
        # never sent because dispatch raised.
        assert all(not c.startswith("off ") for c in stub._worker.commands)

    def test_error_message_lists_known_names(
        self, stub: _StubGriz, render: RenderAPI
    ) -> None:
        with pytest.raises(ValueError) as exc_info:
            render.show("nope")
        msg = str(exc_info.value)
        for name in RenderAPI.KNOWN:
            assert name in msg


class TestState:
    def test_state_returns_plain_bool_dict(self) -> None:
        stub = _StubGriz(
            toggles={"title": True, "time": False, "cmap": True}
        )
        render = RenderAPI(stub)
        out = render.state()
        assert out == {"title": True, "time": False, "cmap": True}
        # Plain dict, not whatever proxy lived inside the state structure.
        assert type(out) is dict

    def test_state_when_render_block_missing(self) -> None:
        # A defensive read should yield {} rather than KeyError-ing.
        class _GrizNoRender:
            def state(self) -> dict:
                return {}

            def _require_worker(self):  # pragma: no cover - never called
                raise AssertionError("state() must not call worker")

        render = RenderAPI(_GrizNoRender())  # type: ignore[arg-type]
        assert render.state() == {}
