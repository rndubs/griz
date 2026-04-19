"""Phase 1 smoke tests for the minimal Worker."""

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
        import time
        time.sleep(0.5)

    assert out.exists() and out.stat().st_size > 0


def test_send_command_after_cleanup_fails(griz_bin, sample_database):
    w = Worker(sample_database, griz_bin=griz_bin, width=256, height=256)
    w.cleanup()
    with pytest.raises(WorkerError):
        w.send_command("state 1")


def test_command_with_newline_rejected(griz_bin, sample_database):
    with Worker(sample_database, griz_bin=griz_bin, width=256, height=256) as w:
        with pytest.raises(ValueError):
            w.send_command("state 1\nstate 2")
