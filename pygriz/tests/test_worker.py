"""Phase 1 smoke tests for the minimal Worker."""

import json
import time
from pathlib import Path

import pytest

from griz import Worker, WorkerError


def test_missing_database(tmp_path):
    with pytest.raises(FileNotFoundError):
        Worker(tmp_path / "does-not-exist.pltA")


def test_clean_shutdown(griz_bin, sample_database):
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        w.send_command("state 1")
        w.send_command("rx 15")
    assert w.returncode == 0


def test_outrgb_writes_image(griz_bin, sample_database, tmp_path):
    """`outrgb` should produce a non-empty SGI image file.

    The Phase 1 server inherits a pre-existing batch-mode `double free`
    abort from the `outrgb` write path (tracked separately); it kills the
    process *after* the file is flushed to disk, so a non-zero size is the
    relevant assertion here, not the return code.
    """
    out = tmp_path / "frame.rgb"
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        w.send_command("state 1")
        w.send_command(f"outrgb {out}")
        # let the write flush before cleanup tears down stdin
        time.sleep(0.5)

    assert out.exists() and out.stat().st_size > 0


def test_json_envelope_response(griz_bin, sample_database):
    """Phase 2: each executed command produces a single JSON response line.

    Verifies both accepted input forms:
      - raw command string → response with id:null
      - JSON request with `id` → response echoing the same id
    """
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        w.send_command("state 1")
        w.send_command('{"type":"request","id":"req_1","cmd":"rx 15"}')
        # let the responses land in the drain thread
        time.sleep(0.5)
        stdout_lines = w.recent_stdout()

    responses = []
    for line in stdout_lines:
        stripped = line.strip()
        if not stripped.startswith("{"):
            continue
        try:
            responses.append(json.loads(stripped))
        except json.JSONDecodeError:
            continue

    assert len(responses) >= 2, f"expected >=2 JSON responses, got: {stdout_lines}"

    # first response: raw command, id should be null
    raw_resp = responses[0]
    assert raw_resp["type"] == "response"
    assert raw_resp["status"] == "ok"
    assert raw_resp["id"] is None

    # second response: JSON request, id should round-trip
    json_resp = responses[1]
    assert json_resp["type"] == "response"
    assert json_resp["status"] == "ok"
    assert json_resp["id"] == "req_1"


def test_json_envelope_rejects_malformed(griz_bin, sample_database):
    """A `{...}`-shaped line that is not valid JSON yields an error response."""
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        w.send_command('{"type":"request", "id":"bad", "cmd":')  # truncated
        time.sleep(0.5)
        stdout_lines = w.recent_stdout()

    errors = []
    for line in stdout_lines:
        stripped = line.strip()
        if not stripped.startswith("{"):
            continue
        try:
            parsed = json.loads(stripped)
        except json.JSONDecodeError:
            continue
        if parsed.get("status") == "error":
            errors.append(parsed)

    assert errors, f"expected at least one error response, got: {stdout_lines}"
    err = errors[0]
    assert err["error"]["code"] == "invalid_request"


def test_send_command_after_cleanup_fails(griz_bin, sample_database):
    w = Worker(sample_database, griz_bin=griz_bin, width=256, height=256)
    w.cleanup()
    with pytest.raises(WorkerError):
        w.send_command("state 1")


def test_command_with_newline_rejected(griz_bin, sample_database):
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        with pytest.raises(ValueError):
            w.send_command("state 1\nstate 2")
