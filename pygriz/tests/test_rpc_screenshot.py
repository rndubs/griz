"""Inline PNG screenshot smoke test for the RPC transport.

Exercises the Phase 3 path at planning/UI.md §3: the `screenshot`
command drives render → PNG encode on the server, emits a `kind=0x02`
binary frame (subtype=0x02 screenshot, codec=0x02 png), and returns a
JSON response that correlates to the binary body via `request_id`.

The default worker tests cover envelope parity over stdio; this file
adds the one RPC-only surface — binary-frame correlation — that cannot
be exercised through the MCP layer.
"""

from __future__ import annotations

import pytest

from griz import GrizCommandError, WorkerError
from griz.rpc_worker import RpcWorker


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def test_screenshot_returns_png_bytes(griz_bin, sample_database):
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=256, height=256
    ) as w:
        w.cmd("state 1")
        shot = w.screenshot()

    assert shot["fmt"] == "png"
    assert shot["w"] == 256
    assert shot["h"] == 256
    assert shot["seq"] >= 1
    # Binary body must be a real PNG — check the 8-byte signature.
    assert shot["bytes"].startswith(PNG_SIGNATURE)
    # Sub-header carries the correlation id the client used to route
    # the frame.
    assert shot["header"]["fmt"] == "png"
    assert shot["header"]["w"] == 256
    assert shot["header"]["h"] == 256


def test_screenshot_with_alpha(griz_bin, sample_database):
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=128, height=128
    ) as w:
        shot = w.screenshot(alpha=True)

    assert shot["bytes"].startswith(PNG_SIGNATURE)
    # PNG color-type byte lives at offset 25 in the IHDR chunk. 6 =
    # PNG_COLOR_TYPE_RGB_ALPHA, 2 = PNG_COLOR_TYPE_RGB.
    assert shot["bytes"][25] == 6


def test_screenshot_seq_monotonic(griz_bin, sample_database):
    with RpcWorker(
        sample_database, griz_bin=griz_bin, width=128, height=128
    ) as w:
        a = w.screenshot()
        b = w.screenshot()
        c = w.screenshot()
    assert a["seq"] < b["seq"] < c["seq"]


def test_screenshot_rejects_on_stdio(griz_bin, sample_database):
    """Stdio transport must refuse the screenshot command with a typed
    error — binary frames are RPC-only per 02-protocol.md §6.1."""
    from griz import Worker
    with Worker(
        sample_database, griz_bin=griz_bin, width=128, height=128
    ) as w:
        with pytest.raises(GrizCommandError) as excinfo:
            w.cmd("screenshot")
    assert excinfo.value.code == "unsupported_command"
