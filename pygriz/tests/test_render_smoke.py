"""End-to-end smoke for RenderAPI against a real griz-server.

Skips automatically when the server binary or sample database is
absent. The unit-style coverage in test_render.py exercises the
whitelist + dispatch logic with a stub; this just checks the
real-server round-trip.
"""

from __future__ import annotations

from griz import Griz


def test_show_hide_round_trip(griz_bin, sample_database) -> None:
    with Griz(sample_database, griz_bin=griz_bin) as g:
        # Both off in the default render state — confirm before flipping.
        before = g.render.state()
        assert before.get("title") is False
        assert before.get("time") is False

        g.render.show("title", "time")
        after_show = g.render.state()
        assert after_show["title"] is True
        assert after_show["time"] is True

        g.render.hide("title")
        after_hide = g.render.state()
        assert after_hide["title"] is False
        # `time` stayed on; combined hide didn't drag it down with title.
        assert after_hide["time"] is True


def test_set_toggles_typed_form(griz_bin, sample_database) -> None:
    with Griz(sample_database, griz_bin=griz_bin) as g:
        g.render.set_toggles(title=True, edges=False, cmap=True)
        s = g.render.state()
        assert s["title"] is True
        assert s["edges"] is False
        assert s["cmap"] is True


def test_expanded_vocabulary_round_trips(griz_bin, sample_database) -> None:
    """R7: every name in RenderAPI.KNOWN must be settable AND readable.

    Guards the invariant that the whitelist mirrors what the server
    exposes via q_state — anything we let users *set* must come back
    in `render.toggles` so set→state→assert flows work.
    """
    from griz.render import RenderAPI

    with Griz(sample_database, griz_bin=griz_bin) as g:
        s = g.render.state()
        # Every known name appears in q_state's toggle block.
        assert RenderAPI.KNOWN == frozenset(s.keys())

        # Flip every R7-added toggle on and confirm round-trip.
        new_names = ["path", "cscale", "scale", "date", "tinfo"]
        g.render.show(*new_names)
        s = g.render.state()
        for name in new_names:
            assert s[name] is True, f"{name} should round-trip True"
