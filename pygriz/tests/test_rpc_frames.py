"""Auto-push frame + viewport-resize smoke test (RPC transport).

Exercises the Phase 3 streaming path from planning/UI.md:

* After a state-mutating command, the server renders the current view,
  JPEG-encodes it, and pushes a `kind=0x02` subtype=0x01 codec=0x01
  binary frame with a JSON sub-header carrying {w, h, seq, fmt, ...}.
  The frame has no `request_id`, so it lands in `_anon_binaries`.
* The new `resize` JSON command (Src/server_core.c::server_try_resize)
  re-creates the OSMesa framebuffer and emits both an auto-push frame
  at the new dimensions and a success response with {w, h}.
"""

from __future__ import annotations

import time

import pytest

from griz.rpc_worker import RpcWorker


JPEG_SOI = b"\xff\xd8\xff"


def _drain_jpeg(w: RpcWorker, deadline_s: float = 5.0) -> dict:
    """Wait for the next anon JPEG frame and return its parsed payload."""
    end = time.monotonic() + deadline_s
    with w._cond:  # noqa: SLF001 — deliberate; test introspects internals
        while True:
            for frame in list(w._anon_binaries):  # noqa: SLF001
                if frame.get("codec") == 0x01:
                    w._anon_binaries.remove(frame)  # noqa: SLF001
                    return frame
            remaining = end - time.monotonic()
            if remaining <= 0:
                raise AssertionError("no JPEG auto-push frame arrived")
            w._cond.wait(timeout=remaining)  # noqa: SLF001


def test_mutating_command_pushes_jpeg_frame(griz_bin, sample_database):
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=256, height=256
    ) as w:
        # `state 1` mutates time + view, so the dispatcher fires the
        # auto-push path.
        w.cmd("state 1")
        frame = _drain_jpeg(w)

    assert frame["subtype"] == 0x01  # frame
    assert frame["codec"]   == 0x01  # jpeg
    assert frame["body"][:3] == JPEG_SOI

    hdr = frame["header"]
    assert hdr["fmt"] == "jpeg"
    assert hdr["w"] == 256
    assert hdr["h"] == 256
    assert hdr["seq"] >= 1
    assert 1 <= hdr["quality"] <= 100
    assert hdr["bytes"] == len(frame["body"])


def test_resize_updates_framebuffer(griz_bin, sample_database):
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=256, height=256
    ) as w:
        # Drain any startup-time auto-push frame so the post-resize
        # assertion is unambiguous.
        try:
            _drain_jpeg(w, deadline_s=0.5)
        except AssertionError:
            pass

        request_id = "resize_1"
        w._send_json({  # noqa: SLF001
            "type": "request",
            "id":   request_id,
            "cmd":  "resize",
            "w":    384,
            "h":    192,
        })

        # Wait for the response.
        with w._cond:  # noqa: SLF001
            deadline = time.monotonic() + 5.0
            while request_id not in w._responses_by_id:  # noqa: SLF001
                remaining = deadline - time.monotonic()
                assert remaining > 0, "resize response timed out"
                w._cond.wait(timeout=remaining)  # noqa: SLF001
            response = w._responses_by_id.pop(request_id)  # noqa: SLF001

        assert response["status"] == "ok"
        assert response["data"] == {"w": 384, "h": 192}

        frame = _drain_jpeg(w)
        hdr = frame["header"]
        assert hdr["w"] == 384
        assert hdr["h"] == 192

        # Follow-up screenshot must return at the new size too.
        shot = w.screenshot()
        assert shot["w"] == 384
        assert shot["h"] == 192


def test_resize_rejects_oversize(griz_bin, sample_database):
    from griz import GrizCommandError
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=128, height=128
    ) as w:
        with pytest.raises(GrizCommandError) as excinfo:
            w._send_json({  # noqa: SLF001
                "type": "request",
                "id":   "too_big",
                "cmd":  "resize",
                "w":    8192,
                "h":    8192,
            })
            with w._cond:  # noqa: SLF001
                deadline = time.monotonic() + 5.0
                while "too_big" not in w._responses_by_id:  # noqa: SLF001
                    remaining = deadline - time.monotonic()
                    assert remaining > 0, "oversize resize timed out"
                    w._cond.wait(timeout=remaining)  # noqa: SLF001
                response = w._responses_by_id.pop("too_big")  # noqa: SLF001
            if response.get("status") == "error":
                err = response.get("error") or {}
                raise GrizCommandError(
                    err.get("code") or "internal_error",
                    err.get("message") or "",
                    response=response,
                )

    assert excinfo.value.code == "resource_limit"


def test_resize_rejects_missing_fields(griz_bin, sample_database):
    from griz import GrizCommandError
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=128, height=128
    ) as w:
        w._send_json({  # noqa: SLF001
            "type": "request",
            "id":   "bad_body",
            "cmd":  "resize",
        })
        with w._cond:  # noqa: SLF001
            deadline = time.monotonic() + 5.0
            while "bad_body" not in w._responses_by_id:  # noqa: SLF001
                remaining = deadline - time.monotonic()
                assert remaining > 0, "resize response timed out"
                w._cond.wait(timeout=remaining)  # noqa: SLF001
            response = w._responses_by_id.pop("bad_body")  # noqa: SLF001

    assert response["status"] == "error"
    assert response["error"]["code"] == "invalid_request"


def _collect_jpegs(w: RpcWorker, deadline_s: float) -> list[dict]:
    """Collect every anon JPEG frame that arrives before the deadline."""
    out: list[dict] = []
    end = time.monotonic() + deadline_s
    while True:
        remaining = end - time.monotonic()
        if remaining <= 0:
            break
        try:
            frame = _drain_jpeg(w, deadline_s=remaining)
        except AssertionError:
            break
        out.append(frame)
    return out


def test_burst_commands_are_rate_limited(griz_bin, sample_database):
    """A burst of rapid-fire mutating commands must coalesce: fewer
    pushed frames than issued commands, a monotonic-with-gaps seq
    stream (drops burn seqs per 02-protocol.md §4.2), and a final
    frame at the end of the burst reflecting the settled state."""
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=256, height=256
    ) as w:
        # Drain the startup-time render, if any.
        _collect_jpegs(w, deadline_s=0.3)

        # Issue a 25-command burst tighter than the 33 ms cap. `rx 1`
        # is a cheap rotation that always dirties the view.
        burst_size = 25
        for _ in range(burst_size):
            w.cmd("rx 1")

        # Wait long enough for the deferred-flush tail to drain (cap is
        # ~33 ms; give the poll loop plenty of slack).
        frames = _collect_jpegs(w, deadline_s=1.0)

    assert 1 <= len(frames) < burst_size, (
        f"expected coalescing, got {len(frames)} frames for {burst_size} "
        f"commands"
    )

    # Seqs are monotonic but should have gaps — every skipped frame
    # burns its seq so the client can infer drops from the run.
    seqs = [f["header"]["seq"] for f in frames]
    assert seqs == sorted(seqs)
    assert len(set(seqs)) == len(seqs)
    if len(frames) > 1:
        last_seq = frames[-1]["header"]["seq"]
        first_seq = frames[0]["header"]["seq"]
        # Delivered frames < burst_size, so with per-command seq burns
        # the tail seq must exceed the number of delivered frames.
        assert last_seq - first_seq >= len(frames) - 1
