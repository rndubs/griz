"""Phase 4 smoke test: picking + element/node queries (RPC transport).

Exercises the pieces landed under planning/UI.md Phase 4:

* `q_node <id>` and `q_element <id>` — new metadata queries (06 §6.1).
* `pick_at <x> <y> <mode>` — ID-buffer pass + hilite mutation + typed
  response (06 §2.1, §10 step 2). A pick at coordinates far outside
  the viewport is a guaranteed miss; a pick at the viewport centre
  may hit or miss depending on the camera, but must never crash and
  the response shape must be well-formed in both cases.
* After a hit, `q_selection.highlighted` must reflect the picked
  primitive (06 §5.2).
"""

from __future__ import annotations

from pathlib import Path

import pytest

from griz.rpc_worker import RpcWorker


REPO_ROOT = Path(__file__).resolve().parents[2]
HEX_DB = REPO_ROOT / "Src" / "test" / "image" / "hex_strain" / "hex_strain.pltA"


@pytest.fixture(scope="session")
def hex_database() -> Path:
    if not HEX_DB.exists():
        pytest.skip(f"hex sample database not found at {HEX_DB}")
    return HEX_DB


def test_q_node_returns_coords(griz_bin, sample_database):
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=256, height=256
    ) as w:
        response = w.cmd("q_node 1")

    data = response["data"]
    assert data is not None
    assert data["kind"] == "node"
    assert data["id"] == 1
    assert data["index"] == 1
    # Coords may be a 2- or 3-vector depending on mesh dimension.
    assert isinstance(data["coords"], list)
    assert len(data["coords"]) in (2, 3)
    assert all(isinstance(c, (int, float)) for c in data["coords"])
    # MVP-deferred fields must be present-but-null so clients can rely
    # on the response shape.
    assert data["displacement"] is None
    assert data["attached_elements"] is None


def test_q_node_unknown_id_returns_error(griz_bin, sample_database):
    from griz import GrizCommandError
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=128, height=128
    ) as w:
        with pytest.raises(GrizCommandError) as excinfo:
            w.cmd("q_node 999999")
    assert excinfo.value.code == "not_found"


def test_q_element_returns_connectivity(griz_bin, sample_database):
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=256, height=256
    ) as w:
        response = w.cmd("q_element 1")

    data = response["data"]
    assert data is not None
    assert data["id"] == 1
    # `kind` is the class short_name (e.g. "brick", "beam"); surface it
    # unchanged but require it to be a non-empty string.
    assert isinstance(data["kind"], str) and data["kind"]
    assert isinstance(data["type"], str) and data["type"]
    # Connectivity must be a list of positive-integer node labels. Its
    # length matches the element type; we don't care about exact count
    # here — every supported element type has >= 2 nodes.
    assert isinstance(data["connectivity"], list)
    assert len(data["connectivity"]) >= 2
    assert all(isinstance(n, int) and n >= 1 for n in data["connectivity"])
    # result_value_per_int_pt is post-MVP (06 §11); shape-stable null.
    assert data["result_value_per_int_pt"] is None


def test_q_element_unknown_id_returns_error(griz_bin, sample_database):
    from griz import GrizCommandError
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=128, height=128
    ) as w:
        with pytest.raises(GrizCommandError) as excinfo:
            w.cmd("q_element 999999")
    assert excinfo.value.code == "not_found"


def test_pick_at_out_of_viewport_is_miss(griz_bin, sample_database):
    """A pick_at with coordinates outside the rendered viewport is the
    simplest possible miss path: the render pass is skipped entirely
    and we synthesise `data=null`."""
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=128, height=128
    ) as w:
        response = w.cmd("pick_at -50 -50 any")

    assert response["status"] == "ok"
    assert response["data"] is None


def test_pick_at_invalid_syntax_errors(griz_bin, sample_database):
    from griz import GrizCommandError
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=128, height=128
    ) as w:
        with pytest.raises(GrizCommandError) as excinfo:
            w.cmd("pick_at notanumber")
    assert excinfo.value.code == "invalid_syntax"


def test_pick_at_hit_updates_hilite(griz_bin, hex_database):
    """Point-pick over a hex mesh must land on a brick face for at least
    one pixel in a scanned grid. On a hit, q_selection.highlighted must
    reflect the new singleton (confirms the pick_at → griz_set_hilite →
    state_changed pipeline is wired end-to-end)."""
    width = 256
    height = 256
    with RpcWorker(
        hex_database, griz_bin=griz_bin, width=width, height=height
    ) as w:
        # Fit the mesh in the viewport before scanning.
        w.cmd("rview")
        w.cmd("state 1")

        hit_data = None
        # 7x7 grid skipping the borders — should cover a primitive on
        # any reasonable default camera.
        for y in range(height // 8, height, height // 8):
            for x in range(width // 8, width, width // 8):
                data = w.cmd(f"pick_at {x} {y} any")["data"]
                if data is not None:
                    hit_data = data
                    break
            if hit_data is not None:
                break

        assert hit_data is not None, (
            "pick_at missed every pixel in the 7x7 grid over a hex mesh — "
            "either the ID-buffer pass isn't covering hex faces, or the "
            "default camera orients the mesh outside the viewport"
        )
        assert isinstance(hit_data["kind"], str) and hit_data["kind"]
        assert isinstance(hit_data["id"], int) and hit_data["id"] >= 1

        sel = w.cmd("q_selection")["data"]
        hl = sel["highlighted"]
        assert hl is not None
        assert hl["kind"] == hit_data["kind"]
        assert hl["id"] == hit_data["id"]
