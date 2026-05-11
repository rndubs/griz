"""Phase 2 smoke tests for the JSON-protocol Worker."""

import time

import pytest

from griz import GRIZ_PROTOCOL_VERSION, GrizCommandError, Worker, WorkerError


def test_missing_database(tmp_path):
    with pytest.raises(FileNotFoundError):
        Worker(tmp_path / "does-not-exist.pltA")


def test_handshake_populates_server_info(griz_bin, sample_database):
    """Ready event + hello_ack should land in server_info."""
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        info = w.server_info
        assert info is not None
        assert info.get("version") == GRIZ_PROTOCOL_VERSION
        assert info.get("server") == "griz-server"
        # After handshake the merged record picks up hello_ack compatibility.
        assert info.get("compatible") is True


def test_clean_shutdown(griz_bin, sample_database):
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        w.cmd("state 1")
        w.cmd("rx 15")
    assert w.returncode == 0


def test_cmd_returns_json_response(griz_bin, sample_database):
    """`cmd()` round-trips a JSON request and surfaces the response dict."""
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        resp = w.cmd("state 1")
        assert resp["type"] == "response"
        assert resp["status"] == "ok"
        assert resp["id"].startswith("req_")


def test_outrgb_writes_image(griz_bin, sample_database, tmp_path):
    """`outrgb` should produce a non-empty SGI image file and return ok."""
    out = tmp_path / "frame.rgb"
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        w.cmd("state 1")
        resp = w.cmd(f"outrgb {out}")

    assert resp["status"] == "ok"
    assert out.exists() and out.stat().st_size > 0


def test_raw_send_command_produces_anon_response(griz_bin, sample_database):
    """Raw (non-JSON) input still generates a response line — consumed as anon."""
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        w.send_command("state 1")
        # give the reader thread a moment to pick up the response
        deadline = time.monotonic() + 5.0
        anon: list = []
        while time.monotonic() < deadline:
            anon = w.drain_anon_responses()
            if anon:
                break
            time.sleep(0.05)

    assert anon, "expected at least one anonymous response"
    first = anon[0]
    assert first["type"] == "response"
    assert first["status"] == "ok"
    assert first["id"] is None


def test_malformed_json_yields_invalid_request(griz_bin, sample_database):
    """A `{...}`-shaped line that is not valid JSON yields an error response."""
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        w.send_command('{"type":"request", "id":"bad", "cmd":')  # truncated
        deadline = time.monotonic() + 5.0
        anon: list = []
        while time.monotonic() < deadline:
            anon = w.drain_anon_responses()
            if anon:
                break
            time.sleep(0.05)

    assert anon, "expected at least one anonymous error response"
    err = anon[0]
    assert err["status"] == "error"
    assert err["error"]["code"] == "invalid_request"


def test_stdout_is_captured_into_response(griz_bin, sample_database):
    """Interpreter chatter should land in the response's stdout field,
    not interleave with the JSON stream. The `help` command reliably
    writes multi-line output via the command dispatcher."""
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        resp = w.cmd("help")
    # Something was written during `help` — either stdout or stderr,
    # depending on how the interpreter routes the text. The protocol
    # guarantee is "captured, not leaked" so at least one of them must
    # be non-empty; we should never see the raw text in recent_stdout.
    out = resp["stdout"]
    err = resp["stderr"]
    assert out or err, f"expected captured output for `help`, got stdout={out!r} stderr={err!r}"


def test_q_state_returns_structured_data(griz_bin, sample_database):
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        w.cmd("state 1")
        resp = w.cmd("q_state")

    assert resp["status"] == "ok"
    data = resp.get("data")
    assert isinstance(data, dict)
    time_block = data.get("time")
    assert isinstance(time_block, dict)
    assert "time_state" in time_block
    assert "max_time_state" in time_block
    assert "state_count" in time_block
    viewport = data.get("viewport")
    assert viewport == {"width": 256, "height": 256}


def test_q_time_returns_time_values(griz_bin, sample_database):
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        resp = w.cmd("q_time")
    data = resp["data"]
    assert "time_value" in data
    assert "max_time_value" in data
    assert data["max_time_state"] >= 0


def test_q_view_returns_viewport(griz_bin, sample_database):
    with Worker(sample_database, griz_bin=griz_bin, width=512, height=384) as w:
        resp = w.cmd("q_view")
    assert resp["data"]["viewport"] == {"width": 512, "height": 384}


def test_invalid_command_raises(griz_bin, sample_database):
    """An unknown griz command should surface as a GrizCommandError."""
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        with pytest.raises(GrizCommandError) as excinfo:
            w.cmd("totally-not-a-command")
    assert excinfo.value.code == "unknown_command"
    assert "not valid" in excinfo.value.message


def test_cmd_after_cleanup_fails(griz_bin, sample_database):
    w = Worker(sample_database, griz_bin=griz_bin, width=256, height=256)
    w.cleanup()
    with pytest.raises(WorkerError):
        w.cmd("state 1")


def test_raw_send_command_with_newline_rejected(griz_bin, sample_database):
    """Raw mode is strict about line framing — multi-line input must be rejected."""
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        with pytest.raises(ValueError):
            w.send_command("state 1\nstate 2")
